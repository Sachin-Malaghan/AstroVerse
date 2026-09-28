// See CLAUDE.md "Missions".
#include "AstroMissionSubsystem.h"
#include "AstroGeodesy.h"
#include "AstroMissionActors.h"
#include "AstroSimulationSubsystem.h"
#include "AstroTravelSubsystem.h"
#include "BodyTerrain.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Math/AstroConstants.h"
#include "TimeController.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroMission, Log, All);

namespace
{
    // Ascent profile (mission s, altitude km, downrange km): a heavy launcher's gravity turn to
    // a 200 km orbit. SECO at T+540 s with ~7.4 km/s over the ground (+ Earth's rotation).
    struct FAscentKey { double T, H, X; };
    const FAscentKey Profile[] = {
        { 0, 0, 0 }, { 20, 1.2, 0.05 }, { 60, 11, 1.5 }, { 100, 35, 10 }, { 160, 68, 55 },
        { 220, 110, 170 }, { 300, 150, 420 }, { 420, 185, 950 }, { 540, 200, 1800 } };
    constexpr double SECO = 540.0;
    constexpr double OrbitGroundSpeed = 7100.0; // m/s after SECO in Earth's rotating frame
    constexpr double CountdownSeconds = 10.0;
    constexpr double MountHeight = 7.0; // rocket base above the pad deck (launch mount)
    constexpr double CoastSeconds = 5.0;
    constexpr double RendezvousSeconds = 16.0;
    constexpr double DockedSeconds = 6.0;

    double CatmullRom(double P0, double P1, double P2, double P3, double U)
    {
        return 0.5 * ((2.0 * P1) + (-P0 + P2) * U + (2.0 * P0 - 5.0 * P1 + 4.0 * P2 - P3) * U * U + (-P0 + 3.0 * P1 - 3.0 * P2 + P3) * U * U * U);
    }

    struct FEvent { double T; const TCHAR* Text; };
    const FEvent Events[] = {
        { 0.0, TEXT("Liftoff!") },
        { 72.0, TEXT("Max-Q: maximum aerodynamic pressure") },
        { 158.0, TEXT("Main engine cutoff") },
        { 160.0, TEXT("Stage separation") },
        { 165.0, TEXT("Second stage ignition") },
        { 215.0, TEXT("Fairing separation - capsule exposed") },
        { 540.0, TEXT("Second stage cutoff - in orbit, 200 km, 7.8 km/s") },
    };

    AActor* ViewActorOf(UWorld* World)
    {
        APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
        return PC ? (PC->GetPawn() ? static_cast<AActor*>(PC->GetPawn()) : PC->GetViewTarget()) : nullptr;
    }

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdMissionLaunch(
        TEXT("astro.Mission.Launch"), TEXT("astro.Mission.Launch [Destination=Mars] [lat lon] - rocket launch, docking, and flight in the ring ship"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            if (UAstroMissionSubsystem* Mission = UAstroMissionSubsystem::Get(World))
            {
                const FName Dest = Args.Num() > 0 ? FName(*Args[0]) : FName(TEXT("Mars"));
                if (Args.Num() >= 3)
                {
                    Mission->Launch(Dest, FCString::Atod(*Args[1]), FCString::Atod(*Args[2]), TEXT("Custom site"));
                }
                else
                {
                    Mission->Launch(Dest, 13.7199, 80.2304, TEXT("Satish Dhawan Space Centre, Sriharikota"));
                }
            }
        }));

    FAutoConsoleCommandWithWorld GAstroCmdMissionAbort(TEXT("astro.Mission.Abort"), TEXT("End the mission and remove its vehicles"),
        FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
        {
            if (UAstroMissionSubsystem* Mission = UAstroMissionSubsystem::Get(World)) { Mission->Abort(); }
        }));
}

bool UAstroMissionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UAstroMissionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UAstroTravelSubsystem>();
}

UAstroMissionSubsystem* UAstroMissionSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UAstroMissionSubsystem>() : nullptr;
}

