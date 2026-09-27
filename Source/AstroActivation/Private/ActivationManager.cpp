// See CLAUDE.md Phase 5.
#include "ActivationManager.h"
#include "BodyRegistry.h"

void FActivationManager::Initialize(const FBodyRegistry* InRegistry)
{
    Registry = InRegistry;
    LockedTargets.Reset();
    CurrentReferenceFrame = NAME_None;
    Tiers.Init(EFidelityTier::Dormant, Registry ? Registry->Num() : 0);
}

bool FActivationManager::IsStructurallyActive(int32 BodyIndex) const
{
    const FBodyDefinition& Body = Registry->Get(BodyIndex);
    if (LockedTargets.Contains(Body.BodyID) || Body.BodyID == CurrentReferenceFrame)
    {
        return true;
    }
    // Standing on a moon makes its parent Active too.
    const int32 Frame = Registry->FindIndex(CurrentReferenceFrame);
    return Frame != INDEX_NONE && Registry->Get(Frame).BodyType == EAstroBodyType::Moon && Registry->Get(Frame).ParentIndex == BodyIndex;
}

void FActivationManager::Tick(float DeltaSeconds, const TArray<FActivationBodyView>& Views)
{
    if (!Registry)
    {
        return;
    }
    Tiers.SetNum(Registry->Num());
    for (int32 i = 0; i < Registry->Num(); ++i)
    {
        if (IsStructurallyActive(i))
        {
            Tiers[i] = EFidelityTier::Active;
        }
        else if (Views.IsValidIndex(i) && Views[i].bInFrustum && Views[i].AngularRadiusRad >= MinAmbientAngularRadiusRad)
        {
            Tiers[i] = EFidelityTier::Ambient;
        }
        else
        {
            Tiers[i] = EFidelityTier::Dormant;
        }
    }
}

FName FActivationManager::LockObservationTarget(FName BodyID)
{
    if (!Registry || Registry->FindIndex(BodyID) == INDEX_NONE)
    {
        return NAME_None;
    }
    // Re-locking an existing target refreshes it to newest.
    LockedTargets.Remove(BodyID);
    FName Evicted = NAME_None;
    if (LockedTargets.Num() >= MaxConcurrentLocks)
    {
        Evicted = LockedTargets[0];
        LockedTargets.RemoveAt(0);
    }
    LockedTargets.Add(BodyID);
    return Evicted;
}

void FActivationManager::ReleaseObservationLock(FName BodyID)
{
    LockedTargets.Remove(BodyID);
}

void FActivationManager::ReleaseAllLocks()
{
    LockedTargets.Reset();
}

void FActivationManager::SetReferenceFrame(FName BodyID)
{
    CurrentReferenceFrame = BodyID;
}

EFidelityTier FActivationManager::GetTierForBody(FName BodyID) const
{
    return Registry ? GetTierForIndex(Registry->FindIndex(BodyID)) : EFidelityTier::Dormant;
}

EFidelityTier FActivationManager::GetTierForIndex(int32 BodyIndex) const
{
    return Tiers.IsValidIndex(BodyIndex) ? Tiers[BodyIndex] : EFidelityTier::Dormant;
}
