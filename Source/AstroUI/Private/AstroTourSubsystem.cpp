// See CLAUDE.md "Guided tour".
#include "AstroTourSubsystem.h"
#include "AstroScaleDomainSubsystem.h"
#include "AstroSimulationSubsystem.h"
#include "AstroTravelSubsystem.h"
#include "AstroUISubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroTour, Log, All);

namespace
{
    UAstroUISubsystem* UIFor(const UWorld* World)
    {
        return World ? UAstroUISubsystem::Get(World->GetFirstPlayerController()) : nullptr;
    }

    FAutoConsoleCommandWithWorld GAstroCmdTourStart(TEXT("astro.Tour.Start"), TEXT("Start the guided tour"),
        FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (UAstroTourSubsystem* T = UAstroTourSubsystem::Get(World)) { T->Start(); } }));
    FAutoConsoleCommandWithWorld GAstroCmdTourStop(TEXT("astro.Tour.Stop"), TEXT("Stop the guided tour"),
        FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (UAstroTourSubsystem* T = UAstroTourSubsystem::Get(World)) { T->Stop(); } }));
    FAutoConsoleCommandWithWorld GAstroCmdTourNext(TEXT("astro.Tour.Next"), TEXT("Skip to the next tour stop"),
        FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (UAstroTourSubsystem* T = UAstroTourSubsystem::Get(World)) { T->Next(); } }));
}

bool UAstroTourSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UAstroTourSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadStops();
    if (FParse::Param(FCommandLine::Get(), TEXT("AstroTour")))
    {
        bAutoStartPending = true;
        AutoStartDelay = 4.0; // let the simulation, UI and pawn come up first
    }
}

UAstroTourSubsystem* UAstroTourSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UAstroTourSubsystem>() : nullptr;
}

TStatId UAstroTourSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UAstroTourSubsystem, STATGROUP_Tickables);
}

void UAstroTourSubsystem::LoadStops()
{
    const UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/UI/DataTables/DT_Tour.DT_Tour"));
    if (!Table)
    {
        FString CSV;
        if (FFileHelper::LoadFileToString(CSV, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("UI/DataTables/DT_Tour.csv"))))
        {
            UDataTable* Transient = NewObject<UDataTable>(GetTransientPackage());
            Transient->RowStruct = FAstroTourStopRow::StaticStruct();
            Transient->CreateTableFromCSVString(CSV);
            Table = Transient;
        }
    }
    Stops.Reset();
    if (Table)
    {
        TArray<FName> Names = Table->GetRowNames();
        Names.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
        for (const FName& Name : Names)
        {
            Stops.Add(*Table->FindRow<FAstroTourStopRow>(Name, TEXT("Tour")));
        }
    }
    UE_LOG(LogAstroTour, Log, TEXT("Tour: %d stops"), Stops.Num());
}

void UAstroTourSubsystem::Start()
{
    if (Stops.Num() == 0)
    {
        return;
    }
    // Always begin from the solar system.
    if (UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this); Domains && Domains->GetDomain() == EAstroScaleDomain::Galaxy)
    {
        Domains->RequestDomain(EAstroScaleDomain::SolarSystem);
    }
    bRunning = true;
    Index = 0;
    Step = EStep::Enter;
    StepSeconds = 0.0;
    UE_LOG(LogAstroTour, Display, TEXT("Tour started"));
}

void UAstroTourSubsystem::Stop()
{
    if (!bRunning)
    {
        return;
    }
    bRunning = false;
    OnCameraRequest.Broadcast(NAME_None, 0.0, 0.0, 0.0, 0.0, false); // stop the drift
    if (UAstroUISubsystem* UI = UIFor(GetWorld()))
    {
        UI->HideCaption();
        UI->ShowToast(TEXT("Tour stopped.  F2 to start again, Home to return to Earth."));
    }
}

void UAstroTourSubsystem::Next()
{
    if (!bRunning)
    {
        return;
    }
    if (UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this); Travel && Travel->IsTravelling())
    {
        Travel->FinishNow();
    }
    if (++Index >= Stops.Num())
    {
        Finish();
        return;
    }
    Step = EStep::Enter;
    StepSeconds = 0.0;
}

