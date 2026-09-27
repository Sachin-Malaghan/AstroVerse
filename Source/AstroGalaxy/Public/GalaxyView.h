#pragma once
#include "CoreMinimal.h"
// Separate scale-domain: Milky Way disc representation + the Sun's real
// position/velocity marker. NOT per-star N-body — a statistical/artistic
// representation. See CLAUDE.md Phase 10.

class ASTROGALAXY_API FGalaxyView
{
public:
    void ActivateGalaxyScaleDomain();
    void ReturnToSolarSystemScaleDomain();
};
