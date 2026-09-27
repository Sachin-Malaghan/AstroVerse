#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AstroWarpEffects.generated.h"
// Presentation of a transit: a radial streak post-process that swells mid-warp, a
// streaking tunnel around the viewer, and (piloted style) a placeholder cockpit frame that
// banks with the player's steering. Driven by UAstroTravelSubsystem. See CLAUDE.md Phase 9.

class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UStaticMeshComponent;

UCLASS(NotPlaceable)
class ASTROTRAVEL_API AAstroWarpEffects : public AActor
{
    GENERATED_BODY()

public:
    AAstroWarpEffects();

    void Begin(AActor* Viewer, bool bPiloted);
    void Update(float Intensity, float DistanceMeters, const FVector2D& Steer);
    void End();

    UPROPERTY(VisibleAnywhere, Category = "Astro|Warp")
    TObjectPtr<UPostProcessComponent> PostProcess;

    UPROPERTY(VisibleAnywhere, Category = "Astro|Warp")
    TObjectPtr<UStaticMeshComponent> Tunnel;

    // Cockpit frame pieces (piloted style).
    UPROPERTY(VisibleAnywhere, Category = "Astro|Warp")
    TArray<TObjectPtr<UStaticMeshComponent>> CockpitParts;

private:
    UStaticMeshComponent* AddPart(const TCHAR* Name, const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator);

    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PostMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> TunnelMID;
    TWeakObjectPtr<AActor> ViewerActor;
    bool bPiloted = false;
};
