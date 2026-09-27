#pragma once
#include "CelestialBody.h"
#include "Planet.generated.h"
// See CLAUDE.md Phase 2.

UCLASS()
class ASTROBODIES_API APlanet : public ACelestialBody
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astro|Planet")
    bool bHasRings = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astro|Planet")
    TArray<FName> MoonBodyIDs;
};
