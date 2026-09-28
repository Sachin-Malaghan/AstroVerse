#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/AstroVector3d.h"
#include "AstroBeltActor.generated.h"
// Small-body populations as statistical ensembles (never N-body, per CLAUDE.md "Fidelity
// tier"): the main asteroid belt with its Kirkwood gaps, Jupiter's Trojans at L4 / L5, and
// the Kuiper belt (classical belt + Plutinos in 3:2 resonance with Neptune). Each object is a
// heliocentric Kepler orbit sampled from published distributions (a, e, i); positions are
// solved every frame on worker threads and drawn as instanced dots of constant screen size,
// placed through the same scaled-space mapping as the planets. See CLAUDE.md "Small bodies".

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

UENUM()
enum class EAstroBeltGroup : uint8
{
    MainBelt,
    Trojans,
    Kuiper,
};

UCLASS(NotPlaceable)
class ASTRORENDERING_API AAstroBeltActor : public AActor
{
    GENERATED_BODY()

public:
    AAstroBeltActor();
    virtual void Tick(float DeltaSeconds) override;
    virtual void BeginPlay() override;

    int32 GetObjectCount() const { return Objects.Num(); }

private:
    struct FBeltObject
    {
        // Orbit: position = P (cos E - e) + Q sin E, with P, Q the scaled in-plane axes.
        FAstroVector3d P, Q;
        double Eccentricity = 0.0;
        double MeanAnomalyAtEpoch = 0.0;
        double MeanMotion = 0.0; // rad / s
        uint8 Group = 0;
    };

    void Populate();
    void AddObject(EAstroBeltGroup Group, double SemiMajorAxisAU, double Eccentricity, double InclinationDeg,
                   double NodeDeg, double ArgPeriDeg, double MeanAnomalyDeg);

    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Groups;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Glows;
    TArray<FBeltObject> Objects;
    TArray<TArray<int32>> GroupMembers;
    TArray<FTransform> Scratch;
    bool bPopulated = false;
};
