#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AstroPauseMenuWidget.generated.h"
// Pause menu: resume, travel style and clock-during-travel (user settings), exposure,
// credits (the CC BY attribution the planet textures require), quit. See CLAUDE.md Phase 11.

class UTextBlock;
class UVerticalBox;

DECLARE_DELEGATE(FOnAstroMenuClosed);

UCLASS()
class ASTROUI_API UAstroPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    FOnAstroMenuClosed OnResume;

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

    UPROPERTY() TObjectPtr<UTextBlock> StyleLabel;
    UPROPERTY() TObjectPtr<UTextBlock> ClockLabel;
    UPROPERTY() TObjectPtr<UTextBlock> ExposureLabel;
    UPROPERTY() TObjectPtr<UTextBlock> CreditsText;
};
