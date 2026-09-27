#pragma once
#include "CoreMinimal.h"
#include "FidelityTier.h"
// Watches camera frustum, distance, and selection state; promotes/demotes
// bodies between tiers. Locks are capped at 2-3 concurrent (see CLAUDE.md
// Phase 5) — locking one more demotes the oldest, UI states this plainly.
// Promotion never snaps position: Dormant bodies keep propagating, so a
// handoff to Active is a correction blend, not a creation from nothing.

class ASTROACTIVATION_API FActivationManager
{
public:
    void Tick(float DeltaSeconds);
    void LockObservationTarget(FName BodyID);
    void ReleaseObservationLock(FName BodyID);
    void SetReferenceFrame(FName BodyID); // body the player is currently standing on
    EFidelityTier GetTierForBody(FName BodyID) const;

private:
    static constexpr int32 MaxConcurrentLocks = 3;
    TArray<FName> LockedTargets;
    FName CurrentReferenceFrame;
};
