// See CLAUDE.md Phase 11.
#include "AstroHUDWidget.h"
#include "AstroActivationSubsystem.h"
#include "AstroSimulationSubsystem.h"
#include "AstroTravelSubsystem.h"
#include "AstroUIFormat.h"
#include "AstroSiteSubsystem.h"
#include "Physics/KeplerOrbit.h"
#include "Rendering/DrawElements.h"
#include "HAL/IConsoleManager.h"
#include "AstroRenderingSubsystem.h"
#include "AstroSpaceEnvironment.h"
#include "AstroScaleDomainSubsystem.h"
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
    UTextBlock* LiveText = nullptr;
    UButton* Live = MakeButton(TEXT(" LIVE "), LiveText);
    LiveLabel = LiveText;
    Live->OnClicked.AddDynamic(this, &UAstroHUDWidget::OnLiveClicked);
    Rewind->OnClicked.AddDynamic(this, &UAstroHUDWidget::OnRewindClicked);
    Slower->OnClicked.AddDynamic(this, &UAstroHUDWidget::OnSlowerClicked);
    PauseButton->OnClicked.AddDynamic(this, &UAstroHUDWidget::OnPauseClicked);
    Faster->OnClicked.AddDynamic(this, &UAstroHUDWidget::OnFasterClicked);
    for (UButton* Button : { Rewind, Slower, PauseButton, Faster, Live })
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
    BottomSlot->SetSize(FVector2D(760, 90));

    // --- Site panel (left): the Sun at a place on the ground.
    SiteLabelLayer = WidgetTree->ConstructWidget<UCanvasPanel>();
    UCanvasPanelSlot* SiteLayerSlot = Root->AddChildToCanvas(SiteLabelLayer);
    SiteLayerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    SiteLayerSlot->SetOffsets(FMargin(0));
    SitePanel = WidgetTree->ConstructWidget<UBorder>();
    SitePanel->SetBrushColor(PanelColor);
    SitePanel->SetPadding(FMargin(14, 10));
    UVerticalBox* SiteBox = WidgetTree->ConstructWidget<UVerticalBox>();
    SitePanel->AddChild(SiteBox);
    SiteTitle = MakeText(18, Accent);
    SiteText = MakeText(13, FLinearColor(0.88f, 0.9f, 0.96f, 1.0f));
    SiteBox->AddChildToVerticalBox(SiteTitle)->SetPadding(FMargin(0, 0, 0, 4));
    SiteBox->AddChildToVerticalBox(SiteText);
    USizeBox* SiteSize = WidgetTree->ConstructWidget<USizeBox>();
    SiteSize->SetWidthOverride(440.0f);
    SiteSize->AddChild(SitePanel);
    Place(Root, SiteSize, FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(20, 100));
    SitePanel->SetVisibility(ESlateVisibility::Collapsed);

    // --- Body list (left): the Sun, planets, dwarf planets; a planet's moons under it.
    UBorder* ListPanel = WidgetTree->ConstructWidget<UBorder>();
    ListPanel->SetBrushColor(PanelColor);
    ListPanel->SetPadding(FMargin(8, 6));
    BodyList = WidgetTree->ConstructWidget<UVerticalBox>();
    ListPanel->AddChild(BodyList);
    Place(Root, ListPanel, FAnchors(0.0f, 0.5f), FVector2D(0.0f, 0.5f), FVector2D(12, 20));

    // --- View options (bottom left, above the help line): Solar System Scope-style toggles.
    UBorder* Options = WidgetTree->ConstructWidget<UBorder>();
    Options->SetBrushColor(PanelColor);
    Options->SetPadding(FMargin(6, 4));
    UHorizontalBox* OptionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    Options->AddChild(OptionRow);
    AddToggle(OptionRow, TEXT("Orbits"), TEXT("astro.UI.Orbits"));
    AddToggle(OptionRow, TEXT("Labels"), TEXT("astro.UI.Labels"));
    AddToggle(OptionRow, TEXT("Belts"), TEXT("astro.UI.Belts"));
    AddToggle(OptionRow, TEXT("Milky Way"), TEXT("astro.Sky.MilkyWay"));
    AddToggle(OptionRow, TEXT("Sky guide"), TEXT("astro.Sky.MilkyWayGuide"));
    Place(Root, Options, FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(12, -30));

    // --- Caption (guided tour narration), above the toasts.
    CaptionPanel = WidgetTree->ConstructWidget<UBorder>();
    CaptionPanel->SetBrushColor(PanelColor);
    CaptionPanel->SetPadding(FMargin(18, 12));
    UVerticalBox* CaptionBox = WidgetTree->ConstructWidget<UVerticalBox>();
    CaptionPanel->AddChild(CaptionBox);
    CaptionTitle = MakeText(22, Accent);
    CaptionText = MakeText(15);
    CaptionText->SetWrapTextAt(720.0f);
    CaptionFooter = MakeText(12, Dim);
    CaptionBox->AddChildToVerticalBox(CaptionTitle)->SetPadding(FMargin(0, 0, 0, 6));
    CaptionBox->AddChildToVerticalBox(CaptionText)->SetPadding(FMargin(0, 0, 0, 8));
    CaptionBox->AddChildToVerticalBox(CaptionFooter);
    USizeBox* CaptionSize = WidgetTree->ConstructWidget<USizeBox>();
    CaptionSize->SetWidthOverride(760.0f);
    CaptionSize->AddChild(CaptionPanel);
    Place(Root, CaptionSize, FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0, -140));
    CaptionPanel->SetVisibility(ESlateVisibility::Collapsed);
    TravelText->SetVisibility(ESlateVisibility::Collapsed);
    TravelBar->SetVisibility(ESlateVisibility::Collapsed);

    // --- Help (F1 / H toggles the whole HUD; the help line is always there).
    HelpText = MakeText(12, Dim);
    HelpText->SetText(FText::FromString(TEXT(
        "WASD / Space / C  fly     Drag  look     Q E  roll     Wheel  speed     Shift  boost\n"
        "F  orbit the selection (mouse circles, wheel or W / S zoom; F again: free flight)     Home  back to Earth if lost\n"
        "Click  select, double-click  orbit it, K  lock     T  travel     G  land / take off     M  galaxy     V  Milky Way guide     U  face the Sun\n"
        "[ ]  time slower / faster     P  pause     R  rewind     L  live (real UTC)     F2  guided tour, N  next\n"
        "H  hide HUD     Esc  menu (sun at a site, settings)")));
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

