#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AstroPauseMenuWidget.generated.h"
// Pause menu: resume, travel style and clock-during-travel (user settings), exposure,
// credits (the CC BY attribution the planet textures require), quit. See CLAUDE.md Phase 11.
// Also: the guided tour, return home, and "Sun at a site" (latitude / longitude / UTC offset).

class UTextBlock;
class UVerticalBox;
class UEditableTextBox;

DECLARE_DELEGATE(FOnAstroMenuClosed);
// Lat (deg, N+), lon (deg, E+), UTC offset (h), name.
DECLARE_DELEGATE_FourParams(FOnAstroSiteRequest, double, double, double, const FString&);
// "Tour", "Home", "ClearSite".
DECLARE_DELEGATE_OneParam(FOnAstroMenuCommand, FName);

UCLASS()
class ASTROUI_API UAstroPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    FOnAstroMenuClosed OnResume;
    FOnAstroSiteRequest OnSiteRequest;
    FOnAstroMenuCommand OnCommand;

protected:
    virtual void NativeOnInitialized() override;

private:
    void Build();
    void Refresh();
    class UButton* AddButton(UVerticalBox* Box, const FString& Label, UTextBlock** OutText = nullptr);

    UFUNCTION() void OnResumeClicked();
    UFUNCTION() void OnStyleClicked();
    UFUNCTION() void OnClockClicked();
    UFUNCTION() void OnBrighterClicked();
    UFUNCTION() void OnDarkerClicked();
    UFUNCTION() void OnCreditsClicked();
    UFUNCTION() void OnQuitClicked();
    UFUNCTION() void OnTourClicked();
    UFUNCTION() void OnMissionClicked();
    UFUNCTION() void OnSkyClicked();
    UFUNCTION() void OnSunClicked();
    UFUNCTION() void OnHomeClicked();
    UFUNCTION() void OnSiteClicked();
    UFUNCTION() void OnClearSiteClicked();
    UEditableTextBox* AddField(class UHorizontalBox* Row, const FString& Hint, const FString& Default, float Width);

    UPROPERTY() TObjectPtr<UTextBlock> StyleLabel;
    UPROPERTY() TObjectPtr<UTextBlock> ClockLabel;
    UPROPERTY() TObjectPtr<UTextBlock> ExposureLabel;
    UPROPERTY() TObjectPtr<UTextBlock> SkyLabel;
    UPROPERTY() TObjectPtr<UTextBlock> SunLabel;
    UPROPERTY() TObjectPtr<UTextBlock> CreditsText;
    UPROPERTY() TObjectPtr<UEditableTextBox> LatBox;
    UPROPERTY() TObjectPtr<UEditableTextBox> LonBox;
    UPROPERTY() TObjectPtr<UEditableTextBox> ZoneBox;
    UPROPERTY() TObjectPtr<UEditableTextBox> NameBox;
    UPROPERTY() TObjectPtr<UTextBlock> SiteStatus;
};
