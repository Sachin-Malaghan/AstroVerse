#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AstroPlayerController.generated.h"
// Thin wiring for global controls: time (god-mode clock), selecting and locking bodies,
// scale-domain and HUD toggles, travel. Movement belongs to the pawns. See CLAUDE.md Phase 8.

class UAstroInputActions;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAstroSelectionChanged, FName, BodyID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAstroUIRequest);

UCLASS()
class ASTROAPP_API AAstroPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    const UAstroInputActions* GetInputActions() const { return InputActions; }

    UFUNCTION(BlueprintPure, Category = "Astro|Selection")
    FName GetSelectedBody() const { return SelectedBody; }

    UFUNCTION(BlueprintCallable, Category = "Astro|Selection")
    void SelectBody(FName BodyID);

    // Body nearest the view center within MaxAngleDeg (or covering the center).
    UFUNCTION(BlueprintPure, Category = "Astro|Selection")
    FName FindBodyUnderReticle(float MaxAngleDeg = 4.0f) const;

    UPROPERTY(BlueprintAssignable, Category = "Astro|Selection")
    FOnAstroSelectionChanged OnSelectionChanged;

    // Raised for the HUD layer (Phase 11) and travel (Phase 9) to act on.
    UPROPERTY(BlueprintAssignable, Category = "Astro|UI")
    FOnAstroUIRequest OnToggleHUD;

    UPROPERTY(BlueprintAssignable, Category = "Astro|UI")
    FOnAstroUIRequest OnMenu;

    UPROPERTY(BlueprintAssignable, Category = "Astro|Travel")
    FOnAstroUIRequest OnTravelRequested;

private:
    void EnsureInputActions();
    void OnSelect(const FInputActionValue& Value);
    void OnTimeFaster(const FInputActionValue& Value);
    void OnTimeSlower(const FInputActionValue& Value);
    void OnTimePause(const FInputActionValue& Value);
    void OnTimeRewind(const FInputActionValue& Value);
    void OnToggleGalaxy(const FInputActionValue& Value);
    void OnToggleHUDAction(const FInputActionValue& Value);
    void OnMenuAction(const FInputActionValue& Value);
    void OnTravelAction(const FInputActionValue& Value);

    UPROPERTY(Transient)
    TObjectPtr<UAstroInputActions> InputActions;

    FName SelectedBody;
};
