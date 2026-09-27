#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AstroGalaxyActor.generated.h"
// The galaxy scale-domain's scene: a raymarched Milky Way volume, a "you are here" Sun
// marker with its galactic velocity arrow, and a fixed-exposure post-process. Its own unit
// system (GalaxyCmPerLightYear) — never shares coordinates with the solar system.
// See CLAUDE.md Phase 10.

class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(NotPlaceable)
class ASTROGALAXY_API AAstroGalaxyActor : public AActor
{
    GENERATED_BODY()

public:
    AAstroGalaxyActor();

    virtual void Tick(float DeltaSeconds) override;

    // Galaxy-domain scale: engine centimeters per light-year.
    static constexpr double CmPerLightYear = 10.0;
    static constexpr double LightYearsPerKpc = 3261.56;
    static constexpr double CmPerKpc = CmPerLightYear * LightYearsPerKpc;
    // The Sun in the galactocentric frame (kpc; +Z north galactic pole, Sun on -X).
    static FVector SunKpc() { return FVector(-8.2, 0.0, 0.021); }
    // Galaxy frame (right-handed) -> engine space offset from the actor (left-handed: flip Y).
    static FVector KpcToEngine(const FVector& Kpc) { return FVector(Kpc.X, -Kpc.Y, Kpc.Z) * CmPerKpc; }

    UPROPERTY(VisibleAnywhere, Category = "Astro|Galaxy")
    TObjectPtr<UStaticMeshComponent> Volume;

    UPROPERTY(VisibleAnywhere, Category = "Astro|Galaxy")
    TObjectPtr<UStaticMeshComponent> SunMarker;

    UPROPERTY(VisibleAnywhere, Category = "Astro|Galaxy")
    TObjectPtr<UStaticMeshComponent> VelocityArrow;

    UPROPERTY(VisibleAnywhere, Category = "Astro|Galaxy")
    TObjectPtr<UTextRenderComponent> SunLabel;

    UPROPERTY(VisibleAnywhere, Category = "Astro|Galaxy")
    TObjectPtr<UPostProcessComponent> PostProcess;

    void SetDomainActive(bool bActive);

    // Engine location of the Sun marker / galactic center.
    FVector GetSunLocation() const { return GetActorLocation() + KpcToEngine(SunKpc()); }
    FVector GetCenterLocation() const { return GetActorLocation(); }

private:
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> VolumeMID;
};