void UAstroHUDWidget::ShowCaption(const FString& Title, const FString& Text, const FString& Footer)
{
    CaptionTitle->SetText(FText::FromString(Title));
    CaptionText->SetText(FText::FromString(Text));
    CaptionFooter->SetText(FText::FromString(Footer));
    CaptionPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UAstroHUDWidget::HideCaption()
{
    CaptionPanel->SetVisibility(ESlateVisibility::Collapsed);
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
    UpdateSite();
    UpdateSkyGuide();
    UpdateOrbits(InDeltaTime);
    UpdateBodyList();
    UpdateSunPointer();
    if (ToastRemaining > 0.0f)
    {
        ToastRemaining -= InDeltaTime;
        ToastText->SetRenderOpacity(FMath::Clamp(ToastRemaining, 0.0f, 1.0f));
    }
}

static TAutoConsoleVariable<int32> CVarAstroUIOrbits(TEXT("astro.UI.Orbits"), 1, TEXT("Show orbit lines."));
static TAutoConsoleVariable<int32> CVarAstroUILabels(TEXT("astro.UI.Labels"), 1, TEXT("Show body labels."));

void UAstroBodyButtonHandler::OnClicked()
{
    if (HUD.IsValid())
    {
        HUD->RequestBody(BodyID);
    }
}

void UAstroToggleHandler::OnClicked()
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName))
    {
        CVar->Set(CVar->GetInt() != 0 ? 0 : 1, ECVF_SetByCode);
    }
    Refresh();
}

