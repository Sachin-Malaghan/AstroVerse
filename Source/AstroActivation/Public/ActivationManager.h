#pragma once
#include "CoreMinimal.h"
#include "FidelityTier.h"
// Watches camera frustum, distance, and selection state; promotes/demotes
// bodies between tiers. Locks are capped at 3 concurrent (see CLAUDE.md
// Phase 5) — locking one more demotes the oldest, and the caller is told.
// Promotion never snaps position: Dormant bodies keep propagating, and the
// simulation hands off the exact state when a moon switches to/from N-body.
// Engine-agnostic apart from Core types, so it is unit-testable in isolation.

class FBodyRegistry;

// What the manager needs to know about one body this frame.
struct FActivationBodyView
{
    bool bInFrustum = false;
    double AngularRadiusRad = 0.0; // apparent size from the camera
};

struct ASTROACTIVATION_API FActivationResult
{
    TArray<EFidelityTier> Tiers;      // indexed by registry body index
    FName EvictedLock;                // set if the last LockObservationTarget pushed one out
};

class ASTROACTIVATION_API FActivationManager
{
public:
    static constexpr int32 MaxConcurrentLocks = 3;

    void Initialize(const FBodyRegistry* InRegistry);

    // Recomputes every body's tier from the latest view data (same indexing as the registry).
    void Tick(float DeltaSeconds, const TArray<FActivationBodyView>& Views);

    // Returns the body that was demoted to make room, or NAME_None.
    FName LockObservationTarget(FName BodyID);
    void ReleaseObservationLock(FName BodyID);
    void ReleaseAllLocks();
    const TArray<FName>& GetLockedTargets() const { return LockedTargets; }

    void SetReferenceFrame(FName BodyID); // body the player is currently standing on / orbiting
    void ClearReferenceFrame() { CurrentReferenceFrame = NAME_None; }
    FName GetReferenceFrame() const { return CurrentReferenceFrame; }

    EFidelityTier GetTierForBody(FName BodyID) const;
    EFidelityTier GetTierForIndex(int32 BodyIndex) const;
    const TArray<EFidelityTier>& GetTiers() const { return Tiers; }

    // Bodies smaller than this on screen stay Dormant even inside the frustum (~1 px at 1080p/60deg).
    double MinAmbientAngularRadiusRad = 0.0005;

private:
    bool IsStructurallyActive(int32 BodyIndex) const;

    const FBodyRegistry* Registry = nullptr;
    TArray<FName> LockedTargets; // oldest first
    FName CurrentReferenceFrame;
    TArray<EFidelityTier> Tiers;
};
