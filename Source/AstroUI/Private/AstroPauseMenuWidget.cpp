// See CLAUDE.md Phase 11.
#include "AstroPauseMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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
#include "AstroSpaceEnvironment.h"

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
    AddButton(Box, TEXT("Guided tour: Sun to the Milky Way   (F2)"))->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnTourClicked);
    AddButton(Box, TEXT("Return home - Earth   (Home)"))->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnHomeClicked);

    // Sun at a site: stand on a place on Earth and see its real sky.
    UTextBlock* SiteHeader = NewText(WidgetTree, 16, FLinearColor(1.0f, 0.78f, 0.45f, 1.0f));
    SiteHeader->SetText(FText::FromString(TEXT("Sun at a site  (latitude N+, longitude E+, UTC offset in hours)")));
    SiteHeader->SetAutoWrapText(true);
    Box->AddChildToVerticalBox(SiteHeader)->SetPadding(FMargin(0, 12, 0, 4));
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    Box->AddChildToVerticalBox(Row);
    LatBox = AddField(Row, TEXT("latitude"), TEXT("28.6139"), 90.0f);
    LonBox = AddField(Row, TEXT("longitude"), TEXT("77.2090"), 90.0f);
    ZoneBox = AddField(Row, TEXT("UTC +h"), TEXT("5.5"), 60.0f);
    NameBox = AddField(Row, TEXT("name"), TEXT("New Delhi"), 150.0f);
    AddButton(Box, TEXT("Go to site"))->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnSiteClicked);
    AddButton(Box, TEXT("Clear site"))->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnClearSiteClicked);
    SiteStatus = NewText(WidgetTree, 12, FLinearColor(0.62f, 0.68f, 0.78f, 1.0f));
    Box->AddChildToVerticalBox(SiteStatus)->SetPadding(FMargin(0, 2, 0, 8));
    AddButton(Box, TEXT(""), &Style)->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnStyleClicked);
    AddButton(Box, TEXT(""), &Clock)->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnClockClicked);
    StyleLabel = Style;
    ClockLabel = Clock;
    UTextBlock* Sky = nullptr;
    AddButton(Box, TEXT(""), &Sky)->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnSkyClicked);
    SkyLabel = Sky;
    UTextBlock* SunText = nullptr;
    AddButton(Box, TEXT(""), &SunText)->OnClicked.AddDynamic(this, &UAstroPauseMenuWidget::OnSunClicked);
    SunLabel = SunText;
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
    SkyLabel->SetText(FText::FromString(AAstroSpaceEnvironment::GetMilkyWayMode() != 0
        ? TEXT("Milky Way:  Enhanced (easy to see)") : TEXT("Milky Way:  Realistic (as photographed)")));
    if (IConsoleVariable* SunLook = IConsoleManager::Get().FindConsoleVariable(TEXT("astro.Sun.Look")))
    {
        static const TCHAR* Names[] = { TEXT("Sun:  Physical (real brightness)"), TEXT("Sun:  Natural (white, as from space)"), TEXT("Sun:  Stylized (Solar System Scope)") };
        SunLabel->SetText(FText::FromString(Names[FMath::Clamp(SunLook->GetInt(), 0, 2)]));
    }
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

UEditableTextBox* UAstroPauseMenuWidget::AddField(UHorizontalBox* Row, const FString& Hint, const FString& Default, float Width)
{
    UEditableTextBox* Field = WidgetTree->ConstructWidget<UEditableTextBox>();
    Field->SetHintText(FText::FromString(Hint));
    Field->SetText(FText::FromString(Default));
    UHorizontalBoxSlot* FieldSlot = Row->AddChildToHorizontalBox(Field);
    FieldSlot->SetPadding(FMargin(0, 0, 4, 0));
    FSlateChildSize Size(ESlateSizeRule::Fill);
    Size.Value = Width; // relative share of the row
    FieldSlot->SetSize(Size);
    return Field;
}

void UAstroPauseMenuWidget::OnSunClicked()
{
    if (IConsoleVariable* SunLook = IConsoleManager::Get().FindConsoleVariable(TEXT("astro.Sun.Look")))
    {
        SunLook->Set((SunLook->GetInt() + 1) % 3, ECVF_SetByCode);
    }
    Refresh();
}

void UAstroPauseMenuWidget::OnSkyClicked()
{
    AAstroSpaceEnvironment::SetMilkyWayMode(AAstroSpaceEnvironment::GetMilkyWayMode() != 0 ? 0 : 1);
    Refresh();
}

void UAstroPauseMenuWidget::OnTourClicked()
{
    OnCommand.ExecuteIfBound(TEXT("Tour"));
}

void UAstroPauseMenuWidget::OnHomeClicked()
{
    OnCommand.ExecuteIfBound(TEXT("Home"));
}

void UAstroPauseMenuWidget::OnClearSiteClicked()
{
    OnCommand.ExecuteIfBound(TEXT("ClearSite"));
}

void UAstroPauseMenuWidget::OnSiteClicked()
{
    auto Parse = [](const UEditableTextBox* Field, double Min, double Max, double& Out)
    {
        const FString Text = Field->GetText().ToString().TrimStartAndEnd();
        if (Text.IsEmpty() || !Text.IsNumeric())
        {
            return false;
        }
        Out = FCString::Atod(*Text);
        return Out >= Min && Out <= Max;
    };
    double Lat = 0.0, Lon = 0.0, Zone = 0.0;
    if (!Parse(LatBox, -90.0, 90.0, Lat) || !Parse(LonBox, -180.0, 360.0, Lon) || !Parse(ZoneBox, -14.0, 14.0, Zone))
    {
        SiteStatus->SetText(FText::FromString(TEXT("Latitude -90..90, longitude -180..180 (east positive), UTC offset -14..14 hours.")));
        return;
    }
    SiteStatus->SetText(FText::FromString(FString::Printf(TEXT("Going to %.4f, %.4f ..."), Lat, Lon)));
    OnSiteRequest.ExecuteIfBound(Lat, Lon, Zone, NameBox->GetText().ToString());
}

void UAstroPauseMenuWidget::OnQuitClicked()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
