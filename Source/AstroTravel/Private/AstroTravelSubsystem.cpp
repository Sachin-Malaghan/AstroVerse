// See CLAUDE.md Phase 9.
#include "AstroTravelSubsystem.h"
#include "AstroSimulationSubsystem.h"
#include "AstroWarpEffects.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Math/AstroConstants.h"
#include "StereoRendering.h"
#include "TimeController.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroTravel, Log, All);

namespace
{
    AActor* ViewActorOf(UWorld* World)
    {
        APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
        return PC ? (PC->GetPawn() ? static_cast<AActor*>(PC->GetPawn()) : PC->GetViewTarget()) : nullptr;
    }

    FAstroVector3d Rotate(const FAstroVector3d& V, const FAstroVector3d& Axis, double Angle)
    {
        const double C = FMath::Cos(Angle), S = FMath::Sin(Angle);
        return V * C + Axis.Cross(V) * S + Axis * (Axis.Dot(V) * (1.0 - C));
    }

    FAstroVector3d Slerp(const FAstroVector3d& A, const FAstroVector3d& B, double T)
    {
        const double Dot = FMath::Clamp(A.Dot(B), -1.0, 1.0);
        const double Omega = FMath::Acos(Dot);
        if (Omega < 1e-6)
        {
            return A;
        }
        if (Omega > UE_PI - 1e-3)
        {
            // Opposite directions: swing through a perpendicular.
            FAstroVector3d Perp = A.Cross(FAstroVector3d(0, 0, 1));
            Perp = Perp.LengthSquared() > 1e-12 ? Perp.Normalized() : FAstroVector3d(1, 0, 0);
            return Rotate(A, Perp, UE_PI * T);
        }
        const double S = FMath::Sin(Omega);
        return (A * (FMath::Sin((1.0 - T) * Omega) / S) + B * (FMath::Sin(T * Omega) / S)).Normalized();
    }

    double SmootherStep(double X)
    {
        X = FMath::Clamp(X, 0.0, 1.0);
        return X * X * X * (X * (X * 6.0 - 15.0) + 10.0);
    }

    bool IsStereo()
    {
        return GEngine && GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabled();
    }
}

bool UAstroTravelSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

UAstroTravelSubsystem* UAstroTravelSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UAstroTravelSubsystem>() : nullptr;
}

TStatId UAstroTravelSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UAstroTravelSubsystem, STATGROUP_Tickables);
}

