// See CLAUDE.md Phase 2.
#include "AstroSimulationSubsystem.h"
#include "AstroBodiesSettings.h"
#include "AstroBody.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Math/AstroConstants.h"
#include "Math/ScaledSpace.h"
#include "Misc/Paths.h"
#include "Moon.h"
#include "Planet.h"
#include "Star.h"
#include "TimeController.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroSim, Log, All);

bool UAstroSimulationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

UAstroSimulationSubsystem* UAstroSimulationSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UAstroSimulationSubsystem>() : nullptr;
}

void UAstroSimulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    const UAstroBodiesSettings* Settings = GetDefault<UAstroBodiesSettings>();
    LinearRenderLimitMeters = Settings->LinearRenderLimitMeters;
    BaseStepSeconds = Settings->BaseStepSeconds;
    MaxStepSeconds = FMath::Max(Settings->MaxStepSeconds, BaseStepSeconds);
    StepsPerRealSecondBudget = Settings->StepsPerRealSecondBudget;

    bool bLoaded;
    if (!Settings->PlanetTable.IsNull())
    {
        bLoaded = Registry.LoadFromDataTables(Settings->PlanetTable.LoadSynchronous(), Settings->MoonTable.LoadSynchronous(), LoadError);
    }
    else
    {
        bLoaded = Registry.LoadFromCSVDirectory(FPaths::Combine(FPaths::ProjectContentDir(), Settings->CSVFallbackDirectory), LoadError);
    }
    if (!bLoaded)
    {
        UE_LOG(LogAstroSim, Error, TEXT("Body data failed to load: %s"), *LoadError);
        return;
    }

    TimeController = UTimeController::Get(&InWorld);
    if (!TimeController.IsValid())
    {
        LoadError = TEXT("No UTimeController in this world.");
        UE_LOG(LogAstroSim, Error, TEXT("%s"), *LoadError);
        return;
    }

    Simulation.SetMaxStepsPerAdvance(Settings->MaxStepsPerFrame);
    const double StartTime = TimeController->GetSimulatedEpoch();
    const double StartWall = FPlatformTime::Seconds();
    Simulation.Initialize(Registry, StartTime, BaseStepSeconds);
    UE_LOG(LogAstroSim, Log, TEXT("Solar system ready: %d bodies, integrated J2000 -> %s in %.1f ms."),
        Registry.Num(), *TimeController->GetSimulatedDateTime().ToString(), (FPlatformTime::Seconds() - StartWall) * 1000.0);

    TimeHandle = TimeController->OnSimTimeAdvanced.AddUObject(this, &UAstroSimulationSubsystem::HandleSimTimeAdvanced);
    bReady = true;
    UpdateRenderFrame();

    // Default floating origin: 50,000 km above Earth, if present, so the scene isn't empty.
    const int32 Earth = Registry.FindIndex(TEXT("Earth"));
    if (Earth != INDEX_NONE)
    {
        SetRenderOriginAnchor(Earth, FAstroVector3d(0.0, -5.0e7, 1.0e7));
    }

    if (Settings->bSpawnBodiesOnBeginPlay)
    {
        SpawnBodyActors();
    }
}

void UAstroSimulationSubsystem::Deinitialize()
{
    if (TimeController.IsValid())
    {
        TimeController->OnSimTimeAdvanced.Remove(TimeHandle);
    }
    Super::Deinitialize();
}

void UAstroSimulationSubsystem::HandleSimTimeAdvanced(double SimSeconds, double SimDelta)
{
    if (!bReady)
    {
        return;
    }
    UpdateStepSize();
    Simulation.AdvanceTo(SimSeconds);
    UpdateRenderFrame();
    OnSimulationAdvanced.Broadcast(SimSeconds);
}

