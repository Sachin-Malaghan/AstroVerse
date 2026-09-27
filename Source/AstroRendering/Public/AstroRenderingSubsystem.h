#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BodyAppearanceRow.h"
#include "AstroRenderingSubsystem.generated.h"
// Decorates the simulation's body actors with presentation (UBodyShadingComponent),
// spawns the space environment, and applies the per-platform render budget
// (desktop: Lumen/VSM/TSR; VR: trimmed for 90 Hz). See CLAUDE.md Phase 6.

class AAstroSpaceEnvironment;

UCLASS()
class ASTRORENDERING_API UAstroRenderingSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    static UAstroRenderingSubsystem* Get(const UObject* WorldContext);

    AAstroSpaceEnvironment* GetEnvironment() const { return Environment.Get(); }
    bool IsVRBudgetActive() const { return bVRBudget; }

    // Re-applies the budget for the current device (call after toggling stereo).
    void ApplyRenderBudget();

private:
    bool TryDecorate();
    void LoadAppearance();

    TMap<FName, FAstroBodyAppearanceRow> Appearance;
    TWeakObjectPtr<AAstroSpaceEnvironment> Environment;
    bool bDecorated = false;
    bool bVRBudget = false;
};
