#pragma once
#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "SunCoronaComponent.generated.h"
// Corona/flare VFX driven by AStar::LuminosityWatts. See CLAUDE.md Phase 6.
// A camera-facing quad around the star (additive, depth-tested so planets eclipse it);
// the photosphere itself is the star's own mesh. Screen-space glare comes from bloom.

UCLASS(ClassGroup = (Astro), meta = (BlueprintSpawnableComponent))
class ASTRORENDERING_API USunCoronaComponent : public UStaticMeshComponent
{
    GENERATED_BODY()

public:
    USunCoronaComponent();

    // Quad half-size in solar radii.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astro|Corona")
    float CoronaExtent = 6.0f;

    void SetCoronaLuminance(double Luminance);

    virtual void OnRegister() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<class UMaterialInstanceDynamic> CoronaMID;
};
