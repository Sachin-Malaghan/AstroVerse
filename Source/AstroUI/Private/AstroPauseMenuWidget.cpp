// See CLAUDE.md Phase 11.
#include "AstroPauseMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "TravelSystem.h"

namespace
{
    UTextBlock* NewText(UWidgetTree* Tree, int32 Size, const FLinearColor& Color = FLinearColor(0.92f, 0.94f, 1.0f, 1.0f))
    {
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
        Text->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
        Text->SetColorAndOpacity(FSlateColor(Color));
        return Text;
    }
}

void UAstroPauseMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!WidgetTree->RootWidget)
    {
        Build();
    }
    Refresh();
}

UButton* UAstroPauseMenuWidget::AddButton(UVerticalBox* Box, const FString& Label, UTextBlock** OutText)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    Button->SetBackgroundColor(FLinearColor(0.1f, 0.12f, 0.2f, 0.9f));
    UTextBlock* Text = NewText(WidgetTree, 18);
    Text->SetText(FText::FromString(Label));
    Button->AddChild(Text);
    Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(0, 4));
    if (OutText)
    {
        *OutText = Text;
    }
    return Button;
}

void UAstroPauseMenuWidget::Build()
{
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(460.0f);
    UOverlaySlot* SizeSlot = Root->AddChildToOverlay(Size);
    SizeSlot->SetHorizontalAlignment(HAlign_Center);
    SizeSlot->SetVerticalAlignment(VAlign_Center);
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.04f, 0.9f));
    Panel->SetPadding(FMargin(24));
    Size->AddChild(Panel);
    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->AddChild(Box);

    UTextBlock* Title = NewText(WidgetTree, 30, FLinearColor(1.0f, 0.78f, 0.45f, 1.0f));
    Title->SetText(FText::FromString(TEXT("AstroVerse")));
    Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(0, 0, 0, 12));

    UTextBlock* Style = nullptr;
    UTextBlock* Clock = nullptr;
    AddButton(Box, TEXT("Resume"))->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnResumeClicked);
    AddButton(Box, TEXT(""), &Style)->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnStyleClicked);
    AddButton(Box, TEXT(""), &Clock)->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnClockClicked);
    StyleLabel = Style;
    ClockLabel = Clock;
    AddButton(Box, TEXT("Exposure  +"))->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnBrighterClicked);
    ExposureLabel = NewText(WidgetTree, 14, FLinearColor(0.62f, 0.68f, 0.78f, 1.0f));
    Box->AddChildToVerticalBox(ExposureLabel)->SetHorizontalAlignment(HAlign_Center);
    AddButton(Box, TEXT("Exposure  -"))->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnDarkerClicked);
    AddButton(Box, TEXT("Credits"))->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnCreditsClicked);
    AddButton(Box, TEXT("Quit"))->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnQuitClicked);

    CreditsText = NewText(WidgetTree, 12, FLinearColor(0.75f, 0.8f, 0.9f, 1.0f));
    CreditsText->SetAutoWrapText(true);
    CreditsText->SetText(FText::FromString(TEXT(
        "Planet textures by Solar System Scope (solarsystemscope.com), CC BY 4.0\n"
        "Planetary orbital elements: JPL, Standish, \"Keplerian Elements for Approximate Positions of the Major Planets\"\n"
        "Gravitational parameters: JPL DE440     Rotation models: IAU WGCCRE\n"
        "Atmosphere coefficients: Bruneton, \"Precomputed Atmospheric Scattering\" (2017)\n"
        "Crater depths: Pike (1974)     Solar motion: Schonrich, Binney & Dehnen (2010)\n"
        "Built with Unreal Engine 5.8")));
    CreditsText->SetVisibility(ESlateVisibility::Collapsed);
    Box->AddChildToVerticalBox(CreditsText)->SetPadding(FMargin(0, 12, 0, 0));
}

void UAstroPauseMenuWidget::Refresh()
{
    const UAstroTravelSettings* Settings = UAstroTravelSettings::Get();
    StyleLabel->SetText(FText::FromString(Settings->Style == EAstroTravelStyle::PilotedShip ? TEXT("Travel:  Piloted ship") : TEXT("Travel:  Cinematic warp")));
    ClockLabel->SetText(FText::FromString(Settings->Clock == EAstroClockDuringTravel::Pause ? TEXT("Clock during travel:  Paused") : TEXT("Clock during travel:  Keeps running")));
    float Exposure = 0.0f;
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("astro.Render.ExposureCompensation")))
    {
        Exposure = CVar->GetFloat();
    }
    ExposureLabel->SetText(FText::FromString(FString::Printf(TEXT("Exposure compensation  %+.1f EV"), Exposure)));
}

void UAstroPauseMenuWidget::OnResumeClicked()
{
    OnResume.ExecuteIfBound();
}

void UAstroPauseMenuWidget::OnStyleClicked()
{
    UAstroTravelSettings* Settings = UAstroTravelSettings::Get();
    Settings->Style = Settings->Style == EAstroTravelStyle::PilotedShip ? EAstroTravelStyle::CinematicWarp : EAstroTravelStyle::PilotedShip;
    Settings->Save();
    Refresh();
}

void UAstroPauseMenuWidget::OnClockClicked()
{
    UAstroTravelSettings* Settings = UAstroTravelSettings::Get();
    Settings->Clock = Settings->Clock == EAstroClockDuringTravel::Pause ? EAstroClockDuringTravel::KeepRunning : EAstroClockDuringTravel::Pause;
    Settings->Save();
    Refresh();
}

void UAstroPauseMenuWidget::OnBrighterClicked()
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("astro.Render.ExposureCompensation")))
    {
        CVar->Set(CVar->GetFloat() + 0.5f, ECVF_SetByCode);
    }
    Refresh();
}

void UAstroPauseMenuWidget::OnDarkerClicked()
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("astro.Render.ExposureCompensation")))
    {
        CVar->Set(CVar->GetFloat() - 0.5f, ECVF_SetByCode);
    }
    Refresh();
}

void UAstroPauseMenuWidget::OnCreditsClicked()
{
    CreditsText->SetVisibility(CreditsText->IsVisible() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UAstroPauseMenuWidget::OnQuitClicked()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
