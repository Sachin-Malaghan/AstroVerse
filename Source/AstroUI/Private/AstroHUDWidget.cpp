// See CLAUDE.md Phase 11.
#include "AstroHUDWidget.h"
#include "AstroActivationSubsystem.h"
#include "AstroSimulationSubsystem.h"
#include "AstroTravelSubsystem.h"
#include "AstroUIFormat.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "Math/AstroConstants.h"
#include "Styling/CoreStyle.h"
#include "TimeController.h"

namespace
{
    const FLinearColor PanelColor(0.02f, 0.025f, 0.04f, 0.62f);
    const FLinearColor Accent(1.0f, 0.78f, 0.45f, 1.0f);
    const FLinearColor Dim(0.62f, 0.68f, 0.78f, 1.0f);

    UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
    {
        UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
        Slot->SetAnchors(Anchors);
        Slot->SetAlignment(Alignment);
        Slot->SetPosition(Position);
        Slot->SetAutoSize(true);
        return Slot;
    }
}

void UAstroHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!Root)
    {
        Build();
    }
}

UTextBlock* UAstroHUDWidget::MakeText(int32 Size, const FLinearColor& Color)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
    Text->SetColorAndOpacity(FSlateColor(Color));
    Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
    Text->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.8f));
    return Text;
}

UButton* UAstroHUDWidget::MakeButton(const FString& Label, UTextBlock*& OutLabel)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    Button->SetBackgroundColor(FLinearColor(0.1f, 0.12f, 0.18f, 0.8f));
    OutLabel = MakeText(14);
    OutLabel->SetText(FText::FromString(Label));
    Button->AddChild(OutLabel);
    return Button;
}

