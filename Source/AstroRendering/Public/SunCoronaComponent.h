#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SunCoronaComponent.generated.h"
// Corona/flare VFX driven by AStar::LuminosityWatts. See CLAUDE.md Phase 6.

UCLASS(ClassGroup = (Astro), meta = (BlueprintSpawnableComponent))
class ASTRORENDERING_API USunCoronaComponent : public UActorComponent
{
    GENERATED_BODY()
};
