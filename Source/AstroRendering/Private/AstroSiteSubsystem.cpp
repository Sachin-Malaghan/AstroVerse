// See CLAUDE.md "Sun at a site".
#include "AstroSiteSubsystem.h"
#include "AstroGeodesy.h"
#include "AstroRenderingSubsystem.h"
#include "AstroSimulationSubsystem.h"
#include "AstroSiteActor.h"
#include "AstroSpaceEnvironment.h"
#include "BodyTerrain.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Math/AstroConstants.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroSite, Log, All);

namespace
{
    FAstroVector3d ToAstro(const FVector& V) { return FAstroVector3d(V.X, V.Y, V.Z); }

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdSitePaths(
        TEXT("astro.Site.Paths"), TEXT("astro.Site.Paths 0|1 - show the Sun's paths over the site"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            if (UAstroSiteSubsystem* Site = UAstroSiteSubsystem::Get(World))
            {
                Site->SetPathsVisible(Args.Num() == 0 ? !Site->ArePathsVisible() : FCString::Atoi(*Args[0]) != 0);
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdSiteReport(
        TEXT("astro.Site.Report"), TEXT("Print the site's sun data to the log"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            const UAstroSiteSubsystem* Site = UAstroSiteSubsystem::Get(World);
            if (!Site || !Site->HasSite())
            {
                return;
            }
            const FAstroSiteReport& R = Site->GetReport();
            auto Local = [&](double T) { return (T - R.LocalDayStartSim) / 3600.0; };
            UE_LOG(LogAstroSite, Display, TEXT("Site %s (%.4f, %.4f) elev %.0f m: sun az %.2f el %.2f (apparent %.2f); rise %.3f h az %.1f, noon %.3f h el %.2f, set %.3f h az %.1f, day %.2f h; 1 m stick shadow %.2f m toward %.1f"),
                *R.Name, R.LatDeg, R.LonDeg, R.ElevationM, R.Sun.AzimuthDeg, R.Sun.ElevationDeg, R.Sun.ApparentElevationDeg,
                Local(R.Today.SunriseSim), R.Today.SunriseAzimuthDeg, Local(R.Today.SolarNoonSim), R.Today.NoonElevationDeg,
                Local(R.Today.SunsetSim), R.Today.SunsetAzimuthDeg, R.Today.DayLengthHours, R.ShadowLengthPerMeter, R.ShadowAzimuthDeg);
        }));
}

bool UAstroSiteSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

UAstroSiteSubsystem* UAstroSiteSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UAstroSiteSubsystem>() : nullptr;
}

TStatId UAstroSiteSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UAstroSiteSubsystem, STATGROUP_Tickables);
}

