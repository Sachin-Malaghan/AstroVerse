#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/AstroVector3d.h"
#include "AstroSpaceEnvironment.generated.h"
// Scene-wide presentation for space: exposure metered from the real sunlight at the
// camera (so a sunlit Earth, a dim Neptune and the Sun's glare all sit at believable
// relative brightness), the galactic star background, and the star's light intensity.
// Spawned by UAstroRenderingSubsystem. See CLAUDE.md Phase 6.

class UPostProcessComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UBodyShadingComponent;
class UAstroSimulationSubsystem;
class UMaterialInstanceDynamic;

UCLASS(NotPlaceable)
class ASTRORENDERING_API AAstroSpaceEnvironment : public AActor
{
    GENERATED_BODY()

public:
    // Galactic axes in the sim (J2000 ecliptic) frame: X to the galactic centre, Z to the
    // north galactic pole. The Milky Way band in every sky lies along the X-Y plane.
    void GetGalacticAxes(FAstroVector3d& OutX, FAstroVector3d& OutY, FAstroVector3d& OutZ) const { OutX = GalacticX; OutY = GalacticY; OutZ = GalacticZ; }
    // Milky Way guide (V): brighter band plus labels, for teaching.
    static bool IsMilkyWayGuideOn();
    // Sky rendering: 0 = realistic (as photographed), 1 = enhanced Milky Way (default).
    static int32 GetMilkyWayMode();
    static void SetMilkyWayMode(int32 Mode);

    AAstroSpaceEnvironment();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Environment")
    TObjectPtr<UPostProcessComponent> PostProcess;

    // Engine sky (multiple scattering, aerial perspective) for the one body whose
    // atmosphere the camera is in or near; far bodies keep their shell. Phase 7.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Environment")
    TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

    // Ambient from the sky (real-time capture): a sunlit atmosphere fills shadows; airless
    // bodies capture a black sky, so their shadows stay hard and dark, as they really are.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Environment")
    TObjectPtr<USkyLightComponent> SkyLight;

    // The body SkyAtmosphere is currently rendering, or INDEX_NONE.
    int32 GetSkyBodyIndex() const { return SkyBodyIndex; }

    // Current metered exposure (EV100), after smoothing and compensation.
    UFUNCTION(BlueprintPure, Category = "Astro|Environment")
    double GetExposureEV100() const { return CurrentEV100; }

    // Illuminance from the star at the camera (lux).
    UFUNCTION(BlueprintPure, Category = "Astro|Environment")
    double GetSunLuxAtCamera() const { return SunLuxAtCamera; }

private:
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> StarFieldMID;

    // Galactic frame axes in sim (ecliptic) space.
    FAstroVector3d GalacticX, GalacticY, GalacticZ;

    void UpdateSkyAtmosphere(const UAstroSimulationSubsystem* Sim, const FAstroVector3d& CameraSim, double& OutDaylight);
    void ConfigureSkyFor(const UBodyShadingComponent* Shading, double GroundRadiusMeters);

    int32 SkyBodyIndex = INDEX_NONE;
    double SkyGroundMeters = 0.0;

    double CurrentEV100 = 15.0;
    double SunLuxAtCamera = 0.0;
    bool bExposureInitialized = false;
};
