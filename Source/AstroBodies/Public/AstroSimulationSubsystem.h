#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BodyRegistry.h"
#include "SolarSystemSimulation.h"
#include "AstroRenderFrame.h"
#include "AstroSimulationSubsystem.generated.h"
// Owns the body registry and FSolarSystemSimulation for one world, advances
// them off AstroTime::UTimeController, spawns the body actors, and holds the
// floating render origin. See CLAUDE.md Phase 2 and "Scale and precision".

class AAstroBody;
class UTimeController;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAstroSimulationAdvanced, double /*SimSeconds*/);
// A dev / scripted command put the viewer somewhere explicitly (camera modes should yield).
DECLARE_MULTICAST_DELEGATE(FOnAstroViewerPlaced);

UCLASS()
class ASTROBODIES_API UAstroSimulationSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    static UAstroSimulationSubsystem* Get(const UObject* WorldContext);

    bool IsReady() const { return bReady; }
    const FBodyRegistry& GetRegistry() const { return Registry; }
    FSolarSystemSimulation& GetSimulation() { return Simulation; }
    const FSolarSystemSimulation& GetSimulation() const { return Simulation; }
    const FString& GetLoadError() const { return LoadError; }

    int32 FindBodyIndex(FName BodyID) const { return Registry.FindIndex(BodyID); }
    AAstroBody* GetBodyActor(int32 BodyIndex) const;

    // --- Floating origin. The render origin is where engine (0,0,0) sits in sim space.
    // Fixed, anchored to a body in inertial axes (orbiting / flying near it), or anchored in a
    // body's co-rotating frame with engine +Z = local up (landed: the ground holds still).
    void SetRenderOrigin(const FAstroVector3d& SimPositionMeters);
    void SetRenderOriginAnchor(int32 BodyIndex, const FAstroVector3d& OffsetFromBodyMeters);
    // Offset is in the body-fixed frame; engine axes become local east / south / up at that point.
    void SetRenderOriginBodyFixed(int32 BodyIndex, const FAstroVector3d& OffsetBodyFixedMeters);

    FOnAstroViewerPlaced OnViewerPlaced;
    // Switches to the body's rotating frame without moving the origin (e.g. on touchdown).
    void ConvertAnchorToBodyFixed(int32 BodyIndex);
    void ClearRenderOriginAnchor();
    FAstroVector3d GetRenderOrigin() const { return Frame.Origin; }
    int32 GetRenderOriginAnchorBody() const { return Origin.Mode == FAstroRenderOriginState::EMode::Fixed ? INDEX_NONE : Origin.BodyIndex; }
    bool IsRotatingFrame() const { return Origin.Mode == FAstroRenderOriginState::EMode::BodyFixed; }
    const FAstroRenderFrame& GetRenderFrame() const { return Frame; }
    // Shifts the origin by an engine-space offset (cm), e.g. when a pawn rebases to (0,0,0).
    void ShiftRenderOrigin(const FVector& EngineOffsetCm);

    // Engine-space placement for a body this frame: location (cm), rotation, and
    // the scaled-space factor to apply to its true radius.
    bool GetBodyRenderTransform(int32 BodyIndex, FVector& OutLocationCm, FQuat& OutRotation, double& OutScaleFactor) const;

    // Sim-space <-> engine-space conversion for this frame (true scale).
    FVector SimToEngineDirection(const FAstroVector3d& V) const { return Frame.ToEngineDirection(V); }
    FAstroVector3d EngineToSimDirection(const FVector& V) const { return Frame.ToSimDirection(V); }
    FVector SimToEnginePosition(const FAstroVector3d& SimPositionMeters) const { return Frame.ToEnginePosition(SimPositionMeters); }
    FAstroVector3d EngineToSimPosition(const FVector& EngineLocationCm) const { return Frame.ToSimPosition(EngineLocationCm); }
    FQuat SimToEngineRotation(const FAstroMatrix3d& BodyToEcliptic) const { return Frame.ToEngineRotation(BodyToEcliptic); }
    // Scaled-space engine location for an arbitrary sim point (for markers, trails, labels).
    FVector SimToScaledEnginePosition(const FAstroVector3d& SimPositionMeters, double* OutScaleFactor = nullptr) const;

    double GetLinearRenderLimitMeters() const { return LinearRenderLimitMeters; }

    // True if a moon's orbit can be integrated at >= MinStepsPerMoonOrbit steps per orbit
    // within the per-second step budget at the current time scale.
    bool CanResolveMoonInNBody(int32 MoonIndex) const;
    // Promote/demote a moon and immediately re-fit the step size so its orbit is never under-resolved.
    void SetMoonUsesNBody(int32 MoonIndex, bool bUseNBody);
    static constexpr double MinStepsPerMoonOrbit = 100.0;

    FOnAstroSimulationAdvanced OnSimulationAdvanced;

    // Blueprint-facing helpers.
    UFUNCTION(BlueprintPure, Category = "Astro|Bodies")
    TArray<FName> GetAllBodyIDs() const;

    UFUNCTION(BlueprintPure, Category = "Astro|Bodies")
    FVector GetBodySimPositionKm(FName BodyID) const;

    UFUNCTION(BlueprintPure, Category = "Astro|Bodies")
    double GetDistanceBetweenBodiesKm(FName A, FName B) const;

private:
    void HandleSimTimeAdvanced(double SimSeconds, double SimDelta);
    void UpdateStepSize();
    void SpawnBodyActors();
    void UpdateRenderFrame();

    FBodyRegistry Registry;
    FSolarSystemSimulation Simulation;
    FString LoadError;
    bool bReady = false;

    TWeakObjectPtr<UTimeController> TimeController;
    FDelegateHandle TimeHandle;

    TArray<TWeakObjectPtr<AAstroBody>> BodyActors;

    FAstroRenderOriginState Origin;
    FAstroRenderFrame Frame;
    double LinearRenderLimitMeters = 1.0e8;

    double BaseStepSeconds = 3600.0;
    double MaxStepSeconds = 86400.0;
    int32 StepsPerRealSecondBudget = 60000;
};