void UAstroToggleHandler::Refresh() const
{
    const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
    const bool bOn = CVar && CVar->GetInt() != 0;
    if (Text.IsValid())
    {
        Text->SetText(FText::FromString(Label));
        Text->SetColorAndOpacity(FSlateColor(bOn ? FLinearColor(1.0f, 0.85f, 0.5f, 1.0f) : FLinearColor(0.5f, 0.55f, 0.65f, 1.0f)));
    }
}

void UAstroHUDWidget::AddToggle(UHorizontalBox* Row, const FString& Label, const FString& CVar)
{
    UTextBlock* Text = nullptr;
    UButton* Button = MakeButton(Label, Text);
    UAstroToggleHandler* Handler = NewObject<UAstroToggleHandler>(this);
    Handler->CVarName = CVar;
    Handler->Label = Label;
    Handler->Text = Text;
    Button->OnClicked.AddDynamic(Handler, &UAstroToggleHandler::OnClicked);
    ToggleHandlers.Add(Handler);
    Row->AddChildToHorizontalBox(Button)->SetPadding(FMargin(2, 0));
}

void UAstroHUDWidget::UpdateBodyList()
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (!Sim || !Sim->IsReady() || !BodyList || bCompact)
    {
        return;
    }
    for (UAstroToggleHandler* Toggle : ToggleHandlers)
    {
        Toggle->Refresh(); // keys (V, menu) change the same settings
    }
    // Moons of the selected planet (or of the selected moon's planet) are listed under it.
    const FBodyRegistry& Registry = Sim->GetRegistry();
    const int32 Sel = Sim->FindBodyIndex(SelectedBody);
    int32 Expanded = INDEX_NONE;
    if (Sel != INDEX_NONE)
    {
        Expanded = Registry.Get(Sel).BodyType == EAstroBodyType::Moon ? Registry.Get(Sel).ParentIndex : Sel;
    }
    const FName ExpandedID = Expanded != INDEX_NONE ? Registry.Get(Expanded).BodyID : NAME_None;
    if (bBodyListBuilt && ExpandedID == BodyListBuiltFor)
    {
        for (const TPair<FName, TWeakObjectPtr<UTextBlock>>& Entry : BodyListLabels)
        {
            if (Entry.Value.IsValid())
            {
                Entry.Value->SetColorAndOpacity(FSlateColor(Entry.Key == SelectedBody ? Accent : FLinearColor(0.88f, 0.9f, 0.96f, 1.0f)));
            }
        }
        return;
    }
    bBodyListBuilt = true;
    BodyListBuiltFor = ExpandedID;
    BodyList->ClearChildren();
    BodyHandlers.Reset();
    BodyListLabels.Reset();
    auto AddEntry = [&](int32 Index, bool bIndent)
    {
        const FBodyDefinition& Def = Registry.Get(Index);
        UTextBlock* Text = nullptr;
        UButton* Button = MakeButton((bIndent ? TEXT("    ") : TEXT("")) + Def.DisplayName.ToString(), Text);
        Text->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", bIndent ? 11 : 13));
        Text->SetJustification(ETextJustify::Left);
        Button->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
        UAstroBodyButtonHandler* Handler = NewObject<UAstroBodyButtonHandler>(this);
        Handler->BodyID = Def.BodyID;
        Handler->HUD = this;
        Button->OnClicked.AddDynamic(Handler, &UAstroBodyButtonHandler::OnClicked);
        BodyHandlers.Add(Handler);
        BodyListLabels.Emplace(Def.BodyID, Text);
        BodyList->AddChildToVerticalBox(Button)->SetHorizontalAlignment(HAlign_Fill);
    };
    // Order: star, then planets / dwarf planets by distance, each followed by its moons if expanded.
    TArray<int32> Primaries;
    for (int32 i = 0; i < Registry.Num(); ++i)
    {
        if (Registry.Get(i).BodyType != EAstroBodyType::Moon)
        {
            Primaries.Add(i);
        }
    }
    Primaries.Sort([&](int32 A, int32 B) { return Registry.Get(A).Elements.SemiMajorAxis < Registry.Get(B).Elements.SemiMajorAxis; });
    for (int32 Index : Primaries)
    {
        AddEntry(Index, false);
        if (Index == Expanded)
        {
            for (int32 m = 0; m < Registry.Num(); ++m)
            {
                if (Registry.Get(m).ParentIndex == Index && Registry.Get(m).BodyType == EAstroBodyType::Moon)
                {
                    AddEntry(m, true);
                }
            }
        }
    }
}