TStatId UAstroMissionSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UAstroMissionSubsystem, STATGROUP_Tickables);
}

bool UAstroMissionSubsystem::IsControllingCamera() const
{
    return Status.bActive && Status.Phase != EAstroMissionPhase::Departure && Status.Phase != EAstroMissionPhase::Parked;
}

bool UAstroMissionSubsystem::Launch(FName InDestination, double LatDeg, double LonDeg, const FString& SiteName)
{
    UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    Earth = Sim && Sim->IsReady() ? Sim->FindBodyIndex(TEXT("Earth")) : INDEX_NONE;
    if (Earth == INDEX_NONE || Sim->FindBodyIndex(InDestination) == INDEX_NONE)
    {
        return false;
    }
    Abort();
    if (UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this); Travel && Travel->IsTravelling())
    {
        Travel->FinishNow();
    }
    const FBodyDefinition& Def = Sim->GetRegistry().Get(Earth);
    const bool bGeodetic = Def.Terrain.IsValid() && Def.Terrain->UsesGeodeticLatitude();
    SiteUp = AstroGeodesy::SurfaceDirection(LatDeg, LonDeg, Def.EquatorialRadiusMeters, Def.PolarRadiusMeters, bGeodetic);
    SiteEast = FAstroVector3d(0, 0, 1).Cross(SiteUp).Normalized();
    SiteNorth = SiteUp.Cross(SiteEast);
    GroundRadius = (Def.Terrain.IsValid() ? Def.Terrain->EllipsoidRadius(SiteUp) + FMath::Max(Def.Terrain->HeightAt(SiteUp, 0.5), 0.0) : Def.EquatorialRadiusMeters) + 1.5;

    // Launch in daylight: if the Sun is low at the pad, jump to 10:00 local mean solar time.
    const FAstroVector3d SunBF = Def.GetOrientationAt(Sim->GetSimulation().GetSimSeconds()).Transposed()
        * (Sim->GetSimulation().GetBodyState(Sim->GetRegistry().GetStarIndex()).Position - Sim->GetSimulation().GetBodyState(Earth).Position).Normalized();
    if (SunBF.Dot(SiteUp) < 0.25)
    {
        if (UTimeController* Time = UTimeController::Get(this))
        {
            const FDateTime Now = Time->GetSimulatedDateTime();
            const double UtcHours = FMath::Fmod(10.0 - LonDeg / 15.0 + 48.0, 24.0);
            FDateTime Day(Now.GetYear(), Now.GetMonth(), Now.GetDay());
            Time->JumpToDateTime(Day + FTimespan::FromHours(UtcHours));
            Time->SetTimeScale(1.0);
            Time->Play();
        }
    }

    FActorSpawnParameters Params;
    Params.Name = TEXT("AstroMissionRocket");
    Rocket = GetWorld()->SpawnActor<AAstroRocketActor>(Params);
    Params.Name = TEXT("AstroMissionShip");
    Ship = GetWorld()->SpawnActor<AAstroRingShipActor>(Params);
    Ship->SetActorHiddenInGame(true);

    Destination = InDestination;
    Status = FAstroMissionStatus();
    Status.bActive = true;
    Status.Phase = EAstroMissionPhase::Countdown;
    Status.Title = FString::Printf(TEXT("Mission to %s  -  from %s"), *InDestination.ToString(), *SiteName);
    PhaseSeconds = 0.0;
    MissionTime = -CountdownSeconds;
    RealSinceLiftoff = 0.0;
    NextEvent = 0;
    bStageSeparated = bFairingSeparated = false;
    SetEvent(TEXT("Countdown - all systems go"));
    UE_LOG(LogAstroMission, Display, TEXT("Mission to %s from %.4f, %.4f"), *InDestination.ToString(), LatDeg, LonDeg);
    return true;
}

