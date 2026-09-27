#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BodyRegistry.h"
#include "SolarSystemSimulation.h"
#include "AstroSimulationSubsystem.generated.h"
// Owns the body registry and FSolarSystemSimulation for one world, advances
// them off AstroTime::UTimeController, spawns the body actors, and holds the
// floating render origin. See CLAUDE.md Phase 2 and "Scale and precision".

class AAstroBody;
class UTimeController;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAstroSimulationAdvanced, double /*SimSeconds*/);

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
    // Optionally anchored to a body so it rides along with it (landed / orbiting a body).
    void SetRenderOrigin(const FAstroVector3d& SimPositionMeters);
    void SetRenderOriginAnchor(int32 BodyIndex, const FAstroVector3d& OffsetFromBodyMeters);
    void ClearRenderOriginAnchor();
    FAstroVector3d GetRenderOrigin() const;
    int32 GetRenderOriginAnchorBody() const { return AnchorBodyIndex; }
    // Shifts the origin by an engine-space offset (cm), e.g. when a pawn rebases to (0,0,0).
    void ShiftRenderOrigin(const FVector& EngineOffsetCm);

    // Engine-space placement for a body this frame: location (cm), rotation, and
    // the scaled-space factor to apply to its true radius.
    bool GetBodyRenderTransform(int32 BodyIndex, FVector& OutLocationCm, FQuat& OutRotation, double& OutScaleFactor) const;

    // Sim-space <-> engine-space conversion (ecliptic right-handed m <-> engine left-handed cm), true scale.
    static FVector SimToEngineDirection(const FAstroVector3d& V);
    static FAstroVector3d EngineToSimDirection(const FVector& V);
    FVector SimToEnginePosition(const FAstroVector3d& SimPositionMeters) const;
    FAstroVector3d EngineToSimPosition(const FVector& EngineLocationCm) const;
    static FQuat SimToEngineRotation(const FAstroMatrix3d& BodyToEcliptic);
    // Scaled-space engine location for an arbitrary sim point (for markers, trails, labels).
    FVector SimToScaledEnginePosition(const FAstroVector3d& SimPositionMeters, double* OutScaleFactor = nullptr) const;

    double GetLinearRenderLimitMeters() const { return LinearRenderLimitMeters; }

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

    FBodyRegistry Registry;
    FSolarSystemSimulation Simulation;
    FString LoadError;
    bool bReady = false;

    TWeakObjectPtr<UTimeController> TimeController;
    FDelegateHandle TimeHandle;

    TArray<TWeakObjectPtr<AAstroBody>> BodyActors;

    FAstroVector3d RenderOrigin;
    int32 AnchorBodyIndex = INDEX_NONE;
    FAstroVector3d AnchorOffset;
    double LinearRenderLimitMeters = 1.0e8;

    double BaseStepSeconds = 3600.0;
    double MaxStepSeconds = 86400.0;
    int32 StepsPerRealSecondBudget = 60000;
};
