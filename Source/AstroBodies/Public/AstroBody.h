#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BodyDataRow.h"
#include "AstroBody.generated.h"
// Base class for every simulated body. See CLAUDE.md Phase 2.
// Real orbital elements and mass live in Data Tables (Content/Bodies/DataTables),
// not hardcoded here — adding a body is a data change, not a recompile.
// The actor is presentation only: each frame it reads its state from
// UAstroSimulationSubsystem, which owns the physics.

class UAstroSimulationSubsystem;
class UStaticMeshComponent;
struct FBodyDefinition;

UCLASS(Abstract)
class ASTROBODIES_API AAstroBody : public AActor
{
    GENERATED_BODY()

public:
    AAstroBody();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astro|Body")
    FName BodyID; // matches a row in DT_Planets / DT_Moons

    // Filled from the registry when bound; shown for inspection.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Body")
    double MassKg = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Body")
    double RadiusMeters = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Body")
    EAstroBodyType BodyType = EAstroBodyType::Planet;

    // Unit sphere-ish mesh scaled to the body's true radius (placeholder until the Phase 6 art pass).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Body")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    // Radius of BodyMesh's source asset in cm (engine Sphere = 50).
    UPROPERTY(EditDefaultsOnly, Category = "Astro|Body")
    double MeshRadiusCm = 50.0;

    void BindToSimulation(UAstroSimulationSubsystem* InSimulation, int32 InBodyIndex);
    int32 GetBodyIndex() const { return BodyIndex; }
    const FBodyDefinition* GetDefinition() const;

    // Latest scaled-space factor (1 = drawn at true offset and size).
    double GetRenderScaleFactor() const { return RenderScaleFactor; }

    virtual void Tick(float DeltaSeconds) override;

protected:
    // Hook for subclasses once they know their definition (rings, lights, materials).
    virtual void OnBoundToDefinition(const FBodyDefinition& Definition) {}
    virtual void OnRenderTransformUpdated(double ScaleFactor) {}

    TWeakObjectPtr<UAstroSimulationSubsystem> Simulation;
    int32 BodyIndex = INDEX_NONE;
    double RenderScaleFactor = 1.0;
};