void UAstroMissionSubsystem::Abort()
{
    if (Rocket.IsValid()) { Rocket->Destroy(); }
    if (Ship.IsValid()) { Ship->Destroy(); }
    Rocket.Reset();
    Ship.Reset();
    if (UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this))
    {
        Travel->OnPathApplied.RemoveAll(this);
        Travel->OnTravelArrived.RemoveDynamic(this, &UAstroMissionSubsystem::HandleArrived);
    }
    Status = FAstroMissionStatus();
}

void UAstroMissionSubsystem::SetEvent(const FString& Text)
{
    Status.EventText = Text;
    EventTimer = 6.0;
}

FAstroVector3d UAstroMissionSubsystem::TrajectoryBF(double T, double* OutAltitude, double* OutDownrange) const
{
    double H = 0.0, X = 0.0;
    if (T <= 0.0)
    {
        H = X = 0.0;
    }
    else if (T >= SECO)
    {
        H = Profile[UE_ARRAY_COUNT(Profile) - 1].H * 1000.0;
        X = Profile[UE_ARRAY_COUNT(Profile) - 1].X * 1000.0 + OrbitGroundSpeed * (T - SECO);
    }
    else
    {
        const int32 N = UE_ARRAY_COUNT(Profile);
        int32 i = 0;
        while (i < N - 2 && Profile[i + 1].T < T) { ++i; }
        const FAscentKey& A = Profile[FMath::Max(i - 1, 0)];
        const FAscentKey& B = Profile[i];
        const FAscentKey& C = Profile[i + 1];
        const FAscentKey& D = Profile[FMath::Min(i + 2, N - 1)];
        const double U = (T - B.T) / (C.T - B.T);
        H = FMath::Max(0.0, CatmullRom(A.H, B.H, C.H, D.H, U)) * 1000.0;
        X = FMath::Max(0.0, CatmullRom(A.X, B.X, C.X, D.X, U)) * 1000.0;
    }
    if (OutAltitude) { *OutAltitude = H; }
    if (OutDownrange) { *OutDownrange = X; }
    // Downrange along the great circle heading east from the pad.
    const double Theta = X / GroundRadius;
    return (SiteUp * FMath::Cos(Theta) + SiteEast * FMath::Sin(Theta)) * (GroundRadius + MountHeight + H);
}

FAstroVector3d UAstroMissionSubsystem::BodyFixedToSim(const UAstroSimulationSubsystem* Sim, const FAstroVector3d& BF) const
{
    const FBodyDefinition& Def = Sim->GetRegistry().Get(Earth);
    return Sim->GetSimulation().GetBodyState(Earth).Position + Def.GetOrientationAt(Sim->GetSimulation().GetSimSeconds()) * BF;
}

FAstroVector3d UAstroMissionSubsystem::BodyFixedDirToSim(const UAstroSimulationSubsystem* Sim, const FAstroVector3d& Dir) const
{
    return Sim->GetRegistry().Get(Earth).GetOrientationAt(Sim->GetSimulation().GetSimSeconds()) * Dir;
}

FVector UAstroMissionSubsystem::ToEngine(const UAstroSimulationSubsystem* Sim, const FAstroVector3d& BF) const
{
    return Sim->SimToEnginePosition(BodyFixedToSim(Sim, BF));
}

FQuat UAstroMissionSubsystem::EngineRotation(const UAstroSimulationSubsystem* Sim, const FAstroVector3d& AxisBF, const FAstroVector3d& UpHintBF, bool bAxisIsZ) const
{
    const FVector Axis = Sim->SimToEngineDirection(BodyFixedDirToSim(Sim, AxisBF)).GetSafeNormal();
    const FVector Hint = Sim->SimToEngineDirection(BodyFixedDirToSim(Sim, UpHintBF)).GetSafeNormal();
    return bAxisIsZ ? FRotationMatrix::MakeFromZX(Axis, Hint).ToQuat() : FRotationMatrix::MakeFromXZ(Axis, Hint).ToQuat();
}

