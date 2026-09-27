#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AstroSiteActor.generated.h"
// In-world overlay for a site (see UAstroSiteSubsystem): compass spokes on the ground, the
// Sun's paths as dots on a sky dome around the site, a marker at the Sun's current position,
// and a 1 m gnomon whose real shadow the engine casts. Markers are unlit and scaled to the
// current exposure so they read in daylight and at night. See CLAUDE.md "Sun at a site".

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS(NotPlaceable)
class ASTRORENDERING_API AAstroSiteActor : public AActor
{
    GENERATED_BODY()

public:
    AAstroSiteActor();

    // Radius of the sky dome the paths are drawn on (cm). Close enough to walk around.
    static constexpr double DomeRadiusCm = 3000.0;

    // Place the overlay: ground point and local axes in engine space.
    void SetFrame(const FVector& Ground, const FVector& East, const FVector& North, const FVector& Up);
    // Paths: ENU unit directions per path (0 = today, 1 = June solstice, 2 = December, 3 = equinox).
    void SetPaths(const TArray<TArray<FVector>>& PathsENU);
    void SetSun(const FVector& SunENU, bool bAboveHorizon);
    void SetPathsVisible(bool bVisible);
    // Keeps unlit markers readable at the current exposure.
    void SetExposureEV100(double EV100);

    // Sky directions are drawn around the viewer (like the real sky: no parallax however far
    // you walk from the stick); the compass and gnomon stay on the ground.
    void SetViewer(const FVector& ViewerLocation);

    // Engine position of an ENU offset (cm) from the ground point, or from the viewer (sky).
    FVector ToWorld(const FVector& ENUCm) const { return Ground + East * ENUCm.X + North * ENUCm.Y + Up * ENUCm.Z; }
    FVector ToSky(const FVector& ENUCm) const { return Viewer + East * ENUCm.X + North * ENUCm.Y + Up * ENUCm.Z; }

private:
    void RebuildInstances();
    UMaterialInstanceDynamic* MakeGlow(const FLinearColor& Color);

    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TObjectPtr<USceneComponent> SkyRoot;
    FVector Viewer = FVector::ZeroVector;
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> PathDots;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Compass;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> SunMarker;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Gnomon;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Glows;
    TArray<FLinearColor> GlowColors;

    FVector Ground = FVector::ZeroVector, East = FVector::ForwardVector, North = FVector::RightVector, Up = FVector::UpVector;
    TArray<TArray<FVector>> Paths;
    FVector SunENU = FVector::UpVector;
    bool bSunUp = false;
};