FString UAstroSiteSubsystem::CompassName(double AzimuthDeg, bool bVastu)
{
    static const TCHAR* Points[] = { TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW") };
    // Vastu Shastra names of the eight directions (dik).
    static const TCHAR* Vastu[] = { TEXT("Uttara"), TEXT("Ishanya"), TEXT("Purva"), TEXT("Agneya"), TEXT("Dakshina"), TEXT("Nairutya"), TEXT("Paschima"), TEXT("Vayavya") };
    const int32 K = FMath::RoundToInt(FMath::Fmod(AzimuthDeg + 360.0, 360.0) / 45.0) % 8;
    return bVastu ? FString::Printf(TEXT("%s - %s"), Points[K], Vastu[K]) : FString(Points[K]);
}

void UAstroSiteSubsystem::SetSite(FName Body, double LatDeg, double LonDeg, double UtcOffsetHours, const FString& Name)
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    BodyIndex = Sim ? Sim->FindBodyIndex(Body) : INDEX_NONE;
    if (BodyIndex == INDEX_NONE)
    {
        UE_LOG(LogAstroSite, Warning, TEXT("Unknown body %s"), *Body.ToString());
        return;
    }
    const FBodyDefinition& Def = Sim->GetRegistry().Get(BodyIndex);
    const bool bGeodetic = Def.Terrain.IsValid() && Def.Terrain->UsesGeodeticLatitude();
    const FAstroVector3d Dir = AstroGeodesy::SurfaceDirection(LatDeg, LonDeg, Def.EquatorialRadiusMeters, Def.PolarRadiusMeters, bGeodetic);
    const double Height = Def.Terrain.IsValid() ? Def.Terrain->HeightAt(Dir, 0.5) : 0.0;
    const double Radius = Def.Terrain.IsValid() ? Def.Terrain->EllipsoidRadius(Dir) : Def.EquatorialRadiusMeters;
    SiteBF = Dir * (Radius + Height);
    UpBF = bGeodetic ? AstroGeodesy::GeodeticNormal(LatDeg, LonDeg) : Dir;
    EastBF = FAstroVector3d(0.0, 0.0, 1.0).Cross(UpBF).Normalized();
    NorthBF = UpBF.Cross(EastBF);

    Report = FAstroSiteReport();
    Report.bValid = true;
    Report.Body = Body;
    Report.Name = Name.IsEmpty() ? FString::Printf(TEXT("%.4f, %.4f"), LatDeg, LonDeg) : Name;
    Report.LatDeg = LatDeg;
    Report.LonDeg = LonDeg;
    Report.UtcOffsetHours = UtcOffsetHours;
    Report.ElevationM = Height;
    Report.bRealElevation = Def.Terrain.IsValid() && Def.Terrain->HasRealElevation();
    LastPathDayStart = -1e30;

    if (!Actor.IsValid())
    {
        FActorSpawnParameters Params;
        Params.Name = TEXT("AstroSite");
        Actor = GetWorld()->SpawnActor<AAstroSiteActor>(Params);
    }
    Actor->SetActorHiddenInGame(false);
    Actor->SetPathsVisible(bPathsVisible);
    Recompute();
    UE_LOG(LogAstroSite, Display, TEXT("Site: %s on %s (%.5f, %.5f), ground %.0f m%s, UTC%+.2f"),
        *Report.Name, *Body.ToString(), LatDeg, LonDeg, Height, Report.bRealElevation ? TEXT(" (real elevation)") : TEXT(""), UtcOffsetHours);
}

void UAstroSiteSubsystem::ClearSite()
{
    Report.bValid = false;
    Labels.Reset();
    if (Actor.IsValid())
    {
        Actor->SetActorHiddenInGame(true);
    }
}

void UAstroSiteSubsystem::SetPathsVisible(bool bVisible)
{
    bPathsVisible = bVisible;
    if (Actor.IsValid())
    {
        Actor->SetPathsVisible(bVisible);
    }
}

TUniquePtr<FAstroSolarGeometry> UAstroSiteSubsystem::MakeGeometry() const
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    const FBodyRegistry& Registry = Sim->GetRegistry();
    const FSolarSystemSimulation& State = Sim->GetSimulation();
    const int32 Star = Registry.GetStarIndex();
    // Moons: the Sun's direction follows the parent planet's orbit (the moon's own
    // heliocentric path wiggles; its sky's Sun doesn't care at this accuracy).
    const FBodyDefinition& Def = Registry.Get(BodyIndex);
    const int32 OrbitBody = Def.ParentIndex != INDEX_NONE && Def.ParentIndex != Star ? Def.ParentIndex : BodyIndex;
    FOrbitalState Rel;
    Rel.Position = State.GetBodyState(BodyIndex).Position - State.GetBodyState(Star).Position;
    Rel.Velocity = State.GetBodyState(OrbitBody).Velocity - State.GetBodyState(Star).Velocity;
    TUniquePtr<FAstroSolarGeometry> Geometry = MakeUnique<FAstroSolarGeometry>(Def, Rel, Registry.Get(Star).GM + Registry.Get(OrbitBody).GM, State.GetSimSeconds());
    Geometry->SetSite(Report.LatDeg, Report.LonDeg, Report.ElevationM);
    return Geometry;
}