void UAstroMissionSubsystem::PlaceCamera(UAstroSimulationSubsystem* Sim, const FAstroVector3d& CamBF, const FAstroVector3d& LookBF)
{
    // The render origin sits at the camera (Earth's co-rotating frame); the viewer at (0,0,0).
    Sim->SetRenderOriginBodyFixed(Earth, CamBF);
    if (AActor* View = ViewActorOf(GetWorld()))
    {
        View->SetActorLocation(FVector::ZeroVector);
        const FVector Dir = Sim->SimToEngineDirection(BodyFixedDirToSim(Sim, (LookBF - CamBF).Normalized())).GetSafeNormal();
        View->SetActorRotation(FRotationMatrix::MakeFromXZ(Dir, FVector::UpVector).ToQuat());
    }
}

double UAstroMissionSubsystem::ExposureWhite(const UAstroSimulationSubsystem* Sim) const
{
    // Same meter as the space environment: EV100 = log2(E / 2.5), capped at 23.5.
    const FBodyDefinition& Star = Sim->GetRegistry().Get(Sim->GetRegistry().GetStarIndex());
    const double D = (Sim->GetSimulation().GetBodyState(Sim->GetRegistry().GetStarIndex()).Position - Sim->GetRenderOrigin()).Length();
    const double Lux = Star.LuminosityWatts / (4.0 * AstroConstants::Pi * D * D) * 93.0;
    return 1.2 * FMath::Pow(2.0, FMath::Min(FMath::Log2(FMath::Max(Lux, 1e-3) / 2.5), 23.5));
}