void UAstroTourSubsystem::Finish()
{
    bRunning = false;
    OnCameraRequest.Broadcast(NAME_None, 0.0, 0.0, 0.0, 0.0, false);
    if (UAstroUISubsystem* UI = UIFor(GetWorld()))
    {
        UI->HideCaption();
        UI->ShowToast(TEXT("End of the tour.  Explore freely - Home returns to Earth, F2 plays the tour again."), 8.0f);
    }
}

void UAstroTourSubsystem::ShowNarration()
{
    if (UAstroUISubsystem* UI = UIFor(GetWorld()))
    {
        const FAstroTourStopRow& S = Stops[Index];
        UI->ShowCaption(S.Title, S.Narration,
            FString::Printf(TEXT("Tour %d / %d      N  next      F2  end tour"), Index + 1, Stops.Num()));
        if (!S.Body.IsNone() && S.Mode != EAstroTourStopMode::Galaxy)
        {
            UI->SetSelectedBody(S.Body); // the facts card fills in the numbers
        }
    }
}

void UAstroTourSubsystem::EnterStop()
{
    const FAstroTourStopRow& S = Stops[Index];
    UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this);
    ShowNarration();
    switch (S.Mode)
    {
    case EAstroTourStopMode::Galaxy:
        if (Domains && Domains->GetDomain() != EAstroScaleDomain::Galaxy)
        {
            Domains->RequestDomain(EAstroScaleDomain::Galaxy);
        }
        Step = EStep::Galaxy;
        break;
    case EAstroTourStopMode::Orbit:
        OnCameraRequest.Broadcast(S.Body, S.DistanceRadii, S.PhaseDeg, S.ElevationDeg, S.OrbitDriftDegPerSecond, true);
        Step = EStep::Holding;
        break;
    case EAstroTourStopMode::Travel:
    default:
    {
        UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this);
        if (Travel && Sim && Sim->FindBodyIndex(S.Body) != INDEX_NONE && Travel->BeginTravel(S.Body, true))
        {
            Step = EStep::Travelling;
        }
        else
        {
            UE_LOG(LogAstroTour, Warning, TEXT("Tour: can't travel to %s; skipping the warp"), *S.Body.ToString());
            Step = EStep::Arrived;
        }
        break;
    }
    }
    StepSeconds = 0.0;
}

void UAstroTourSubsystem::Tick(float DeltaTime)
{
    if (bAutoStartPending)
    {
        AutoStartDelay -= DeltaTime;
        const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
        if (AutoStartDelay <= 0.0 && Sim && Sim->IsReady() && UIFor(GetWorld()))
        {
            bAutoStartPending = false;
            Start();
        }
    }
    if (!bRunning || !Stops.IsValidIndex(Index))
    {
        return;
    }
    const UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this);
    if (Domains && Domains->IsTransitioning() && Step != EStep::Galaxy)
    {
        return; // wait for a domain change (e.g. leaving the galaxy at the start) to settle
    }
    StepSeconds += DeltaTime;
    const FAstroTourStopRow& S = Stops[Index];
    switch (Step)
    {
    case EStep::Enter:
        EnterStop();
        break;
    case EStep::Travelling:
        if (const UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this); !Travel || !Travel->IsTravelling())
        {
            Step = EStep::Arrived;
            StepSeconds = 0.0;
        }
        break;
    case EStep::Arrived:
        // A frame after arrival, so the pawn's own arrival handling has run.
        OnCameraRequest.Broadcast(S.Body, S.DistanceRadii, S.PhaseDeg, S.ElevationDeg, S.OrbitDriftDegPerSecond, false);
        Step = EStep::Holding;
        StepSeconds = 0.0;
        break;
    case EStep::Galaxy:
        if (!Domains || !Domains->IsTransitioning())
        {
            Step = EStep::Holding;
            StepSeconds = 0.0;
        }
        break;
    case EStep::Holding:
        if (StepSeconds >= S.HoldSeconds)
        {
            Next();
        }
        break;
    }
}
