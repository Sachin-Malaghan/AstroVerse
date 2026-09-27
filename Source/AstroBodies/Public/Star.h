#pragma once
#include "AstroBody.h"
#include "Star.generated.h"
// See CLAUDE.md Phase 2 / Phase 6 (corona + flare VFX lives in AstroRendering).

UCLASS()
class ASTROBODIES_API AStar : public AAstroBody
{
    GENERATED_BODY()
public:
    AStar();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Star")
    double LuminosityWatts = 0.0;
    // Drives the scene's dynamic light — see AstroRendering::SunCoronaComponent.

protected:
    virtual void OnBoundToDefinition(const FBodyDefinition& Definition) override;
};
