#pragma once
#include "CelestialBody.h"
#include "Moon.generated.h"
// See CLAUDE.md Phase 2. Moons default to full N-body fidelity only when
// Active-tier (see AstroActivation::EFidelityTier); Dormant moons use the
// cheap analytic propagation like every other body.

UCLASS()
class ASTROBODIES_API AMoon : public ACelestialBody
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astro|Moon")
    FName ParentPlanetBodyID;
};