bool UAstroSiteSubsystem::GetSiteFrame(FVector& OutGround, FVector& OutEast, FVector& OutNorth, FVector& OutUp) const
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (!Report.bValid || !Sim || !Sim->IsReady())
    {
        return false;
    }
    const FBodyDefinition& Def = Sim->GetRegistry().Get(BodyIndex);
    const FAstroMatrix3d M = Def.GetOrientationAt(Sim->GetSimulation().GetSimSeconds());
    const FAstroVector3d Center = Sim->GetSimulation().GetBodyState(BodyIndex).Position;
    OutGround = Sim->SimToEnginePosition(Center + M * SiteBF);
    OutEast = Sim->SimToEngineDirection(M * EastBF).GetSafeNormal();
    OutNorth = Sim->SimToEngineDirection(M * NorthBF).GetSafeNormal();
    OutUp = Sim->SimToEngineDirection(M * UpBF).GetSafeNormal();
    return true;
}

void UAstroSiteSubsystem::Recompute()
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (!Report.bValid || !Sim || !Sim->IsReady())
    {
        return;
    }
    const double Now = Sim->GetSimulation().GetSimSeconds();
    TUniquePtr<FAstroSolarGeometry> Geometry = MakeGeometry();
    Report.Sun = Geometry->SunAt(Now);
    const double DayStart = FAstroSolarGeometry::LocalMidnight(Now, Report.UtcOffsetHours);
    const bool bNewDay = !FMath::IsNearlyEqual(DayStart, LastPathDayStart);
    if (bNewDay)
    {
        Report.Today = Geometry->DayEvents(DayStart);
        Report.LocalDayStartSim = DayStart;
    }
    const double El = Report.Sun.ApparentElevationDeg;
    Report.ShadowLengthPerMeter = El > 0.5 ? 1.0 / FMath::Tan(FMath::DegreesToRadians(El)) : 0.0;
    Report.ShadowAzimuthDeg = FMath::Fmod(Report.Sun.AzimuthDeg + 180.0, 360.0);

    if (bNewDay)
    {
        // Paths: today, and this year's June solstice, December solstice and March equinox.
        LastPathDayStart = DayStart;
        const FDateTime Today = FDateTime(2000, 1, 1, 11, 58, 55, 816) + FTimespan::FromSeconds(Now + Report.UtcOffsetHours * 3600.0);
        const int32 Year = Today.GetYear();
        auto DayStartFor = [&](int32 Month, int32 Day)
        {
            const double Noon = (FDateTime(Year, Month, Day, 12) - FDateTime(2000, 1, 1, 11, 58, 55, 816)).GetTotalSeconds() - Report.UtcOffsetHours * 3600.0;
            return FAstroSolarGeometry::LocalMidnight(Noon, Report.UtcOffsetHours);
        };
        const double Starts[] = { DayStart, DayStartFor(6, 21), DayStartFor(12, 21), DayStartFor(3, 20) };
        static const TCHAR* Names[] = { TEXT("today"), TEXT("21 Jun"), TEXT("21 Dec"), TEXT("equinox") };
        Paths.Reset();
        HourMarks.Reset();
        TArray<TArray<FVector>> EnginePaths;
        for (int32 p = 0; p < 4; ++p)
        {
            TArray<FAstroVector3d>& Path = Paths.AddDefaulted_GetRef();
            TArray<TPair<FAstroVector3d, FString>>& Marks = HourMarks.AddDefaulted_GetRef();
            TArray<FVector>& Out = EnginePaths.AddDefaulted_GetRef();
            if (p > 0 && FMath::Abs(Starts[p] - DayStart) < 3600.0)
            {
                continue; // today is that day: its path is already drawn as today's
            }
            for (const TPair<double, FAstroSunPosition>& Sample : Geometry->DayPath(Starts[p], 5.0, true))
            {
                Path.Add(Sample.Value.DirectionENU);
                Out.Add(FVector(Sample.Value.DirectionENU.X, Sample.Value.DirectionENU.Y, Sample.Value.DirectionENU.Z));
                const int32 Minute = FMath::RoundToInt((Sample.Key - Starts[p]) / 60.0);
                // Hours on today's path (every hour) and on the others (every 3 h) - local time.
                if (Minute % 60 == 0 && (p == 0 || (Minute / 60) % 3 == 0) && Sample.Value.ElevationDeg > 0.0)
                {
                    Marks.Emplace(Sample.Value.DirectionENU, p == 0 ? FString::Printf(TEXT("%d"), Minute / 60) : FString::Printf(TEXT("%s %d"), Names[p], Minute / 60));
                }
            }
        }
        if (Actor.IsValid())
        {
            Actor->SetPaths(EnginePaths);
        }
    }

    // Place the overlay and refresh the HUD labels.
    FVector Ground, East, North, Up;
    if (Actor.IsValid() && GetSiteFrame(Ground, East, North, Up))
    {
        Actor->SetFrame(Ground, East, North, Up);
        const FAstroVector3d S = Report.Sun.DirectionENU;
        Actor->SetSun(FVector(S.X, S.Y, S.Z), Report.Sun.ElevationDeg > -0.8);
        if (const UAstroRenderingSubsystem* Rendering = UAstroRenderingSubsystem::Get(this); Rendering && Rendering->GetEnvironment())
        {
            Actor->SetExposureEV100(Rendering->GetEnvironment()->GetExposureEV100());
        }
        Labels.Reset();
        for (int32 k = 0; k < 8; ++k)
        {
            const double Az = FMath::DegreesToRadians(k * 45.0);
            const FVector ENU(FMath::Sin(Az) * 1350.0, FMath::Cos(Az) * 1350.0, 30.0);
            Labels.Emplace(ENU, CompassName(k * 45.0, true), k == 0 ? FLinearColor(1.0f, 0.45f, 0.4f) : FLinearColor(0.9f, 0.92f, 1.0f), false);
        }
        if (bPathsVisible)
        {
            const FLinearColor Colors[] = { FLinearColor(1.0f, 0.85f, 0.3f), FLinearColor(1.0f, 0.55f, 0.3f), FLinearColor(0.45f, 0.75f, 1.0f), FLinearColor(0.85f, 0.85f, 0.85f) };
            for (int32 p = 0; p < HourMarks.Num(); ++p)
            {
                for (const TPair<FAstroVector3d, FString>& Mark : HourMarks[p])
                {
                    const FVector D(Mark.Key.X, Mark.Key.Y, Mark.Key.Z);
                    Labels.Emplace(D * (AAstroSiteActor::DomeRadiusCm + 150.0), Mark.Value, Colors[p], true);
                }
            }
            if (Report.Sun.ElevationDeg > -0.8)
            {
                Labels.Emplace(FVector(S.X, S.Y, S.Z) * (AAstroSiteActor::DomeRadiusCm + 250.0), TEXT("Sun now"), FLinearColor(1.0f, 1.0f, 0.8f), true);
            }
        }
    }
}

