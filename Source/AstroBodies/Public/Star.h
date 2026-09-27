#pragma once
#include "CelestialBody.h"
#include "Star.generated.h"
// See CLAUDE.md Phase 2 / Phase 6 (corona + flare VFX lives in AstroRendering).

UCLASS()
class ASTROBODIES_API AStar : public ACelestialBody
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astro|Star")
    double LuminosityWatts = 0.0;
    // Drives the scene's dynamic light — see AstroRendering::SunCoronaComponent.
};
