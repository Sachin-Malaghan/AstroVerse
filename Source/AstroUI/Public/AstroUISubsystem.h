#pragma once
#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "BodyFactsRow.h"
#include "AstroUISubsystem.generated.h"
// Owns the HUD and pause menu for a local player and turns engine events into on-screen
// notices (a 4th lock demoting the oldest, full physics paused at high time scales, travel,
// scale-domain changes) — nothing fails silently. AstroApp creates it once the player
// controller exists and feeds it the selection and viewer status. See CLAUDE.md Phase 11.

class UAstroHUDWidget;
class UAstroPauseMenuWidget;
enum class EAstroScaleDomain : uint8;

UCLASS()
class ASTROUI_API UAstroUISubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    static UAstroUISubsystem* Get(const APlayerController* PC);

    // bVR: the HUD goes on a world-space panel (the caller hosts GetHUD() in a widget component).
    void Create(APlayerController* PC, bool bVR);

    UAstroHUDWidget* GetHUD() const { return HUD; }

    void SetSelectedBody(FName BodyID);
    void SetViewerStatus(const FString& Status);
    void ShowToast(const FString& Message);
    void ToggleHUD();
    void ToggleHelp();
    void ToggleMenu();
    bool IsMenuOpen() const { return bMenuOpen; }

private:
    void LoadFacts();

    UFUNCTION() void HandleLockEvicted(FName Evicted, FName NewBody);
    UFUNCTION() void HandleFullPhysics(FName BodyID, bool bAvailable);
    UFUNCTION() void HandleTravelStarted(FName BodyID);
    UFUNCTION() void HandleTravelArrived(FName BodyID);
    UFUNCTION() void HandleDomainChanged(EAstroScaleDomain Domain);

    UPROPERTY() TObjectPtr<UAstroHUDWidget> HUD;
    UPROPERTY() TObjectPtr<UAstroPauseMenuWidget> Menu;
    TWeakObjectPtr<APlayerController> Controller;
    TMap<FName, FAstroBodyFactsRow> Facts;
    bool bVRMode = false;
    bool bMenuOpen = false;
    bool bHUDHidden = false;
};