void UAstroMissionSubsystem::Tick(float DeltaTime)
{
    if (!Status.bActive || !Rocket.IsValid() || !Ship.IsValid())
    {
        return;
    }
    UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (!Sim || !Sim->IsReady())
    {
        return;
    }
    PhaseSeconds += DeltaTime;
    EventTimer -= DeltaTime;
    const double White = ExposureWhite(Sim);

    switch (Status.Phase)
    {
    case EAstroMissionPhase::Countdown:
        MissionTime = -CountdownSeconds + PhaseSeconds;
        Status.PhaseText = FString::Printf(TEXT("T-%d"), FMath::CeilToInt(-MissionTime));
        if (MissionTime >= 0.0)
        {
            Status.Phase = EAstroMissionPhase::Ascent;
            PhaseSeconds = 0.0;
            MissionTime = 0.0;
        }
        break;
    case EAstroMissionPhase::Ascent:
    {
        RealSinceLiftoff += DeltaTime;
        // 1x for the liftoff, then ramp to ~9x so the climb to orbit takes about a minute.
        const double Rate = 1.0 + 8.0 * FMath::SmoothStep(10.0, 18.0, RealSinceLiftoff);
        MissionTime = FMath::Min(MissionTime + DeltaTime * Rate, SECO);
        while (NextEvent < static_cast<int32>(UE_ARRAY_COUNT(Events)) && MissionTime >= Events[NextEvent].T)
        {
            SetEvent(Events[NextEvent].Text);
            ++NextEvent;
        }
        Status.PhaseText = MissionTime < 158.0 ? TEXT("Ascent - first stage") : MissionTime < 165.0 ? TEXT("Staging") : TEXT("Ascent - second stage");
        if (MissionTime >= SECO)
        {
            Status.Phase = EAstroMissionPhase::Coast;
            PhaseSeconds = 0.0;
        }
        break;
    }
    case EAstroMissionPhase::Coast:
        MissionTime += DeltaTime * 3.0;
        Status.PhaseText = FString::Printf(TEXT("In orbit - %s ahead"), ShipName);
        if (PhaseSeconds >= CoastSeconds)
        {
            Status.Phase = EAstroMissionPhase::Rendezvous;
            PhaseSeconds = 0.0;
            SetEvent(FString::Printf(TEXT("Rendezvous with %s"), ShipName));
        }
        break;
    case EAstroMissionPhase::Rendezvous:
        MissionTime += DeltaTime * 3.0;
        Status.PhaseText = TEXT("Rendezvous and docking");
        if (PhaseSeconds >= RendezvousSeconds)
        {
            Status.Phase = EAstroMissionPhase::Docked;
            PhaseSeconds = 0.0;
            SetEvent(TEXT("Docked - crew transferring to the ship"));
        }
        break;
    case EAstroMissionPhase::Docked:
        MissionTime += DeltaTime * 3.0;
        Status.PhaseText = FString::Printf(TEXT("Crew aboard %s"), ShipName);
        if (PhaseSeconds >= DockedSeconds)
        {
            BeginDeparture();
            return;
        }
        break;
    case EAstroMissionPhase::Departure:
        Status.PhaseText = FString::Printf(TEXT("%s en route to %s"), ShipName, *Destination.ToString());
        return; // the travel system flies; the ship rides along (OnPathApplied)
    case EAstroMissionPhase::Parked:
    {
        // Parked where it arrived; follows the destination along its orbit.
        const int32 Body = Sim->FindBodyIndex(Destination);
        const FAstroVector3d Pos = Sim->GetSimulation().GetBodyState(Body).Position + ParkedOffset;
        const FVector X = Sim->SimToEngineDirection(ParkedBasis.GetColumn(0)), Z = Sim->SimToEngineDirection(ParkedBasis.GetColumn(2));
        Ship->SetShipTransform(Sim->SimToEnginePosition(Pos), FRotationMatrix::MakeFromXZ(X, Z).ToQuat());
        Ship->SetEngines(0.0f, White);
        Status.PhaseText = FString::Printf(TEXT("%s in orbit at %s"), ShipName, *Destination.ToString());
        if (EventTimer < -8.0)
        {
            Status.bActive = false; // caption goes; the ship stays until the next mission
        }
        return;
    }
    default:
        return;
    }

    // --- vehicles and camera (launch through docking), all in Earth's body-fixed frame.
    double Altitude = 0.0, Downrange = 0.0;
    const double T = FMath::Max(MissionTime, 0.0);
    const FAstroVector3d Base = TrajectoryBF(T, &Altitude, &Downrange);
    const FAstroVector3d Ahead = TrajectoryBF(T + 1.0);
    FAstroVector3d Axis = MissionTime < 3.0 ? SiteUp : (Ahead - Base).Normalized();
    if (Axis.Length() < 0.5) { Axis = SiteUp; }
    const FAstroVector3d LocalUp = Base.Normalized();
    const FAstroVector3d Side = Axis.Cross(LocalUp).Length() > 1e-3 ? Axis.Cross(LocalUp).Normalized() : SiteNorth;
    Status.MissionSeconds = MissionTime;
    Status.AltitudeKm = Altitude / 1000.0;
    Status.DownrangeKm = Downrange / 1000.0;
    // Inertial speed: over-the-ground speed plus Earth's rotation at the site.
    const double Ground = (Ahead - Base).Length();
    Status.SpeedKmS = (Ground + 465.1 * FMath::Sqrt(1.0 - FMath::Square(SiteUp.Z)) * FMath::SmoothStep(0.0, SECO, T)) / 1000.0;

    // Ring ship waiting ahead in the same orbit; closes in during the rendezvous.
    const double Nose = 62.0;
    double Gap = 2500.0;
    if (Status.Phase == EAstroMissionPhase::Rendezvous)
    {
        Gap = 0.3 + 2500.0 * FMath::Square(1.0 - FMath::SmoothStep(0.0, 1.0, PhaseSeconds / RendezvousSeconds));
    }
    else if (Status.Phase == EAstroMissionPhase::Docked)
    {
        Gap = 0.3;
    }
    const FAstroVector3d ShipCentre = Base + Axis * (Nose + Gap + AstroRingShipDockOffset());
    // Camera first: the actors below are placed in the frame it sets up (placing them
    // before moving the origin left them hundreds of metres behind at ascent speeds).
    const FAstroVector3d RocketMid = Base + Axis * 32.0;
    FAstroVector3d Cam, Look;
    const FAstroVector3d GroundCam = SiteUp * (GroundRadius + 20.0) + SiteNorth * 240.0 - SiteEast * 80.0;
    if (Status.Phase == EAstroMissionPhase::Countdown)
    {
        Cam = GroundCam;
        Look = SiteUp * (GroundRadius + 32.0);
    }
    else if (Status.Phase == EAstroMissionPhase::Ascent)
    {
        const double PullBack = bStageSeparated && MissionTime < 200.0 ? 110.0 : 55.0;
        const FAstroVector3d Chase = RocketMid - Axis * PullBack + Side * 140.0 + LocalUp * 25.0;
        const double Blend = FMath::SmoothStep(8.0, 16.0, RealSinceLiftoff);
        // Ground camera first (the rocket clears the tower), then a chase camera.
        Cam = GroundCam + (Chase - GroundCam) * Blend;
        Look = RocketMid + Axis * (20.0 * Blend);
    }
    else if (Status.Phase == EAstroMissionPhase::Coast || Status.Phase == EAstroMissionPhase::Rendezvous)
    {
        // Ride just behind the capsule, looking ahead at the ship as it grows.
        Cam = RocketMid - Axis * 70.0 + Side * 28.0 + LocalUp * 14.0;
        Look = ShipCentre;
    }
    else // Docked: a slow orbit around the joined vehicles
    {
        const double A = PhaseSeconds * 0.35;
        Cam = ShipCentre + (Side * FMath::Cos(A) + LocalUp * 0.35 + Axis * FMath::Sin(A) * 0.6) * 150.0;
        Look = ShipCentre - Axis * 20.0;
    }
    PlaceCamera(Sim, Cam, Look);
    const FQuat RocketRot = EngineRotation(Sim, Axis, Side, true);
    Rocket->SetPadTransform(ToEngine(Sim, SiteUp * GroundRadius), EngineRotation(Sim, SiteUp, SiteNorth, true));
    Rocket->SetPadVisible(Altitude < 60000.0);
    Rocket->SetRocketTransform(ToEngine(Sim, Base), RocketRot);

    // Staging: the spent first stage drops behind (the upper stage keeps accelerating away).
    if (MissionTime >= 160.0 && !bStageSeparated)
    {
        bStageSeparated = true;
        Rocket->SeparateStage1();
        SeparationTime = MissionTime;
        SeparationBF = Base;
        SeparationVelBF = Ahead - Base;
    }
    if (bStageSeparated)
    {
        const double Dt = MissionTime - SeparationTime;
        const FAstroVector3d Pos = SeparationBF + SeparationVelBF * Dt - Axis * (0.5 * 12.0 * Dt * Dt) - LocalUp * (0.5 * 9.0 * Dt * Dt);
        const FQuat Tumble = FQuat(Sim->SimToEngineDirection(BodyFixedDirToSim(Sim, Side)), FMath::DegreesToRadians(Dt * 4.0));
        Rocket->SetStage1Transform(ToEngine(Sim, Pos), Tumble * RocketRot);
    }
    if (MissionTime >= 215.0 && !bFairingSeparated)
    {
        bFairingSeparated = true;
        Rocket->SeparateFairing();
        FairingTime = MissionTime;
        FairingBF = Base;
        FairingVelBF = Ahead - Base;
    }
    if (bFairingSeparated)
    {
        const double Dt = MissionTime - FairingTime;
        const FAstroVector3d Pos = FairingBF + FairingVelBF * Dt - Axis * (0.5 * 15.0 * Dt * Dt) + Side * (3.0 * Dt);
        const FQuat Tumble = FQuat(Sim->SimToEngineDirection(BodyFixedDirToSim(Sim, Axis.Cross(Side))), FMath::DegreesToRadians(Dt * 25.0));
        Rocket->SetFairingTransform(ToEngine(Sim, Pos), Tumble * RocketRot);
    }
    const bool bS1 = Status.Phase == EAstroMissionPhase::Ascent && MissionTime >= 0.0 && MissionTime < 158.0;
    const bool bS2 = Status.Phase == EAstroMissionPhase::Ascent && MissionTime >= 165.0 && MissionTime < SECO;
    Rocket->SetThrust(bS1 ? 1.0f : 0.0f, bS2 ? 1.0f : 0.0f, static_cast<float>(FMath::Clamp(Altitude / 40000.0, 0.0, 1.0)), White);

    Ship->SetActorHiddenInGame(Status.Phase < EAstroMissionPhase::Coast);
    Ship->SetShipTransform(ToEngine(Sim, ShipCentre), EngineRotation(Sim, Axis * -1.0, LocalUp, false));
    Ship->SetEngines(0.0f, White);
    Rocket->SetUpperVisible(true);

}

