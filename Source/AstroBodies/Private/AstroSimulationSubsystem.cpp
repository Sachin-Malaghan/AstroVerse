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
    OnSimulationAdvanced.Broadcast(SimSeconds);
}

void UAstroSimulationSubsystem::UpdateStepSize()
{
    // Grow the fixed step in powers of two only as far as the per-second budget
    // requires. Depends on the time scale, never the frame rate.
    const double SimPerRealSecond = TimeController.IsValid() ? FMath::Abs(TimeController->GetTimeScale()) : 1.0;
    const double Required = SimPerRealSecond / StepsPerRealSecondBudget;
    double Step = BaseStepSeconds;
    while (Step < Required && Step * 2.0 <= MaxStepSeconds)
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
    AnchorBodyIndex = INDEX_NONE;
    RenderOrigin = SimPositionMeters;
}

void UAstroSimulationSubsystem::SetRenderOriginAnchor(int32 BodyIndex, const FAstroVector3d& OffsetFromBodyMeters)
{
    AnchorBodyIndex = BodyIndex;
    AnchorOffset = OffsetFromBodyMeters;
}

void UAstroSimulationSubsystem::ClearRenderOriginAnchor()
{
    RenderOrigin = GetRenderOrigin();
    AnchorBodyIndex = INDEX_NONE;
}

FAstroVector3d UAstroSimulationSubsystem::GetRenderOrigin() const
{
    if (AnchorBodyIndex != INDEX_NONE && bReady)
    {
        return Simulation.GetBodyState(AnchorBodyIndex).Position + AnchorOffset;
    }
    return RenderOrigin;
}

void UAstroSimulationSubsystem::ShiftRenderOrigin(const FVector& EngineOffsetCm)
{
    const FAstroVector3d Delta = EngineToSimDirection(EngineOffsetCm) / AstroConstants::UnrealUnitsPerMeter;
    if (AnchorBodyIndex != INDEX_NONE)
    {
        AnchorOffset += Delta;
    }
    else
    {
        RenderOrigin += Delta;
    }
}

FVector UAstroSimulationSubsystem::SimToEngineDirection(const FAstroVector3d& V)
{
    // Ecliptic is right-handed (Z = ecliptic north); the engine is left-handed Z-up: flip Y.
    return FVector(V.X, -V.Y, V.Z);
}

FAstroVector3d UAstroSimulationSubsystem::EngineToSimDirection(const FVector& V)
{
    return FAstroVector3d(V.X, -V.Y, V.Z);
}

FVector UAstroSimulationSubsystem::SimToEnginePosition(const FAstroVector3d& SimPositionMeters) const
{
    return SimToEngineDirection((SimPositionMeters - GetRenderOrigin()) * AstroConstants::UnrealUnitsPerMeter);
}

FAstroVector3d UAstroSimulationSubsystem::EngineToSimPosition(const FVector& EngineLocationCm) const
{
    return GetRenderOrigin() + EngineToSimDirection(EngineLocationCm) / AstroConstants::UnrealUnitsPerMeter;
}

FQuat UAstroSimulationSubsystem::SimToEngineRotation(const FAstroMatrix3d& BodyToEcliptic)
{
    // Conjugate by the Y-flip so the result is a proper rotation in engine space.
    const FVector X = SimToEngineDirection(BodyToEcliptic.GetColumn(0));
    const FVector Y = -SimToEngineDirection(BodyToEcliptic.GetColumn(1));
    const FVector Z = SimToEngineDirection(BodyToEcliptic.GetColumn(2));
    return FMatrix(FPlane(X, 0.0), FPlane(Y, 0.0), FPlane(Z, 0.0), FPlane(0.0, 0.0, 0.0, 1.0)).ToQuat();
}

bool UAstroSimulationSubsystem::GetBodyRenderTransform(int32 BodyIndex, FVector& OutLocationCm, FQuat& OutRotation, double& OutScaleFactor) const
{
    if (!bReady || !Registry.GetAll().IsValidIndex(BodyIndex))
    {
        return false;
    }
    const FAstroVector3d TrueOffset = Simulation.GetBodyState(BodyIndex).Position - GetRenderOrigin();
    const FScaledSpacePlacement Placement = ScaledSpace::Place(TrueOffset, LinearRenderLimitMeters);
    OutLocationCm = SimToEngineDirection(Placement.RenderOffsetMeters * AstroConstants::UnrealUnitsPerMeter);
    OutScaleFactor = Placement.ScaleFactor;
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