void UAstroSimulationSubsystem::UpdateStepSize()
{
    // Grow the fixed step in powers of two only as far as the per-second budget
    // requires. Depends on the time scale, never the frame rate.
    const double SimPerRealSecond = TimeController.IsValid() ? FMath::Abs(TimeController->GetTimeScale()) : 1.0;
    const double Required = SimPerRealSecond / StepsPerRealSecondBudget;

    // N-body moons cap the step so their orbits stay resolved.
    double Cap = MaxStepSeconds;
    for (int32 i = 0; i < Registry.Num(); ++i)
    {
        if (Registry.Get(i).BodyType == EAstroBodyType::Moon && Simulation.DoesBodyUseNBody(i))
        {
            Cap = FMath::Min(Cap, KeplerOrbit::OrbitalPeriod(Registry.Get(i).Elements, Registry.GetOrbitMu(i)) / MinStepsPerMoonOrbit);
        }
    }
    double Step = BaseStepSeconds;
    while (Step > Cap && Step > 1.0)
    {
        Step *= 0.5;
    }
    while (Step < Required && Step * 2.0 <= Cap)
    {
        Step *= 2.0;
    }
    Simulation.SetStepSeconds(Step);
}

void UAstroSimulationSubsystem::SpawnBodyActors()
{
    UWorld* World = GetWorld();
    const UAstroBodiesSettings* Settings = GetDefault<UAstroBodiesSettings>();
    BodyActors.SetNum(Registry.Num());

    // Reuse any body actors already placed in the level, matched by BodyID.
    for (TActorIterator<AAstroBody> It(World); It; ++It)
    {
        const int32 Index = Registry.FindIndex(It->BodyID);
        if (Index != INDEX_NONE && !BodyActors[Index].IsValid())
        {
            It->BindToSimulation(this, Index);
            BodyActors[Index] = *It;
        }
    }

    for (int32 i = 0; i < Registry.Num(); ++i)
    {
        if (BodyActors[i].IsValid())
        {
            continue;
        }
        const FBodyDefinition& Body = Registry.Get(i);
        TSubclassOf<AAstroBody> Class;
        switch (Body.BodyType)
        {
        case EAstroBodyType::Star:   Class = Settings->StarClass ? Settings->StarClass : TSubclassOf<AAstroBody>(AStar::StaticClass()); break;
        case EAstroBodyType::Planet: Class = Settings->PlanetClass ? Settings->PlanetClass : TSubclassOf<AAstroBody>(APlanet::StaticClass()); break;
        case EAstroBodyType::Moon:   Class = Settings->MoonClass ? Settings->MoonClass : TSubclassOf<AAstroBody>(AMoon::StaticClass()); break;
        }

        FActorSpawnParameters Params;
        Params.Name = MakeUniqueObjectName(World->PersistentLevel, Class, Body.BodyID);
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AAstroBody* Actor = World->SpawnActor<AAstroBody>(Class, FTransform::Identity, Params);
        if (Actor)
        {
#if WITH_EDITOR
            Actor->SetActorLabel(Body.DisplayName.ToString());
#endif
            Actor->BindToSimulation(this, i);
            BodyActors[i] = Actor;
        }
    }
}

AAstroBody* UAstroSimulationSubsystem::GetBodyActor(int32 BodyIndex) const
{
    return BodyActors.IsValidIndex(BodyIndex) ? BodyActors[BodyIndex].Get() : nullptr;
}

void UAstroSimulationSubsystem::SetRenderOrigin(const FAstroVector3d& SimPositionMeters)
{
    Origin.SetFixed(SimPositionMeters);
    UpdateRenderFrame();
}

void UAstroSimulationSubsystem::SetRenderOriginAnchor(int32 BodyIndex, const FAstroVector3d& OffsetFromBodyMeters)
{
    Origin.SetInertial(BodyIndex, OffsetFromBodyMeters);
    UpdateRenderFrame();
}

void UAstroSimulationSubsystem::SetRenderOriginBodyFixed(int32 BodyIndex, const FAstroVector3d& OffsetBodyFixedMeters)
{
    Origin.SetBodyFixed(BodyIndex, OffsetBodyFixedMeters);
    UpdateRenderFrame();
}

void UAstroSimulationSubsystem::ConvertAnchorToBodyFixed(int32 BodyIndex)
{
    if (!bReady || !Registry.GetAll().IsValidIndex(BodyIndex))
    {
        return;
    }
    const FAstroVector3d Offset = Frame.Origin - Simulation.GetBodyState(BodyIndex).Position;
    const FAstroMatrix3d SimToBody = Registry.Get(BodyIndex).GetOrientationAt(Simulation.GetSimSeconds()).Transposed();
    SetRenderOriginBodyFixed(BodyIndex, SimToBody * Offset);
}