double UAstroMissionSubsystem::AstroRingShipDockOffset()
{
    return AAstroRingShipActor::DockingPortX;
}

void UAstroMissionSubsystem::BeginDeparture()
{
    UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this);
    if (!Travel)
    {
        Abort();
        return;
    }
    if (Rocket.IsValid())
    {
        Rocket->SetActorHiddenInGame(true); // the capsule stays docked in the ship's port
    }
    Travel->OnPathApplied.AddUObject(this, &UAstroMissionSubsystem::UpdateShipRidingAlong);
    Travel->OnTravelArrived.AddUniqueDynamic(this, &UAstroMissionSubsystem::HandleArrived);
    Status.Phase = EAstroMissionPhase::Departure;
    PhaseSeconds = 0.0;
    SetEvent(FString::Printf(TEXT("Main engines - %s departs for %s"), ShipName, *Destination.ToString()));
    if (!Travel->BeginTravelWithStyle(Destination, EAstroTravelStyle::RealFlight))
    {
        Abort();
    }
}

void UAstroMissionSubsystem::UpdateShipRidingAlong()
{
    // Chase view: the ship just ahead of and below the camera, pointing where the camera looks.
    UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    AActor* View = ViewActorOf(GetWorld());
    if (!Ship.IsValid() || !View || !Sim)
    {
        return;
    }
    const FQuat Q = View->GetActorQuat();
    const FVector Pos = View->GetActorLocation() + Q.RotateVector(FVector(190.0, 0.0, -20.0) * 100.0);
    Ship->SetActorHiddenInGame(false);
    Ship->SetShipTransform(Pos, Q);
    const UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this);
    const double P = Travel ? Travel->GetProgress() : 0.0;
    Ship->SetEngines(static_cast<float>(FMath::Clamp(P * 12.0, 0.0, 1.0) * FMath::Clamp((0.92 - P) * 12.0, 0.0, 1.0)), ExposureWhite(Sim));
    Status.PhaseText = FString::Printf(TEXT("%s en route to %s"), ShipName, *Destination.ToString());
    Status.SpeedKmS = Travel ? Travel->GetSpeedMetersPerSecond() / 1000.0 : 0.0;
}

void UAstroMissionSubsystem::HandleArrived(FName BodyID)
{
    UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this);
    if (Travel)
    {
        Travel->OnPathApplied.RemoveAll(this);
        Travel->OnTravelArrived.RemoveDynamic(this, &UAstroMissionSubsystem::HandleArrived);
    }
    UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (!Sim || !Ship.IsValid())
    {
        return;
    }
    // Freeze the ship's arrival pose relative to the destination.
    const int32 Body = Sim->FindBodyIndex(Destination);
    const FAstroVector3d ShipSim = Sim->EngineToSimPosition(Ship->GetActorLocation());
    ParkedOffset = ShipSim - Sim->GetSimulation().GetBodyState(Body).Position;
    ParkedBasis = FAstroMatrix3d::FromColumns(Sim->EngineToSimDirection(Ship->GetActorForwardVector()),
        Sim->EngineToSimDirection(Ship->GetActorRightVector()), Sim->EngineToSimDirection(Ship->GetActorUpVector()));
    Status.Phase = EAstroMissionPhase::Parked;
    SetEvent(FString::Printf(TEXT("%s has arrived at %s"), ShipName, *Destination.ToString()));
}
