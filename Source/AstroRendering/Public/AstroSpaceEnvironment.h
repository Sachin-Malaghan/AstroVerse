#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AstroSpaceEnvironment.generated.h"
// Scene-wide presentation for space: exposure metered from the real sunlight at the
// camera (so a sunlit Earth, a dim Neptune and the Sun's glare all sit at believable
// relative brightness), the galactic star background, and the star's light intensity.
// Spawned by UAstroRenderingSubsystem. See CLAUDE.md Phase 6.

class UPostProcessComponent;
class UMaterialInstanceDynamic;

UCLASS(NotPlaceable)
class ASTRORENDERING_API AAstroSpaceEnvironment : public AActor
{
    GENERATED_BODY()

public:
    AAstroSpaceEnvironment();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Environment")
    TObjectPtr<UPostProcessComponent> PostProcess;

    // Current metered exposure (EV100), after smoothing and compensation.
    UFUNCTION(BlueprintPure, Category = "Astro|Environment")
    double GetExposureEV100() const { return CurrentEV100; }

    // Illuminance from the star at the camera (lux).
    UFUNCTION(BlueprintPure, Category = "Astro|Environment")
    double GetSunLuxAtCamera() const { return SunLuxAtCamera; }

private:
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> StarFieldMID;

    double CurrentEV100 = 15.0;
    double SunLuxAtCamera = 0.0;
    bool bExposureInitialized = false;
};