bool UAstroTravelSubsystem::BeginTravel(FName BodyID, bool bForceCinematic)
{
    UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    AActor* View = ViewActorOf(GetWorld());
    const int32 Body = Sim && Sim->IsReady() ? Sim->FindBodyIndex(BodyID) : INDEX_NONE;
    if (bTravelling || Body == INDEX_NONE || !View)
    {
        return false;
    }
    const UAstroTravelSettings* Settings = UAstroTravelSettings::Get();
    const FBodyRegistry& Registry = Sim->GetRegistry();
    const FBodyDefinition& Def = Registry.Get(Body);
    const FAstroVector3d BodyPos = Sim->GetSimulation().GetBodyState(Body).Position;

    StartOffset = Sim->EngineToSimPosition(View->GetActorLocation()) - BodyPos;
    if (StartOffset.LengthSquared() < 1.0)
    {
        StartOffset = FAstroVector3d(Def.EquatorialRadiusMeters * 10.0, 0.0, 0.0);
    }

    // Arrive on the sunlit side: 40 deg phase, 10 deg above the body's orbital plane.
    const bool bStar = Def.BodyType == EAstroBodyType::Star;
    const double ArrivalDistance = Def.EquatorialRadiusMeters * (bStar ? Settings->SunArrivalRadii : Settings->ArrivalRadii);
    FAstroVector3d ArrivalDir = StartOffset.Normalized();
    if (!bStar)
    {
        const FAstroVector3d ToSun = (Sim->GetSimulation().GetBodyState(Registry.GetStarIndex()).Position - BodyPos).Normalized();
        const FAstroVector3d Up(0.0, 0.0, 1.0);
        ArrivalDir = Rotate(ToSun, Up, FMath::DegreesToRadians(40.0));
        ArrivalDir = Rotate(ArrivalDir, ArrivalDir.Cross(Up).Normalized(), FMath::DegreesToRadians(-10.0)).Normalized();
    }
    ActiveStyle = bForceCinematic ? EAstroTravelStyle::CinematicWarp : Settings->Style;
    if (ActiveStyle == EAstroTravelStyle::RealFlight)
    {
        // Straight in: arrive on the line of approach (a last-second swing would be a jump cut).
        ArrivalDir = StartOffset.Normalized();
    }
    ArrivalOffset = ArrivalDir * ArrivalDistance;
    StartAbsolute = BodyPos + StartOffset;
    bHasLastPosition = false;
    SpeedMps = 0.0;
    RemainingMeters = StartOffset.Length();

    const double Decades = FMath::Max(0.0, FMath::LogX(10.0, StartOffset.Length() / ArrivalDistance));
    Duration = FMath::Clamp(Settings->BaseSeconds + Settings->SecondsPerDecade * Decades, Settings->BaseSeconds, Settings->MaxSeconds);
    if (ActiveStyle == EAstroTravelStyle::RealFlight)
    {
        Duration *= Settings->RealFlightTimeScale;
    }
    Elapsed = 0.0;
    Progress = 0.0;
    StartRotation = View->GetActorQuat();
    Destination = Body;
    DestinationID = BodyID;
    PilotThrottle = 0.0f;
    PilotSteer = FVector2D::ZeroVector;
    TunnelOffset = FVector2D::ZeroVector;

    if (UCameraComponent* Camera = View->FindComponentByClass<UCameraComponent>())
    {
        BaseFOV = Camera->FieldOfView;
    }

    bPausedClockForTravel = false;
    if (Settings->Clock == EAstroClockDuringTravel::Pause)
    {
        if (UTimeController* Time = UTimeController::Get(this); Time && !Time->IsPaused())
        {
            Time->Pause();
            bPausedClockForTravel = true;
        }
    }

    if (!Effects.IsValid())
    {
        Effects = GetWorld()->SpawnActor<AAstroWarpEffects>();
    }
    if (Effects.IsValid() && ActiveStyle != EAstroTravelStyle::RealFlight)
    {
        Effects->Begin(View, ActiveStyle == EAstroTravelStyle::PilotedShip);
    }

    bTravelling = true;
    UE_LOG(LogAstroTravel, Display, TEXT("Travel to %s (%s, clock %s): %.1f s"), *BodyID.ToString(),
        ActiveStyle == EAstroTravelStyle::PilotedShip ? TEXT("piloted") : ActiveStyle == EAstroTravelStyle::RealFlight ? TEXT("real flight") : TEXT("cinematic"),
        Settings->Clock == EAstroClockDuringTravel::Pause ? TEXT("paused") : TEXT("running"), Duration);
    OnTravelStarted.Broadcast(BodyID);
    return true;
}

void UAstroTravelSubsystem::SetPilotInput(float Throttle, const FVector2D& Steer)
{
    PilotThrottle = FMath::Clamp(Throttle, -1.0f, 1.0f);
    PilotSteer = Steer;
}

double UAstroTravelSubsystem::GetWarpIntensity() const
{
    return bTravelling ? FMath::Pow(FMath::Sin(UE_PI * Progress), 0.7) : 0.0;
}

void UAstroTravelSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bTravelling)
    {
        return;
    }
    Elapsed += DeltaTime;
    if (ActiveStyle == EAstroTravelStyle::PilotedShip)
    {
        // Throttle trims the pace between 0.3x and 2x; the path still ends at the destination.
        Progress = FMath::Min(1.0, Progress + DeltaTime / Duration * (1.0 + 0.9 * PilotThrottle + 0.1));
        TunnelOffset = (TunnelOffset + PilotSteer * 0.02).ClampAxes(-1.0, 1.0) * FMath::Exp(-0.8 * DeltaTime);
        PilotSteer = FVector2D::ZeroVector;
    }
    else
    {
        Progress = FMath::Min(1.0, Elapsed / Duration);
    }

    ApplyPathPoint(ActiveStyle == EAstroTravelStyle::RealFlight ? Progress : SmootherStep(Progress));
    if (Progress >= 1.0)
    {
        Arrive();
    }
}

void UAstroTravelSubsystem::ApplyPathPoint(double S)
{
    UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    AActor* View = ViewActorOf(GetWorld());
    if (!Sim || !View)
    {
        return;
    }
    if (ActiveStyle == EAstroTravelStyle::RealFlight)
    {
        ApplyRealFlight(Sim, View, S);
        return;
    }
    // Geometric (log-space) distance and a delayed swing onto the arrival direction.
    const double D0 = StartOffset.Length(), D1 = ArrivalOffset.Length();
    const double Distance = FMath::Exp(FMath::Lerp(FMath::Loge(D0), FMath::Loge(D1), S));
    const FAstroVector3d Dir = Slerp(StartOffset / D0, ArrivalOffset / D1, FMath::SmoothStep(0.15, 0.9, S));
    const FAstroVector3d Offset = Dir * Distance;

    // The render origin rides the path exactly; the viewer sits at engine (0,0,0).
    Sim->SetRenderOriginAnchor(Destination, Offset);
    View->SetActorLocation(FVector::ZeroVector);

    // Turn to face the destination over the first quarter, keeping ecliptic north up.
    const FVector ToBody = Sim->SimToEngineDirection((Dir * -1.0));
    FQuat Facing = FRotationMatrix::MakeFromXZ(ToBody, FVector::UpVector).ToQuat();
    if (ActiveStyle == EAstroTravelStyle::PilotedShip)
    {
        // Steering banks and yaws the ship inside the tunnel.
        Facing = Facing * FQuat(FRotator(TunnelOffset.Y * 6.0, TunnelOffset.X * 6.0, -TunnelOffset.X * 20.0));
    }
    View->SetActorRotation(FQuat::Slerp(StartRotation, Facing, FMath::SmoothStep(0.0, 0.25, S)));

    const double Warp = GetWarpIntensity();
    if (!IsStereo())
    {
        if (UCameraComponent* Camera = View->FindComponentByClass<UCameraComponent>())
        {
            Camera->SetFieldOfView(BaseFOV + 30.0f * static_cast<float>(Warp));
        }
    }
    if (Effects.IsValid())
    {
        Effects->Update(static_cast<float>(Warp), static_cast<float>(Distance), TunnelOffset);
    }
}