void UAstroHUDWidget::UpdateOrbits(float DeltaTime)
{
    ScreenLines.Reset();
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    APlayerController* PC = GetOwningPlayer();
    const UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this);
    if (bCompact || !CVarAstroUIOrbits.GetValueOnGameThread() || !Sim || !Sim->IsReady() || !PC || !PC->PlayerCameraManager
        || (Domains && Domains->GetDomain() == EAstroScaleDomain::Galaxy))
    {
        return;
    }
    const FBodyRegistry& Registry = Sim->GetRegistry();
    const FSolarSystemSimulation& State = Sim->GetSimulation();
    // Orbit shapes change slowly: rebuild from osculating elements a few times a second.
    OrbitRefreshTimer -= DeltaTime;
    if (OrbitRefreshTimer <= 0.0 || OrbitPaths.Num() == 0)
    {
        OrbitRefreshTimer = 0.5;
        OrbitPaths.Reset();
        for (int32 i = 0; i < Registry.Num(); ++i)
        {
            const FBodyDefinition& Def = Registry.Get(i);
            if (Def.ParentIndex == INDEX_NONE)
            {
                continue;
            }
            FOrbitalState Rel;
            Rel.Position = State.GetBodyState(i).Position - State.GetBodyState(Def.ParentIndex).Position;
            Rel.Velocity = State.GetBodyState(i).Velocity - State.GetBodyState(Def.ParentIndex).Velocity;
            const double Mu = Registry.GetOrbitMu(i);
            const FKeplerElements Elements = KeplerOrbit::ElementsFromState(Rel, Mu, State.GetSimSeconds());
            if (Elements.Eccentricity >= 0.99)
            {
                continue;
            }
            FOrbitPath& Path = OrbitPaths.AddDefaulted_GetRef();
            Path.Body = i;
            Path.Parent = Def.ParentIndex;
            const double Period = KeplerOrbit::OrbitalPeriod(Elements, Mu);
            const int32 N = Def.BodyType == EAstroBodyType::Moon ? 96 : 180;
            for (int32 k = 0; k <= N; ++k)
            {
                Path.Points.Add(KeplerOrbit::StateAtTime(Elements, Mu, State.GetSimSeconds() + Period * k / N).Position);
            }
        }
    }
    const FAstroVector3d Eye = Sim->EngineToSimPosition(PC->PlayerCameraManager->GetCameraLocation());
    const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
    const int32 Selected = Sim->FindBodyIndex(SelectedBody);
    for (const FOrbitPath& Path : OrbitPaths)
    {
        const FBodyDefinition& Def = Registry.Get(Path.Body);
        const FAstroVector3d Centre = State.GetBodyState(Path.Parent).Position;
        const bool bMoon = Def.BodyType == EAstroBodyType::Moon;
        const double OrbitSize = Path.Points.Num() > 0 ? Path.Points[0].Length() : 0.0;
        // Moon orbits only when close enough to see them apart from their planet.
        if (bMoon && (Centre - Eye).Length() > OrbitSize * 60.0)
        {
            continue;
        }
        const bool bSel = Path.Body == Selected;
        FScreenLine Line;
        Line.Color = bSel ? FLinearColor(1.0f, 0.8f, 0.4f, 0.9f) : (bMoon ? FLinearColor(0.5f, 0.65f, 0.8f, 0.35f) : FLinearColor(0.45f, 0.62f, 0.9f, 0.55f));
        Line.Thickness = bSel ? 1.8f : 1.1f;
        for (const FAstroVector3d& Point : Path.Points)
        {
            FVector2D Screen;
            if (PC->ProjectWorldLocationToScreen(Sim->SimToScaledEnginePosition(Centre + Point), Screen, false))
            {
                Line.Points.Add(Screen / Scale);
            }
            else if (Line.Points.Num() > 1)
            {
                ScreenLines.Add(Line); // behind the camera: break the line
                Line.Points.Reset();
            }
            else
            {
                Line.Points.Reset();
            }
        }
        if (Line.Points.Num() > 1)
        {
            ScreenLines.Add(MoveTemp(Line));
        }
    }
}

