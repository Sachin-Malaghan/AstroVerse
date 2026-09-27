#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ActivationManager.h"
#include "AstroActivationSubsystem.generated.h"
// Per-world driver for FActivationManager: feeds it the player's view each
// frame and applies the resulting tiers — Active moons become N-body
// particles (when the step can resolve them), Ambient/Active bodies get full
// render detail. See CLAUDE.md Phase 5.

class UAstroSimulationSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObservationLockEvicted, FName, EvictedBodyID, FName, NewBodyID);
// A body that should be Active can't get full N-body at the current time scale (or can again).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFullPhysicsAvailabilityChanged, FName, BodyID, bool, bAvailable);

UCLASS()
class ASTROACTIVATION_API UAstroActivationSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    static UAstroActivationSubsystem* Get(const UObject* WorldContext);

    // Returns the body demoted to make room (the 3-lock cap), or None. Also broadcasts OnLockEvicted.
    UFUNCTION(BlueprintCallable, Category = "Astro|Activation")
    FName LockObservationTarget(FName BodyID);

    UFUNCTION(BlueprintCallable, Category = "Astro|Activation")
    void ReleaseObservationLock(FName BodyID);

    UFUNCTION(BlueprintPure, Category = "Astro|Activation")
    TArray<FName> GetLockedTargets() const { return Manager.GetLockedTargets(); }

    // Makes BodyID the local reference frame: force-Active and the floating origin rides with it.
    UFUNCTION(BlueprintCallable, Category = "Astro|Activation")
    void SetReferenceFrame(FName BodyID);

    UFUNCTION(BlueprintCallable, Category = "Astro|Activation")
    void ClearReferenceFrame();

    UFUNCTION(BlueprintPure, Category = "Astro|Activation")
    FName GetReferenceFrame() const { return Manager.GetReferenceFrame(); }

    UFUNCTION(BlueprintPure, Category = "Astro|Activation")
    EFidelityTier GetTierForBody(FName BodyID) const { return Manager.GetTierForBody(BodyID); }

    UPROPERTY(BlueprintAssignable, Category = "Astro|Activation")
    FOnObservationLockEvicted OnLockEvicted;

    UPROPERTY(BlueprintAssignable, Category = "Astro|Activation")
    FOnFullPhysicsAvailabilityChanged OnFullPhysicsAvailabilityChanged;

    const FActivationManager& GetManager() const { return Manager; }

private:
    bool TryInitialize();
    void GatherViews(TArray<FActivationBodyView>& OutViews) const;
    void ApplyTiers();

    FActivationManager Manager;
    TWeakObjectPtr<UAstroSimulationSubsystem> Simulation;
    TArray<bool> FullPhysicsBlocked; // per body: Active but N-body not resolvable right now
    bool bInitialized = false;
};
