#pragma once
#include "CoreMinimal.h"
// Separate scale-domain: Milky Way disc representation + the Sun's real
// position/velocity marker. NOT per-star N-body — a statistical/artistic
// representation. See CLAUDE.md Phase 10.
//
// Owns the galaxy scene and the camera moves that make entering and leaving it read as a
// continuous zoom: in, the view pulls back from the Sun to take in the whole disc; out, it
// dives back to the Sun before the fade to the solar system.

class UWorld;
class AActor;
class AAstroGalaxyActor;

class ASTROGALAXY_API FGalaxyView
{
public:
    // Called (faded to black) by UAstroScaleDomainSubsystem.
    void ActivateGalaxyScaleDomain(UWorld* World);
    void ReturnToSolarSystemScaleDomain(UWorld* World);

    // Per-frame: advances an intro/outro camera move. Returns true while one is running.
    bool Tick(UWorld* World, float DeltaSeconds);
    // Starts the dive back toward the Sun; OnDone fires when it ends (then the fade starts).
    void BeginOutro(UWorld* World, TFunction<void()> OnDone);

    bool IsActive() const { return bActive; }
    bool IsCameraMoveRunning() const { return MoveTime >= 0.0f; }
    AAstroGalaxyActor* GetActor() const { return Actor.Get(); }

    // Flight speed scale for pawns in the galaxy domain (engine cm): distance to the
    // nearer of the Sun marker and the galactic plane, so zooming feels the same at any scale.
    double GetFlightScaleCm(const FVector& EngineLocation) const;

private:
    void StartMove(UWorld* World, const FVector& From, const FVector& To, const FVector& LookFrom, const FVector& LookTo, float Seconds);

    bool bActive = false;
    TWeakObjectPtr<AAstroGalaxyActor> Actor;

    // Camera move (positions relative to the galactic center, engine cm).
    FVector MoveFrom, MoveTo, LookFrom, LookTo;
    float MoveTime = -1.0f;
    float MoveDuration = 1.0f;
    TFunction<void()> OnMoveDone;

    // Where the viewer was in the solar system, restored on return.
    FQuat SolarRotation = FQuat::Identity;
};