void UAstroTravelSubsystem::ApplyRealFlight(UAstroSimulationSubsystem* Sim, AActor* View, double S)
{
    // Straight line from the (fixed) departure point to the arrival point beside the (moving)
    // destination. Distance covered grows exponentially from the start and the distance left
    // shrinks exponentially into the arrival, meeting mid-way: gentle departure, a very fast
    // cruise, gentle arrival - every decade of distance takes the same screen time.
    const FAstroVector3d BodyPos = Sim->GetSimulation().GetBodyState(Destination).Position;
    const FAstroVector3d Target = BodyPos + ArrivalOffset;
    const FAstroVector3d Line = Target - StartAbsolute;
    const double Length = FMath::Max(Line.Length(), 1.0);
    const double Near = FMath::Clamp(ArrivalOffset.Length() * 0.5, 1.0, Length * 0.25); // the scale of the ends
    const double Half = Length * 0.5;
    double Travelled;
    if (S < 0.5)
    {
        Travelled = FMath::Exp(FMath::Lerp(FMath::Loge(Near), FMath::Loge(Half), S / 0.5)) - Near * (1.0 - S / 0.5);
    }
    else
    {
        Travelled = Length - (FMath::Exp(FMath::Lerp(FMath::Loge(Half), FMath::Loge(Near), (S - 0.5) / 0.5)) - Near * ((S - 0.5) / 0.5));
    }
    Travelled = FMath::Clamp(Travelled, 0.0, Length);
    if (S >= 1.0)
    {
        Travelled = Length;
    }
    const FAstroVector3d Position = StartAbsolute + Line * (Travelled / Length);
    RemainingMeters = (BodyPos - Position).Length();

    const double Dt = FMath::Max(static_cast<double>(GetWorld()->GetDeltaSeconds()), 1e-4);
    SpeedMps = bHasLastPosition ? (Position - LastPosition).Length() / Dt : 0.0;
    LastPosition = Position;
    bHasLastPosition = true;

    // The origin rides the path; the viewer sits at engine (0,0,0) looking at the destination.
    Sim->SetRenderOriginAnchor(Destination, Position - BodyPos);
    View->SetActorLocation(FVector::ZeroVector);
    const FVector ToBody = Sim->SimToEngineDirection((BodyPos - Position).Normalized());
    const FQuat Facing = FRotationMatrix::MakeFromXZ(ToBody, FVector::UpVector).ToQuat();
    View->SetActorRotation(FQuat::Slerp(StartRotation, Facing, FMath::SmoothStep(0.0, 0.12, S)));
}

void UAstroTravelSubsystem::FinishNow()
{
    if (bTravelling)
    {
        Progress = 1.0;
        ApplyPathPoint(1.0);
        Arrive();
    }
}

void UAstroTravelSubsystem::Arrive()
{
    bTravelling = false;
    if (AActor* View = ViewActorOf(GetWorld()))
    {
        if (UCameraComponent* Camera = View->FindComponentByClass<UCameraComponent>(); Camera && !IsStereo())
        {
            Camera->SetFieldOfView(BaseFOV);
        }
    }
    if (Effects.IsValid())
    {
        Effects->End();
    }
    if (bPausedClockForTravel)
    {
        if (UTimeController* Time = UTimeController::Get(this))
        {
            Time->Play();
        }
        bPausedClockForTravel = false;
    }
    UE_LOG(LogAstroTravel, Display, TEXT("Arrived at %s"), *DestinationID.ToString());
    OnTravelArrived.Broadcast(DestinationID);
}

namespace
{
    FAutoConsoleCommandWithWorldAndArgs GAstroCmdTravelTo(
        TEXT("astro.Travel.To"), TEXT("astro.Travel.To <BodyID>"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            if (UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(World); Travel && Args.Num() > 0)
            {
                Travel->BeginTravel(FName(*Args[0]));
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdTravelStyle(
        TEXT("astro.Travel.Style"), TEXT("astro.Travel.Style <Cinematic|Piloted> (saved per user)"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
        {
            if (Args.Num() > 0)
            {
                UAstroTravelSettings* Settings = UAstroTravelSettings::Get();
                Settings->Style = Args[0].StartsWith(TEXT("P"), ESearchCase::IgnoreCase) ? EAstroTravelStyle::PilotedShip
                                : Args[0].StartsWith(TEXT("R"), ESearchCase::IgnoreCase) ? EAstroTravelStyle::RealFlight
                                : EAstroTravelStyle::CinematicWarp;
                Settings->Save();
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdTravelClock(
        TEXT("astro.Travel.Clock"), TEXT("astro.Travel.Clock <Pause|Run> (saved per user)"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
        {
            if (Args.Num() > 0)
            {
                UAstroTravelSettings* Settings = UAstroTravelSettings::Get();
                Settings->Clock = Args[0].StartsWith(TEXT("P"), ESearchCase::IgnoreCase) ? EAstroClockDuringTravel::Pause : EAstroClockDuringTravel::KeepRunning;
                Settings->Save();
            }
        }));
}