FVector UAstroSiteSubsystem::LabelWorldPosition(const FAstroSiteLabel& Label) const
{
    return Actor.IsValid() ? (Label.bSky ? Actor->ToSky(Label.ENUCm) : Actor->ToWorld(Label.ENUCm)) : FVector::ZeroVector;
}

void UAstroSiteSubsystem::Tick(float DeltaTime)
{
    if (!Report.bValid)
    {
        return;
    }
    if (Actor.IsValid())
    {
        if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
        {
            const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
            Actor->SetViewer(Camera);
            // The overlay describes this spot's sky: shown only while you are there.
            FVector Ground, East, North, Up;
            bNearSite = GetSiteFrame(Ground, East, North, Up) && FVector::Dist(Camera, Ground) < 30.0e5; // 30 km
            Actor->SetActorHiddenInGame(!bNearSite);
        }
    }
    // Sun data at 10 Hz (cheap), the overlay follows the frame every tick.
    RecomputeTimer -= DeltaTime;
    if (RecomputeTimer <= 0.0)
    {
        RecomputeTimer = 0.1;
        Recompute();
    }
    else if (Actor.IsValid())
    {
        FVector Ground, East, North, Up;
        if (GetSiteFrame(Ground, East, North, Up))
        {
            Actor->SetFrame(Ground, East, North, Up);
        }
    }
}
