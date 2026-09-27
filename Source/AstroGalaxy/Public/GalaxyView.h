#pragma once
#include "CoreMinimal.h"
// Separate scale-domain: Milky Way disc representation + the Sun's real
// position/velocity marker. NOT per-star N-body — a statistical/artistic
// representation. See CLAUDE.md Phase 10.
// Phase 7 provides the domain switch; the galaxy presentation itself is Phase 10.

class UWorld;

class ASTROGALAXY_API FGalaxyView
{
public:
    void ActivateGalaxyScaleDomain(UWorld* World);
    void ReturnToSolarSystemScaleDomain(UWorld* World);
    bool IsActive() const { return bActive; }

private:
    bool bActive = false;
};
