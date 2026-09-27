#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BodyAppearanceRow.h"
#include "BodyShadingComponent.generated.h"
// Presentation for one AAstroBody: surface material, atmosphere shell, rings and
// (for the star) corona, plus the per-frame shading inputs that a scene light can't
// provide in scaled space — each body's true direction to the Sun and the illuminance
// it receives. Attached by UAstroRenderingSubsystem. See CLAUDE.md Phase 6.

class AAstroBody;
class UAstroSimulationSubsystem;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class USunCoronaComponent;

UCLASS(ClassGroup = (Astro))
class ASTRORENDERING_API UBodyShadingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBodyShadingComponent();

    // Must be called before RegisterComponent.
    void Configure(const FAstroBodyAppearanceRow* InAppearance);

    virtual void OnRegister() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Rendering")
    TObjectPtr<UStaticMeshComponent> AtmosphereShell;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Rendering")
    TObjectPtr<UStaticMeshComponent> Rings;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Rendering")
    TObjectPtr<USunCoronaComponent> Corona;

    // Illuminance from the star at this body this frame (lux).
    double GetSunLux() const { return SunLux; }

    // Atmosphere data for the engine SkyAtmosphere, which takes over near the body.
    bool HasAtmosphere() const { return AtmosphereTopMeters > 0.0; }
    double GetAtmosphereTopMeters() const { return AtmosphereTopMeters; }
    const FAstroBodyAppearanceRow& GetAppearance() const { return Appearance; }
    // Hides the far-field shell while the engine SkyAtmosphere renders this body.
    void SetShellVisible(bool bVisible);

private:
    void BuildComponents();

    FAstroBodyAppearanceRow Appearance;
    bool bHasAppearance = false;
    double SunLux = 0.0;
    double AtmosphereTopMeters = 0.0;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> AtmosphereMID;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> RingMID;
};
