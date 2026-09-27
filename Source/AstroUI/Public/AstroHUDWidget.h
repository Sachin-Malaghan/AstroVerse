#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BodyFactsRow.h"
#include "AstroHUDWidget.generated.h"
// God-mode time HUD + teaching-mode facts panel, built entirely in C++ (no widget assets).
// Desktop: full-screen overlay with body markers. VR: the same widget in compact mode on a
// world-space wrist panel (no screen-space markers). See CLAUDE.md Phase 11.

class UBorder;
class UButton;
class UCanvasPanel;
class UProgressBar;
class UTextBlock;
class UVerticalBox;

UCLASS()
class ASTROUI_API UAstroHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Compact = VR wrist panel: time bar and facts only.
    void SetCompact(bool bInCompact);

    void SetSelectedBody(FName BodyID);
    FName GetSelectedBody() const { return SelectedBody; }
    // One line under the time bar: "Near Earth - 412 km - 7.7 km/s - co-rotating".
    void SetViewerStatus(const FString& Status);
    void ShowToast(const FString& Message, float Seconds = 4.0f);
    // Multi-line narration panel (guided tour).
    void ShowCaption(const FString& Title, const FString& Text, const FString& Footer);
    void HideCaption();
    void SetFacts(const TMap<FName, FAstroBodyFactsRow>* InFacts) { Facts = InFacts; }
    void ToggleHelp();
    // Local-time display zone for a surface site (vastu / field planning); off = this PC's zone.
    void SetSiteTimeZone(bool bEnabled, double UtcOffsetHours);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void Build();
    void UpdateTimeBar();
    void UpdateMarkers();
    void UpdateFacts();
    void UpdateTravel();
    void UpdateSite();
    void UpdateSkyGuide();
    UTextBlock* MakeText(int32 Size, const FLinearColor& Color = FLinearColor(0.92f, 0.94f, 1.0f, 1.0f));
    UButton* MakeButton(const FString& Label, UTextBlock*& OutLabel);

    UFUNCTION() void OnRewindClicked();
    UFUNCTION() void OnPauseClicked();
    UFUNCTION() void OnSlowerClicked();
    UFUNCTION() void OnFasterClicked();
    UFUNCTION() void OnLiveClicked();

    UPROPERTY() TObjectPtr<UCanvasPanel> Root;
    UPROPERTY() TObjectPtr<UCanvasPanel> MarkerLayer;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> Markers;
    UPROPERTY() TObjectPtr<UTextBlock> DateText;
    UPROPERTY() TObjectPtr<UTextBlock> ScaleText;
    UPROPERTY() TObjectPtr<UTextBlock> PauseLabel;
    UPROPERTY() TObjectPtr<UTextBlock> RewindLabel;
    UPROPERTY() TObjectPtr<UTextBlock> LiveLabel;
    UPROPERTY() TObjectPtr<UBorder> SitePanel;
    UPROPERTY() TObjectPtr<UTextBlock> SiteTitle;
    UPROPERTY() TObjectPtr<UTextBlock> SiteText;
    UPROPERTY() TObjectPtr<UCanvasPanel> SiteLabelLayer;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> SiteLabels;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> SkyLabels;
    UPROPERTY() TObjectPtr<UBorder> CaptionPanel;
    UPROPERTY() TObjectPtr<UTextBlock> CaptionTitle;
    UPROPERTY() TObjectPtr<UTextBlock> CaptionText;
    UPROPERTY() TObjectPtr<UTextBlock> CaptionFooter;
    bool bHasSiteZone = false;
    double SiteZoneHours = 0.0;
    UPROPERTY() TObjectPtr<UTextBlock> StatusText;
    UPROPERTY() TObjectPtr<UTextBlock> Reticle;
    UPROPERTY() TObjectPtr<UBorder> FactsPanel;
    UPROPERTY() TObjectPtr<UTextBlock> FactsTitle;
    UPROPERTY() TObjectPtr<UTextBlock> FactsSummary;
    UPROPERTY() TObjectPtr<UTextBlock> FactsStats;
    UPROPERTY() TObjectPtr<UTextBlock> ToastText;
    UPROPERTY() TObjectPtr<UTextBlock> TravelText;
    UPROPERTY() TObjectPtr<UProgressBar> TravelBar;
    UPROPERTY() TObjectPtr<UTextBlock> HelpText;

    const TMap<FName, FAstroBodyFactsRow>* Facts = nullptr;
    FName SelectedBody;
    float ToastRemaining = 0.0f;
    bool bCompact = false;
    bool bHelpVisible = false;
};
