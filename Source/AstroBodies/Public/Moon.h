#pragma once
#include "AstroBody.h"
#include "Moon.generated.h"
// See CLAUDE.md Phase 2. Moons default to full N-body fidelity only when
// Active-tier (see AstroActivation::EFidelityTier); Dormant moons use the
// cheap analytic propagation like every other body.

UCLASS()
class ASTROBODIES_API AMoon : public AAstroBody
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Moon")
    FName ParentPlanetBodyID;

protected:
    virtual void OnBoundToDefinition(const FBodyDefinition& Definition) override;
};
