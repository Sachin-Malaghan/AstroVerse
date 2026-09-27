#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AsteroidField.generated.h"
// NOT individually N-body simulated — GPU-instanced/Niagara-driven with
// simplified orbital motion, visually convincing rather than per-rock accurate.
// See CLAUDE.md Phase 2 and Open Decisions (asteroid representation).

UCLASS()
class ASTROBODIES_API AAsteroidField : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astro|Asteroids")
    double InnerRadiusMeters = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astro|Asteroids")
    double OuterRadiusMeters = 0.0;
};
