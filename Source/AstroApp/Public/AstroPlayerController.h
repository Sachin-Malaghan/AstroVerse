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
    virtual void PlayerTick(float DeltaTime) override;

    const UAstroInputActions* GetInputActions() const { return InputActions; }

    UFUNCTION(BlueprintPure, Category = "Astro|Selection")
    FName GetSelectedBody() const { return SelectedBody; }

    UFUNCTION(BlueprintCallable, Category = "Astro|Selection")
    void SelectBody(FName BodyID);

    // Guided tour (F2): Sun -> planets -> the whole system -> the Milky Way.
    void ToggleTour();

    // Stand at a place on a body (GPS-style latitude, east longitude) with its sun data and
    // sun-path overlay; UtcOffsetHours sets the local clock (e.g. 5.5 for India).
    void GoToSite(FName Body, double LatDeg, double LonDeg, double UtcOffsetHours, const FString& Name);
    void ClearSite();

    // Body nearest the view center within MaxAngleDeg (or covering the center).
    UFUNCTION(BlueprintPure, Category = "Astro|Selection")
    FName FindBodyUnderReticle(float MaxAngleDeg = 4.0f) const;

    // Body under a screen position (px): inside its disc, or its centre within MaxPixels.
    UFUNCTION(BlueprintPure, Category = "Astro|Selection")
    FName FindBodyAtScreen(FVector2D ScreenPosition, float MaxPixels = 18.0f) const;

    // Free cursor + drag-to-look (the default), restored after menus.
    void ApplyGameInputMode();

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
    void OnHelpAction(const FInputActionValue& Value);
    void OnSelectReleased(const FInputActionValue& Value);
    void OnLockAction(const FInputActionValue& Value);
    FVector2D PressPosition = FVector2D::ZeroVector;
    bool bPressWithCursor = false;
    double LastClickTime = -1.0;
    FName LastClickBody;
    void OnFocusAction(const FInputActionValue& Value);
    void OnHomeAction(const FInputActionValue& Value);
    void OnGoLiveAction(const FInputActionValue& Value);
    void OnTourAction(const FInputActionValue& Value);
    void OnTourNextAction(const FInputActionValue& Value);
    void OnMilkyWayGuideAction(const FInputActionValue& Value);
    void OnFaceSunAction(const FInputActionValue& Value);
    void HandleUICommand(FName Command);
    void FinishGoToSite();
    struct FPendingSite { FName Body; double Lat = 0, Lon = 0, Zone = 0; FString Name; };
    TOptional<FPendingSite> PendingSite;

    UPROPERTY(Transient)
    TObjectPtr<UAstroInputActions> InputActions;

    FName SelectedBody;
};