void UAstroSimulationSubsystem::ClearRenderOriginAnchor()
{
    SetRenderOrigin(Frame.Origin);
}

void UAstroSimulationSubsystem::UpdateRenderFrame()
{
    Frame = bReady ? Origin.Compute(Registry, Simulation) : FAstroRenderFrame{ Origin.FixedOrigin, FAstroMatrix3d() };
}

void UAstroSimulationSubsystem::ShiftRenderOrigin(const FVector& EngineOffsetCm)
{
    if (bReady)
    {
        Origin.Shift(Frame.ToSimDirection(EngineOffsetCm) / AstroConstants::UnrealUnitsPerMeter, Registry, Simulation);
    }
    UpdateRenderFrame();
}

FVector UAstroSimulationSubsystem::SimToScaledEnginePosition(const FAstroVector3d& SimPositionMeters, double* OutScaleFactor) const
{
    const FScaledSpacePlacement Placement = ScaledSpace::Place(SimPositionMeters - Frame.Origin, LinearRenderLimitMeters);
    if (OutScaleFactor)
    {
        *OutScaleFactor = Placement.ScaleFactor;
    }
    return Frame.ToEngineDirection(Placement.RenderOffsetMeters * AstroConstants::UnrealUnitsPerMeter);
}

bool UAstroSimulationSubsystem::GetBodyRenderTransform(int32 BodyIndex, FVector& OutLocationCm, FQuat& OutRotation, double& OutScaleFactor) const
{
    if (!bReady || !Registry.GetAll().IsValidIndex(BodyIndex))
    {
        return false;
    }
    OutLocationCm = SimToScaledEnginePosition(Simulation.GetBodyState(BodyIndex).Position, &OutScaleFactor);
    OutRotation = SimToEngineRotation(Registry.Get(BodyIndex).GetOrientationAt(Simulation.GetSimSeconds()));
    return true;
}

TArray<FName> UAstroSimulationSubsystem::GetAllBodyIDs() const
{
    TArray<FName> IDs;
    for (const FBodyDefinition& Body : Registry.GetAll())
    {
        IDs.Add(Body.BodyID);
    }
    return IDs;
}

FVector UAstroSimulationSubsystem::GetBodySimPositionKm(FName BodyID) const
{
    const int32 Index = Registry.FindIndex(BodyID);
    if (!bReady || Index == INDEX_NONE)
    {
        return FVector::ZeroVector;
    }
    const FAstroVector3d& P = Simulation.GetBodyState(Index).Position;
    return FVector(P.X, P.Y, P.Z) / 1000.0;
}

double UAstroSimulationSubsystem::GetDistanceBetweenBodiesKm(FName A, FName B) const
{
    const int32 IA = Registry.FindIndex(A), IB = Registry.FindIndex(B);
    if (!bReady || IA == INDEX_NONE || IB == INDEX_NONE)
    {
        return 0.0;
    }
    return (Simulation.GetBodyState(IA).Position - Simulation.GetBodyState(IB).Position).Length() / 1000.0;
}

bool UAstroSimulationSubsystem::CanResolveMoonInNBody(int32 MoonIndex) const
{
    if (!bReady || !Registry.GetAll().IsValidIndex(MoonIndex))
    {
        return false;
    }
    const double Period = KeplerOrbit::OrbitalPeriod(Registry.Get(MoonIndex).Elements, Registry.GetOrbitMu(MoonIndex));
    const double SimPerRealSecond = TimeController.IsValid() ? FMath::Abs(TimeController->GetTimeScale()) : 1.0;
    return SimPerRealSecond * MinStepsPerMoonOrbit / Period <= StepsPerRealSecondBudget;
}

void UAstroSimulationSubsystem::SetMoonUsesNBody(int32 MoonIndex, bool bUseNBody)
{
    if (!bReady || Simulation.DoesBodyUseNBody(MoonIndex) == bUseNBody)
    {
        return;
    }
    Simulation.SetMoonUsesNBody(MoonIndex, bUseNBody);
    UpdateStepSize();
}