void UAstroHUDWidget::Build()
{
    Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // Body markers under everything else.
    MarkerLayer = WidgetTree->ConstructWidget<UCanvasPanel>();
    UCanvasPanelSlot* MarkerSlot = Root->AddChildToCanvas(MarkerLayer);
    MarkerSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    MarkerSlot->SetOffsets(FMargin(0));

    // --- Time bar (top center).
    UBorder* TimeBar = WidgetTree->ConstructWidget<UBorder>();
    TimeBar->SetBrushColor(PanelColor);
    TimeBar->SetPadding(FMargin(12, 6));
    UHorizontalBox* TimeRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    TimeBar->AddChild(TimeRow);
    UTextBlock* Unused = nullptr;
    UTextBlock* RewindText = nullptr;
    UTextBlock* PauseText = nullptr;
    UButton* Rewind = MakeButton(TEXT("<<"), RewindText);
    UButton* Slower = MakeButton(TEXT(" - "), Unused);
    UButton* PauseButton = MakeButton(TEXT(" || "), PauseText);
    RewindLabel = RewindText;
    PauseLabel = PauseText;
    UButton* Faster = MakeButton(TEXT(" + "), Unused);
    Rewind->OnClicked.AddDynamic(this, &UAstroHUDWidget::OnRewindClicked);
    Slower->OnClicked.AddDynamic(this, &UAstroHUDWidget::OnSlowerClicked);
    PauseButton->OnClicked.AddDynamic(this, &UAstroHUDWidget::OnPauseClicked);
    Faster->OnClicked.AddDynamic(this, &UAstroHUDWidget::OnFasterClicked);
    for (UButton* Button : { Rewind, Slower, PauseButton, Faster })
    {
        TimeRow->AddChildToHorizontalBox(Button)->SetPadding(FMargin(2, 0));
    }
    DateText = MakeText(18);
    ScaleText = MakeText(16, Accent);
    TimeRow->AddChildToHorizontalBox(DateText)->SetPadding(FMargin(16, 2, 12, 0));
    TimeRow->AddChildToHorizontalBox(ScaleText)->SetPadding(FMargin(4, 4, 4, 0));
    Place(Root, TimeBar, FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0, 16));

    // --- Status line (under the time bar).
    StatusText = MakeText(14, Dim);
    Place(Root, StatusText, FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0, 64));

    // --- Reticle.
    Reticle = MakeText(18, FLinearColor(1, 1, 1, 0.55f));
    Reticle->SetText(FText::FromString(TEXT("+")));
    Place(Root, Reticle, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

    // --- Facts panel (right).
    FactsPanel = WidgetTree->ConstructWidget<UBorder>();
    FactsPanel->SetBrushColor(PanelColor);
    FactsPanel->SetPadding(FMargin(16));
    UVerticalBox* FactsBox = WidgetTree->ConstructWidget<UVerticalBox>();
    FactsPanel->AddChild(FactsBox);
    FactsTitle = MakeText(24, Accent);
    FactsSummary = MakeText(13);
    FactsSummary->SetWrapTextAt(330.0f);
    FactsStats = MakeText(13, FLinearColor(0.85f, 0.88f, 0.95f, 1.0f));
    FactsStats->SetWrapTextAt(330.0f);
    FactsBox->AddChildToVerticalBox(FactsTitle)->SetPadding(FMargin(0, 0, 0, 6));
    FactsBox->AddChildToVerticalBox(FactsSummary)->SetPadding(FMargin(0, 0, 0, 10));
    FactsBox->AddChildToVerticalBox(FactsStats);
    // Fixed width: canvas auto-size gives wrapped text no width to wrap against.
    USizeBox* FactsSize = WidgetTree->ConstructWidget<USizeBox>();
    FactsSize->SetWidthOverride(362.0f);
    FactsSize->AddChild(FactsPanel);
    Place(Root, FactsSize, FAnchors(1.0f, 0.5f), FVector2D(1.0f, 0.5f), FVector2D(-20, 0));
    FactsPanel->SetVisibility(ESlateVisibility::Collapsed);

    // --- Travel progress + toasts (bottom center).
    UVerticalBox* Bottom = WidgetTree->ConstructWidget<UVerticalBox>();
    TravelText = MakeText(16, Accent);
    TravelBar = WidgetTree->ConstructWidget<UProgressBar>();
    TravelBar->SetFillColorAndOpacity(Accent);
    ToastText = MakeText(15);
    Bottom->AddChildToVerticalBox(TravelText)->SetHorizontalAlignment(HAlign_Center);
    UVerticalBoxSlot* BarSlot = Bottom->AddChildToVerticalBox(TravelBar);
    BarSlot->SetPadding(FMargin(0, 4, 0, 10));
    Bottom->AddChildToVerticalBox(ToastText)->SetHorizontalAlignment(HAlign_Center);
    UCanvasPanelSlot* BottomSlot = Place(Root, Bottom, FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0, -40));
    BottomSlot->SetAutoSize(false);
    BottomSlot->SetSize(FVector2D(520, 90));
    TravelText->SetVisibility(ESlateVisibility::Collapsed);
    TravelBar->SetVisibility(ESlateVisibility::Collapsed);

    // --- Help (F1 / H toggles the whole HUD; the help line is always there).
    HelpText = MakeText(12, Dim);
    HelpText->SetText(FText::FromString(TEXT(
        "WASD / Space / C  fly     Mouse  look     Q E  roll     Wheel  speed     Shift  boost\n"
        "Click  select (again: lock)     T  travel     G  land / take off     M  galaxy\n"
        "[ ]  time slower / faster     P  pause     R  rewind     H  hide HUD     Esc  menu")));
    Place(Root, HelpText, FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(20, -40));
    HelpText->SetVisibility(ESlateVisibility::Collapsed);
    UTextBlock* HelpHint = MakeText(12, Dim);
    HelpHint->SetText(FText::FromString(TEXT("F1  controls")));
    Place(Root, HelpHint, FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(20, -16));
}

