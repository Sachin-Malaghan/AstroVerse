#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AstroFloatingOriginComponent.generated.h"
// Keeps its owner (the player pawn) near engine (0,0,0): once the owner strays past
// RebaseDistance, the render origin moves by the owner's offset and the owner is
// teleported back to the origin, so float precision around the viewer never degrades.
// Runs after movement and before body actors place themselves. See CLAUDE.md Phase 7.

UCLASS(ClassGroup = (Astro), meta = (BlueprintSpawnableComponent))
class ASTROBODIES_API UAstroFloatingOriginComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAstroFloatingOriginComponent();

    // Engine distance (cm) from the origin that triggers a rebase. 1 km keeps float error < 0.1 mm.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astro|FloatingOrigin")
    double RebaseDistanceCm = 100000.0;

    UFUNCTION(BlueprintCallable, Category = "Astro|FloatingOrigin")
    void RebaseNow();

    int32 GetRebaseCount() const { return RebaseCount; }

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    int32 RebaseCount = 0;
};