int32 UAstroHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    // Orbit lines under the rest of the HUD.
    for (const FScreenLine& Line : ScreenLines)
    {
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line.Points,
            ESlateDrawEffect::None, Line.Color, true, Line.Thickness);
    }
    return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
}

void UAstroHUDWidget::UpdateSunPointer()
{
    // Where is the Sun? From any planet it is either on screen (its marker says so) or this
    // pointer sits at the screen edge in its direction, with distance and light travel time.
    if (!SunPointer)
    {
        SunPointer = MakeText(14, FLinearColor(1.0f, 0.86f, 0.45f, 1.0f));
        UCanvasPanelSlot* PointerSlot = SiteLabelLayer->AddChildToCanvas(SunPointer);
        PointerSlot->SetAutoSize(true);
        PointerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    }
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    const UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this);
    APlayerController* PC = GetOwningPlayer();
    if (bCompact || !Sim || !Sim->IsReady() || !PC || !PC->PlayerCameraManager || (Domains && Domains->GetDomain() == EAstroScaleDomain::Galaxy))
    {
        SunPointer->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    const int32 Star = Sim->GetRegistry().GetStarIndex();
    const FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
    const FAstroVector3d ToSun = Sim->GetSimulation().GetBodyState(Star).Position - Sim->EngineToSimPosition(CameraLoc);
    const double Distance = ToSun.Length();
    const FVector Dir = Sim->SimToEngineDirection(ToSun / Distance).GetSafeNormal();
    const FVector Local = PC->PlayerCameraManager->GetCameraRotation().UnrotateVector(Dir); // X fwd, Y right, Z up
    FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
    const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
    FVector2D Screen;
    const bool bProjected = Local.X > 0.0 && PC->ProjectWorldLocationToScreen(CameraLoc + Dir * 1.0e7, Screen, false);
    if (bProjected && Screen.X > 0 && Screen.Y > 0 && Screen.X < ViewportSize.X && Screen.Y < ViewportSize.Y)
    {
        SunPointer->SetVisibility(ESlateVisibility::Collapsed); // in view: the body marker labels it
        return;
    }
    // Off screen: pin to an ellipse inside the viewport edge, in the Sun's screen direction.
    FVector2D Towards(Local.Y, -Local.Z);
    if (Towards.IsNearlyZero())
    {
        Towards = FVector2D(0.0, 1.0); // straight behind: point down
    }
    Towards.Normalize();
    const FVector2D Half = ViewportSize / Scale * 0.5f;
    // Keep clear of the body list (left) and the facts card (right).
    const FVector2D Pos = Half + FVector2D(Towards.X * (Half.X - 420.0f), Towards.Y * (Half.Y - 110.0f));
    const double LightSeconds = Distance / 299792458.0;
    const FString Light = LightSeconds < 3600.0
        ? FString::Printf(TEXT("%d min %02d s"), FMath::FloorToInt(LightSeconds / 60.0), FMath::FloorToInt(FMath::Fmod(LightSeconds, 60.0)))
        : FString::Printf(TEXT("%d h %02d min"), FMath::FloorToInt(LightSeconds / 3600.0), FMath::FloorToInt(FMath::Fmod(LightSeconds, 3600.0) / 60.0));
    const TCHAR* Arrow = FMath::Abs(Towards.X) > FMath::Abs(Towards.Y) ? (Towards.X > 0 ? TEXT(">>") : TEXT("<<")) : (Towards.Y > 0 ? TEXT("vv") : TEXT("^^"));
    SunPointer->SetText(FText::FromString(FString::Printf(TEXT("%s  Sun  %s  (light %s)   U to face it"), Arrow, *AstroUIFormat::Distance(Distance), *Light)));
    Cast<UCanvasPanelSlot>(SunPointer->Slot)->SetPosition(Pos);
    SunPointer->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UAstroHUDWidget::UpdateSkyGuide()
{
    struct FGuideLabel { double L, B; const TCHAR* Text; };
    // Galactic longitude / latitude (deg). The Sun's orbital motion points to l = 90 (Cygnus).
    static const FGuideLabel Named[] = {
        { 0.0, 0.0, TEXT("Galactic centre - Sagittarius (26,700 ly)") },
        { 180.0, 0.0, TEXT("Galactic anticentre - Taurus / Auriga") },
        { 90.0, 0.0, TEXT("The Sun moves this way - Cygnus, 230 km/s") },
        { 0.0, 90.0, TEXT("North galactic pole - Coma Berenices") },
    };
    const UAstroRenderingSubsystem* Rendering = UAstroRenderingSubsystem::Get(this);
    const AAstroSpaceEnvironment* Env = Rendering ? Rendering->GetEnvironment() : nullptr;
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    const UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this);
    APlayerController* PC = GetOwningPlayer();
    const bool bOn = Env && Sim && Sim->IsReady() && PC && PC->PlayerCameraManager && !bCompact && AAstroSpaceEnvironment::IsMilkyWayGuideOn()
        && !(Domains && Domains->GetDomain() == EAstroScaleDomain::Galaxy);
    // 4 named points + the plane marked every 10 deg.
    const int32 Count = bOn ? UE_ARRAY_COUNT(Named) + 36 : 0;
    while (SkyLabels.Num() < Count)
    {
        UTextBlock* Label = MakeText(13, FLinearColor(0.75f, 0.85f, 1.0f, 0.9f));
        UCanvasPanelSlot* LabelSlot = SiteLabelLayer->AddChildToCanvas(Label);
        LabelSlot->SetAutoSize(true);
        LabelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        SkyLabels.Add(Label);
    }
    FAstroVector3d GX, GY, GZ;
    if (bOn)
    {
        Env->GetGalacticAxes(GX, GY, GZ);
    }
    const FVector Camera = bOn ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
    const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
    for (int32 i = 0; i < SkyLabels.Num(); ++i)
    {
        UTextBlock* Label = SkyLabels[i];
        bool bVisible = false;
        if (i < Count)
        {
            const bool bNamed = i < static_cast<int32>(UE_ARRAY_COUNT(Named));
            const double L = FMath::DegreesToRadians(bNamed ? Named[i].L : (i - UE_ARRAY_COUNT(Named)) * 10.0);
            const double B = FMath::DegreesToRadians(bNamed ? Named[i].B : 0.0);
            const FAstroVector3d Dir = GX * (FMath::Cos(B) * FMath::Cos(L)) + GY * (FMath::Cos(B) * FMath::Sin(L)) + GZ * FMath::Sin(B);
            FVector2D Screen;
            bVisible = PC->ProjectWorldLocationToScreen(Camera + Sim->SimToEngineDirection(Dir) * 1.0e7, Screen, false);
            if (bVisible)
            {
                Label->SetText(FText::FromString(bNamed ? FString(Named[i].Text) : FString(TEXT("-  -"))));
                Cast<UCanvasPanelSlot>(Label->Slot)->SetPosition(Screen / Scale);
            }
        }
        Label->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}

void UAstroHUDWidget::UpdateSite()
{
    const UAstroSiteSubsystem* Site = UAstroSiteSubsystem::Get(this);
    APlayerController* PC = GetOwningPlayer();
    const bool bShow = Site && Site->HasSite() && !bCompact;
    SitePanel->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    const TArray<FAstroSiteLabel>* Labels = bShow ? &Site->GetLabels() : nullptr;
    const int32 Count = Labels ? Labels->Num() : 0;
    while (SiteLabels.Num() < Count)
    {
        UTextBlock* Label = MakeText(13);
        UCanvasPanelSlot* LabelSlot = SiteLabelLayer->AddChildToCanvas(Label);
        LabelSlot->SetAutoSize(true);
        LabelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        SiteLabels.Add(Label);
    }
    const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
    for (int32 i = 0; i < SiteLabels.Num(); ++i)
    {
        UTextBlock* Label = SiteLabels[i];
        FVector2D Screen;
        const bool bVisible = i < Count && PC && PC->ProjectWorldLocationToScreen(Site->LabelWorldPosition((*Labels)[i]), Screen, false);
        Label->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (bVisible)
        {
            Label->SetText(FText::FromString((*Labels)[i].Text));
            Label->SetColorAndOpacity(FSlateColor((*Labels)[i].Color));
            Cast<UCanvasPanelSlot>(Label->Slot)->SetPosition(Screen / Scale);
        }
    }
    if (!bShow)
    {
        return;
    }
    const FAstroSiteReport& R = Site->GetReport();
    auto Clock = [&](double Sim)
    {
        const double Hours = FMath::Fmod((Sim - R.LocalDayStartSim) / 3600.0 + 48.0, 24.0);
        const int32 Minutes = FMath::RoundToInt(Hours * 60.0) % 1440;
        return FString::Printf(TEXT("%02d:%02d"), Minutes / 60, Minutes % 60);
    };
    const int32 ZoneMin = FMath::RoundToInt(R.UtcOffsetHours * 60.0);
    SiteTitle->SetText(FText::FromString(FString::Printf(TEXT("%s   (%s)"), *R.Name, *R.Body.ToString())));
    FString Text = FString::Printf(TEXT("%.5f %s, %.5f %s   ground %.0f m%s\n"),
        FMath::Abs(R.LatDeg), R.LatDeg >= 0 ? TEXT("N") : TEXT("S"), FMath::Abs(R.LonDeg), R.LonDeg >= 0 ? TEXT("E") : TEXT("W"),
        R.ElevationM, R.bRealElevation ? TEXT(" (real elevation data)") : TEXT(" (procedural)"));
    Text += FString::Printf(TEXT("Local time zone UTC%c%02d:%02d\n\n"), ZoneMin < 0 ? TEXT('-') : TEXT('+'), FMath::Abs(ZoneMin) / 60, FMath::Abs(ZoneMin) % 60);
    const bool bUp = R.Sun.ApparentElevationDeg > -0.833;
    Text += FString::Printf(TEXT("Sun now: azimuth %.1f deg (%s), elevation %.1f deg%s\n"), R.Sun.AzimuthDeg,
        *UAstroSiteSubsystem::CompassName(R.Sun.AzimuthDeg, false), R.Sun.ApparentElevationDeg, bUp ? TEXT("") : TEXT("  - below the horizon"));
    if (R.Today.bRises || R.Today.bSets)
    {
        Text += FString::Printf(TEXT("Sunrise %s  (az %.1f %s)     Sunset %s  (az %.1f %s)\n"),
            *Clock(R.Today.SunriseSim), R.Today.SunriseAzimuthDeg, *UAstroSiteSubsystem::CompassName(R.Today.SunriseAzimuthDeg, false),
            *Clock(R.Today.SunsetSim), R.Today.SunsetAzimuthDeg, *UAstroSiteSubsystem::CompassName(R.Today.SunsetAzimuthDeg, false));
    }
    else
    {
        Text += R.Today.bPolarDay ? TEXT("Midnight sun: the Sun does not set today\n") : TEXT("Polar night: the Sun does not rise today\n");
    }
    const int32 DayMin = FMath::RoundToInt(R.Today.DayLengthHours * 60.0);
    Text += FString::Printf(TEXT("Solar noon %s at %.1f deg     Daylight %d h %02d min\n"), *Clock(R.Today.SolarNoonSim), R.Today.NoonElevationDeg, DayMin / 60, DayMin % 60);
    if (R.ShadowLengthPerMeter > 0.0)
    {
        Text += FString::Printf(TEXT("Shadow of a 1 m stick: %.2f m toward %.0f deg (%s)\n"), R.ShadowLengthPerMeter, R.ShadowAzimuthDeg,
            *UAstroSiteSubsystem::CompassName(R.ShadowAzimuthDeg, false));
    }
    Text += TEXT("\nPaths: yellow today, orange 21 Jun, blue 21 Dec, white equinox.  [ ] change the time speed, L live.");
    SiteText->SetText(FText::FromString(Text));
}

void UAstroHUDWidget::UpdateTimeBar()
{
    const UTimeController* Time = UTimeController::Get(this);
    if (!Time)
    {
        return;
    }
    // Local time: the site's zone when one is set (astro.Site), else this computer's.
    const double ZoneHours = bHasSiteZone ? SiteZoneHours : (FDateTime::Now() - FDateTime::UtcNow()).GetTotalHours();
    const int32 ZoneMinutes = FMath::RoundToInt(ZoneHours * 60.0);
    const FDateTime Local = Time->GetSimulatedDateTime() + FTimespan::FromMinutes(ZoneMinutes);
    DateText->SetText(FText::FromString(FString::Printf(TEXT("%s UTC   %s local (UTC%c%02d:%02d)"),
        *Time->GetSimulatedDateTime().ToString(TEXT("%Y-%m-%d  %H:%M:%S")), *Local.ToString(TEXT("%H:%M")),
        ZoneMinutes < 0 ? TEXT('-') : TEXT('+'), FMath::Abs(ZoneMinutes) / 60, FMath::Abs(ZoneMinutes) % 60)));
    LiveLabel->SetText(FText::FromString(Time->IsLive() ? TEXT(" LIVE ") : TEXT(" go live ")));
    LiveLabel->SetColorAndOpacity(FSlateColor(Time->IsLive() ? FLinearColor(0.35f, 1.0f, 0.45f, 1.0f) : Dim));
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
        bool bShow = bOnScreen && AngularDeg < FOV * 0.15 && (CVarAstroUILabels.GetValueOnGameThread() != 0 || Body.BodyID == SelectedBody);
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
    const UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this);
    if (Domains && Domains->GetDomain() == EAstroScaleDomain::Galaxy)
    {
        FactsPanel->SetVisibility(ESlateVisibility::Collapsed); // solar-system facts don't apply out here
        return;
    }
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
    Lines.Add(bLocked ? TEXT("Click again to release the lock.   T to travel here.") : TEXT("Double-click to orbit, K to lock (full physics).   T to travel here."));
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

void UAstroHUDWidget::OnLiveClicked()
{
    if (UTimeController* Time = UTimeController::Get(this)) { Time->GoLive(); }
}

void UAstroHUDWidget::SetSiteTimeZone(bool bEnabled, double UtcOffsetHours)
{
    bHasSiteZone = bEnabled;
    SiteZoneHours = UtcOffsetHours;
}

void UAstroHUDWidget::OnFasterClicked()
{
    if (UTimeController* Time = UTimeController::Get(this)) { Time->StepTimeScale(+1); }
}
