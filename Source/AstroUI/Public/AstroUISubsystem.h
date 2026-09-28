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
DECLARE_MULTICAST_DELEGATE_FourParams(FOnAstroUISiteRequest, double, double, double, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAstroUICommand, FName);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAstroUIBodyRequested, FName);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAstroUIMissionRequested, FName /*Destination*/, bool /*bPilot*/, FName /*Vehicle*/);
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
    void ShowToast(const FString& Message, float Seconds = 5.0f);
    void ShowCaption(const FString& Title, const FString& Text, const FString& Footer);
    void HideCaption();
    UAstroHUDWidget* GetHUDWidget() const { return HUD; }
    void ToggleHUD();
    void ToggleHelp();
    void ToggleMenu();
    bool IsMenuOpen() const { return bMenuOpen; }
    // Local-time zone for the HUD clock (a site's), or this computer's.
    void SetSiteTimeZone(bool bEnabled, double UtcOffsetHours);

    // Menu requests for the application layer (AstroApp binds these).
    FOnAstroUISiteRequest OnSiteRequested;
    FOnAstroUICommand OnCommand;
    // Body list clicks: select and fly into orbit.
    FOnAstroUIBodyRequested OnBodyRequested;
    FOnAstroUIMissionRequested OnMissionRequested;

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
