#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BodyAppearanceRow.h"
#include "AstroRenderingSubsystem.generated.h"
// Decorates the simulation's body actors with presentation (UBodyShadingComponent),
// spawns the space environment, and applies the per-platform render budget
// (desktop: Lumen/VSM/TSR; VR: trimmed for 90 Hz; mobile: groundwork). See CLAUDE.md Phase 6
// and Phase 12.

UENUM()
enum class EAstroRenderBudget : uint8
{
    Desktop,
    VR,
    Mobile,
};

class AAstroSpaceEnvironment;
class AAstroTerrainActor;

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
    AAstroTerrainActor* GetTerrain() const { return Terrain.Get(); }
    bool IsVRBudgetActive() const { return Budget == EAstroRenderBudget::VR; }
    EAstroRenderBudget GetBudget() const { return Budget; }
    FString GetBudgetName() const;
    // Frame-time target for the active budget (UAstroRenderingSettings *TargetHz).
    double GetFrameBudgetMs() const;

    // Re-applies the budget for the current device (call after toggling stereo).
    // astro.Render.Budget (auto|desktop|vr|mobile) forces one, e.g. to profile VR settings
    // without a headset.
    void ApplyRenderBudget();

private:
    bool TryDecorate();
    void LoadAppearance();

    TMap<FName, FAstroBodyAppearanceRow> Appearance;
    TWeakObjectPtr<AAstroSpaceEnvironment> Environment;
    TWeakObjectPtr<AAstroTerrainActor> Terrain;
    bool bDecorated = false;
    EAstroRenderBudget Budget = EAstroRenderBudget::Desktop;
};
