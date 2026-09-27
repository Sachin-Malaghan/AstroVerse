#pragma once
#include "CoreMinimal.h"
#include "FidelityTier.generated.h"
// Three tiers, one system. Mode ("walking" vs "observation") changes
// membership, not the physics that runs. See CLAUDE.md Phase 5.

UENUM(BlueprintType)
enum class EFidelityTier : uint8
{
    Dormant,  // cheap analytic two-body propagation — always running, every body
    Ambient,  // promoted: currently in camera frustum
    Active    // full N-body — player's reference frame or a locked observation target
};
