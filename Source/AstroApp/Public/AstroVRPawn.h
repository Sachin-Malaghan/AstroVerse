#pragma once
#include "CoreMinimal.h"
#include "AstroPawnBase.h"
#include "AstroVRPawn.generated.h"
// VR pawn — room-scale/teleport locomotion. See CLAUDE.md Phase 8.
// The actor is the tracking origin (floor level); the camera follows the HMD. Walking:
// real steps in the room plus teleport (aim with the right stick) and snap turn. Flying:
// left stick moves along the head's gaze, with a comfort vignette while moving.

class UCameraComponent;
class UMotionControllerComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UWidgetInteractionComponent;
struct FInputActionValue;

UCLASS()
class ASTROAPP_API AAstroVRPawn : public AAstroPawnBase
{
    GENERATED_BODY()

public:
    AAstroVRPawn();

    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|VR")
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|VR")
    TObjectPtr<UMotionControllerComponent> LeftController;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|VR")
    TObjectPtr<UMotionControllerComponent> RightController;

    // Diegetic UI: the god-mode HUD on the left wrist, a laser pointer on the right hand.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|VR")
    TObjectPtr<UWidgetComponent> WristPanel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|VR")
    TObjectPtr<UWidgetInteractionComponent> Pointer;

    // Marker at the teleport target while aiming.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|VR")
    TObjectPtr<UStaticMeshComponent> TeleportMarker;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astro|VR")
    float SnapTurnDegrees = 30.0f;

    // Vignette strength while moving (0 = off).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astro|VR")
    float ComfortVignette = 0.6f;

protected:
    virtual FQuat GetMovementBasis() const override;
    virtual void ApplyLookInput(float DeltaSeconds) override;
    virtual bool UsesHeadTracking() const override { return true; }

private:
    void OnSnapTurn(const FInputActionValue& Value);
    void OnPointerPressed(const FInputActionValue& Value);
    void OnPointerReleased(const FInputActionValue& Value);
    virtual void BeginPlay() override;
    void OnTeleportStarted(const FInputActionValue& Value);
    void OnTeleportReleased(const FInputActionValue& Value);
    bool TraceTeleport(FVector& OutLocation) const;

    bool bSnapArmed = true;
    bool bAimingTeleport = false;
};
