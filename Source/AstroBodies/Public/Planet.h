#pragma once
#include "AstroBody.h"
#include "Planet.generated.h"
// See CLAUDE.md Phase 2.

UCLASS()
class ASTROBODIES_API APlanet : public AAstroBody
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Planet")
    bool bHasRings = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Planet")
    TArray<FName> MoonBodyIDs;

protected:
    virtual void OnBoundToDefinition(const FBodyDefinition& Definition) override;
};