void UAstroHUDWidget::SetCompact(bool bInCompact)
{
    bCompact = bInCompact;
    if (MarkerLayer)
    {
        MarkerLayer->SetVisibility(bCompact ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
        Reticle->SetVisibility(bCompact ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
        HelpText->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UAstroHUDWidget::ToggleHelp()
{
    bHelpVisible = !bHelpVisible;
    HelpText->SetVisibility(bHelpVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UAstroHUDWidget::SetSelectedBody(FName BodyID)
{
    SelectedBody = BodyID;
    FactsPanel->SetVisibility(BodyID.IsNone() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UAstroHUDWidget::SetViewerStatus(const FString& Status)
{
    StatusText->SetText(FText::FromString(Status));
}

void UAstroHUDWidget::ShowToast(const FString& Message, float Seconds)
{
    ToastText->SetText(FText::FromString(Message));
    ToastText->SetRenderOpacity(1.0f);
    ToastRemaining = Seconds;
}

void UAstroHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UpdateTimeBar();
    if (!bCompact)
    {
        UpdateMarkers();
    }
    UpdateFacts();
    UpdateTravel();
    if (ToastRemaining > 0.0f)
    {
        ToastRemaining -= InDeltaTime;
        ToastText->SetRenderOpacity(FMath::Clamp(ToastRemaining, 0.0f, 1.0f));
    }
}

void UAstroHUDWidget::UpdateTimeBar()
{
    const UTimeController* Time = UTimeController::Get(this);
    if (!Time)
    {
        return;
    }
    DateText->SetText(FText::FromString(Time->GetSimulatedDateTime().ToString(TEXT("%Y-%m-%d  %H:%M:%S UTC"))));
    ScaleText->SetText(FText::FromString(AstroUIFormat::TimeScale(Time->GetTimeScale(), Time->IsPaused())));
    PauseLabel->SetText(FText::FromString(Time->IsPaused() ? TEXT("  >  ") : TEXT(" || ")));
    RewindLabel->SetText(FText::FromString(Time->IsRewinding() ? TEXT(">>") : TEXT("<<")));
}

void UAstroHUDWidget::UpdateMarkers()
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    APlayerController* PC = GetOwningPlayer();
    if (!Sim || !Sim->IsReady() || !PC || !PC->PlayerCameraManager)
    {
        return;
    }
    const FBodyRegistry& Registry = Sim->GetRegistry();
    while (Markers.Num() < Registry.Num())
    {
        UTextBlock* Marker = MakeText(12, Dim);
        UCanvasPanelSlot* MarkerCanvasSlot = MarkerLayer->AddChildToCanvas(Marker);
        MarkerCanvasSlot->SetAutoSize(true);
        MarkerCanvasSlot->SetAlignment(FVector2D(0.0f, 0.5f));
        Markers.Add(Marker);
    }

    const FAstroVector3d Eye = Sim->EngineToSimPosition(PC->PlayerCameraManager->GetCameraLocation());
    const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
    const float FOV = PC->PlayerCameraManager->GetFOVAngle();
    FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
    TArray<FVector2D> Placed;
    for (int32 i = 0; i < Registry.Num(); ++i)
    {
        UTextBlock* Marker = Markers[i];
        const FBodyDefinition& Body = Registry.Get(i);
        const FAstroVector3d To = Sim->GetSimulation().GetBodyState(i).Position - Eye;
        const double Distance = To.Length();
        FVector2D Screen;
        const bool bOnScreen = PC->ProjectWorldLocationToScreen(Sim->SimToScaledEnginePosition(Sim->GetSimulation().GetBodyState(i).Position), Screen, false)
            && Screen.X >= 0 && Screen.Y >= 0 && Screen.X <= ViewportSize.X && Screen.Y <= ViewportSize.Y;
        // Hide labels for bodies that already fill a good part of the view, and for moons
        // crowded against their planet (unless selected).
        const double AngularDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Min(1.0, Body.EquatorialRadiusMeters / Distance)));
        bool bShow = bOnScreen && AngularDeg < FOV * 0.15;
        // Behind a nearer body's disk (e.g. Uranus seen "through" Earth).
        for (int32 j = 0; bShow && j < Registry.Num(); ++j)
        {
            const FAstroVector3d ToOther = Sim->GetSimulation().GetBodyState(j).Position - Eye;
            const double OtherDistance = ToOther.Length();
            if (j == i || OtherDistance >= Distance)
            {
                continue;
            }
            const double CosSep = ToOther.Dot(To) / (OtherDistance * Distance);
            const double Radius = Registry.Get(j).EquatorialRadiusMeters;
            bShow = CosSep < FMath::Cos(FMath::Asin(FMath::Min(1.0, Radius / OtherDistance)));
        }
        if (bShow && Body.BodyID != SelectedBody)
        {
            for (const FVector2D& Other : Placed)
            {
                if (FVector2D::Distance(Other, Screen) < 14.0f)
                {
                    bShow = false;
                    break;
                }
            }
        }
        Marker->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!bShow)
        {
            continue;
        }
        Placed.Add(Screen);
        const bool bSelected = Body.BodyID == SelectedBody;
        Marker->SetColorAndOpacity(FSlateColor(bSelected ? Accent : Dim));
        Marker->SetText(FText::FromString(FString::Printf(TEXT("%s  %s   %s"), bSelected ? TEXT("[o]") : TEXT("o"),
            *Body.DisplayName.ToString(), *AstroUIFormat::Distance(Distance))));
        if (UCanvasPanelSlot* MarkerCanvasSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
        {
            MarkerCanvasSlot->SetPosition(Screen / Scale - FVector2D(4.0f, 0.0f));
        }
    }
}

void UAstroHUDWidget::UpdateFacts()
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    const int32 Index = Sim && Sim->IsReady() ? Sim->FindBodyIndex(SelectedBody) : INDEX_NONE;
    if (Index == INDEX_NONE)
    {
        return;
    }
    const FBodyRegistry& Registry = Sim->GetRegistry();
    const FBodyDefinition& Body = Registry.Get(Index);
    const FAstroBodyFactsRow* Row = Facts ? Facts->Find(Body.BodyID) : nullptr;
    const UAstroActivationSubsystem* Activation = UAstroActivationSubsystem::Get(this);
    const bool bLocked = Activation && Activation->GetLockedTargets().Contains(Body.BodyID);

    FactsTitle->SetText(FText::FromString(Body.DisplayName.ToString() + (bLocked ? TEXT("   [locked]") : TEXT(""))));
    FactsSummary->SetText(Row ? Row->Summary : FText::GetEmpty());

    // Derived live from the simulation and the data tables.
    const double R = Body.GetMeanRadiusMeters();
    const double Gravity = Body.GM / (R * R);
    const double Escape = FMath::Sqrt(2.0 * Body.GM / R);
    const APlayerController* PC = GetOwningPlayer();
    const FAstroVector3d Eye = PC && PC->PlayerCameraManager ? Sim->EngineToSimPosition(PC->PlayerCameraManager->GetCameraLocation()) : FAstroVector3d();
    const FAstroVector3d Pos = Sim->GetSimulation().GetBodyState(Index).Position;

    TArray<FString> Lines;
    Lines.Add(FString::Printf(TEXT("Radius  %s"), *AstroUIFormat::Distance(R)));
    Lines.Add(FString::Printf(TEXT("Mass  %s"), *AstroUIFormat::Mass(Body.MassKg)));
    Lines.Add(FString::Printf(TEXT("Surface gravity  %.2f m/s2  (%.2f g)"), Gravity, Gravity / 9.80665));
    Lines.Add(FString::Printf(TEXT("Escape velocity  %s"), *AstroUIFormat::Speed(Escape)));
    if (Body.RotationRateRadPerSec != 0.0)
    {
        const double Day = AstroConstants::TwoPi / FMath::Abs(Body.RotationRateRadPerSec);
        Lines.Add(FString::Printf(TEXT("Rotation  %s%s"), *AstroUIFormat::Duration(Day), Body.RotationRateRadPerSec < 0.0 ? TEXT(" (retrograde)") : TEXT("")));
    }
    if (Body.ParentIndex != INDEX_NONE)
    {
        const double Period = KeplerOrbit::OrbitalPeriod(Body.Elements, Registry.GetOrbitMu(Index));
        const FBodyDefinition& Parent = Registry.Get(Body.ParentIndex);
        Lines.Add(FString::Printf(TEXT("Orbit  %s around %s"), *AstroUIFormat::Duration(Period), *Parent.DisplayName.ToString()));
        const FAstroVector3d Rel = Pos - Sim->GetSimulation().GetBodyState(Body.ParentIndex).Position;
        const FAstroVector3d RelVel = Sim->GetSimulation().GetBodyState(Index).Velocity - Sim->GetSimulation().GetBodyState(Body.ParentIndex).Velocity;
        Lines.Add(FString::Printf(TEXT("Now  %s from %s at %s"), *AstroUIFormat::Distance(Rel.Length()), *Parent.DisplayName.ToString(), *AstroUIFormat::Speed(RelVel.Length())));
    }
    if (!Body.ChildIndices.IsEmpty())
    {
        Lines.Add(FString::Printf(TEXT("Moons in AstroVerse  %d"), Body.ChildIndices.Num()));
    }
    Lines.Add(FString::Printf(TEXT("Distance from you  %s"), *AstroUIFormat::Distance((Pos - Eye).Length())));
    if (Row)
    {
        if (Row->MeanSurfaceTemperatureK > 0.0)
        {
            Lines.Add(FString::Printf(TEXT("Mean temperature  %.0f K  (%.0f C)"), Row->MeanSurfaceTemperatureK, Row->MeanSurfaceTemperatureK - 273.15));
        }
        Lines.Add(TEXT("Atmosphere  ") + Row->AtmosphereText.ToString());
        Lines.Add(TEXT("Named after  ") + Row->NamedAfter.ToString());
        Lines.Add(TEXT("Discovered  ") + Row->Discovery.ToString());
        Lines.Add(TEXT(""));
        Lines.Add(Row->FunFact.ToString());
    }
    Lines.Add(TEXT(""));
    Lines.Add(bLocked ? TEXT("Click again to release the lock.   T to travel here.") : TEXT("Click again to lock (full physics).   T to travel here."));
    FactsStats->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

void UAstroHUDWidget::UpdateTravel()
{
    const UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this);
    const bool bTravelling = Travel && Travel->IsTravelling();
    const ESlateVisibility Visible = bTravelling ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
    TravelText->SetVisibility(Visible);
    TravelBar->SetVisibility(Visible);
    if (bTravelling)
    {
        TravelText->SetText(FText::FromString(FString::Printf(TEXT("Travelling to %s%s   (T to skip)"), *Travel->GetDestination().ToString(),
            Travel->GetActiveStyle() == EAstroTravelStyle::PilotedShip ? TEXT("  -  W/S throttle, mouse steer") : TEXT(""))));
        TravelBar->SetPercent(static_cast<float>(Travel->GetProgress()));
    }
}

void UAstroHUDWidget::OnRewindClicked()
{
    if (UTimeController* Time = UTimeController::Get(this)) { Time->IsRewinding() ? Time->PlayForward() : Time->Rewind(); }
}

void UAstroHUDWidget::OnPauseClicked()
{
    if (UTimeController* Time = UTimeController::Get(this)) { Time->TogglePause(); }
}

void UAstroHUDWidget::OnSlowerClicked()
{
    if (UTimeController* Time = UTimeController::Get(this)) { Time->StepTimeScale(-1); }
}

void UAstroHUDWidget::OnFasterClicked()
{
    if (UTimeController* Time = UTimeController::Get(this)) { Time->StepTimeScale(+1); }
}
