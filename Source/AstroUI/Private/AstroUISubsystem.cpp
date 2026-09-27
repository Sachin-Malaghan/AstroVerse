// See CLAUDE.md Phase 11.
#include "AstroUISubsystem.h"
#include "AstroActivationSubsystem.h"
#include "AstroHUDWidget.h"
#include "AstroPauseMenuWidget.h"
#include "AstroScaleDomainSubsystem.h"
#include "AstroTravelSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UAstroUISubsystem* UAstroUISubsystem::Get(const APlayerController* PC)
{
    const ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
    return Player ? Player->GetSubsystem<UAstroUISubsystem>() : nullptr;
}

void UAstroUISubsystem::LoadFacts()
{
    const UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/UI/DataTables/DT_Facts.DT_Facts"));
    if (!Table)
    {
        // Dev fallback: the CSV source.
        FString CSV;
        if (FFileHelper::LoadFileToString(CSV, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("UI/DataTables/DT_Facts.csv"))))
        {
            UDataTable* Transient = NewObject<UDataTable>(GetTransientPackage());
            Transient->RowStruct = FAstroBodyFactsRow::StaticStruct();
            Transient->CreateTableFromCSVString(CSV);
            Table = Transient;
        }
    }
    if (Table)
    {
        Table->ForeachRow<FAstroBodyFactsRow>(TEXT("Facts"), [this](const FName& Row, const FAstroBodyFactsRow& Value)
        {
            Facts.Add(Row, Value);
        });
    }
}

void UAstroUISubsystem::Create(APlayerController* PC, bool bVR)
{
    if (HUD || !PC)
    {
        return;
    }
    Controller = PC;
    bVRMode = bVR;
    LoadFacts();

    HUD = CreateWidget<UAstroHUDWidget>(PC, UAstroHUDWidget::StaticClass());
    HUD->SetFacts(&Facts);
    HUD->SetCompact(bVR);
    if (!bVR)
    {
        HUD->AddToViewport(0);
    }
    Menu = CreateWidget<UAstroPauseMenuWidget>(PC, UAstroPauseMenuWidget::StaticClass());
    Menu->OnResume.BindUObject(this, &UAstroUISubsystem::ToggleMenu);
    Menu->OnSiteRequest.BindLambda([this](double Lat, double Lon, double Zone, const FString& Name)
    {
        ToggleMenu();
        OnSiteRequested.Broadcast(Lat, Lon, Zone, Name);
    });
    Menu->OnCommand.BindLambda([this](FName Command)
    {
        ToggleMenu();
        OnCommand.Broadcast(Command);
    });

    UWorld* World = PC->GetWorld();
    if (UAstroActivationSubsystem* Activation = UAstroActivationSubsystem::Get(World))
    {
        Activation->OnLockEvicted.AddDynamic(this, &UAstroUISubsystem::HandleLockEvicted);
        Activation->OnFullPhysicsAvailabilityChanged.AddDynamic(this, &UAstroUISubsystem::HandleFullPhysics);
    }
    if (UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(World))
    {
        Travel->OnTravelStarted.AddDynamic(this, &UAstroUISubsystem::HandleTravelStarted);
        Travel->OnTravelArrived.AddDynamic(this, &UAstroUISubsystem::HandleTravelArrived);
    }
    if (UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(World))
    {
        Domains->OnDomainChanged.AddDynamic(this, &UAstroUISubsystem::HandleDomainChanged);
    }
    ShowToast(TEXT("Welcome to AstroVerse.  Click a body to select it, T to travel there.  F1 for controls."));
}

void UAstroUISubsystem::SetSelectedBody(FName BodyID)
{
    if (HUD)
    {
        HUD->SetSelectedBody(BodyID);
    }
}

void UAstroUISubsystem::SetViewerStatus(const FString& Status)
{
    if (HUD)
    {
        HUD->SetViewerStatus(Status);
    }
}

void UAstroUISubsystem::ShowToast(const FString& Message, float Seconds)
{
    if (HUD)
    {
        HUD->ShowToast(Message, Seconds);
    }
}

void UAstroUISubsystem::ShowCaption(const FString& Title, const FString& Text, const FString& Footer)
{
    if (HUD)
    {
        HUD->ShowCaption(Title, Text, Footer);
    }
}

void UAstroUISubsystem::HideCaption()
{
    if (HUD)
    {
        HUD->HideCaption();
    }
}

void UAstroUISubsystem::ToggleHUD()
{
    if (HUD)
    {
        bHUDHidden = !bHUDHidden;
        HUD->SetVisibility(bHUDHidden ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    }
}

void UAstroUISubsystem::SetSiteTimeZone(bool bEnabled, double UtcOffsetHours)
{
    if (HUD)
    {
        HUD->SetSiteTimeZone(bEnabled, UtcOffsetHours);
    }
}

void UAstroUISubsystem::ToggleHelp()
{
    if (HUD)
    {
        HUD->ToggleHelp();
    }
}

void UAstroUISubsystem::ToggleMenu()
{
    APlayerController* PC = Controller.Get();
    if (!Menu || !PC)
    {
        return;
    }
    bMenuOpen = !bMenuOpen;
    if (bMenuOpen)
    {
        Menu->AddToViewport(10);
        FInputModeGameAndUI Mode;
        Mode.SetWidgetToFocus(Menu->TakeWidget());
        PC->SetInputMode(Mode);
        PC->bShowMouseCursor = true;
    }
    else
    {
        Menu->RemoveFromParent();
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }
    // Game pause also stops the god-mode clock (UTimeController skips paused worlds).
    UGameplayStatics::SetGamePaused(PC, bMenuOpen);
}

void UAstroUISubsystem::HandleLockEvicted(FName Evicted, FName NewBody)
{
    ShowToast(FString::Printf(TEXT("Observation locks are limited to 3: released %s to lock %s."), *Evicted.ToString(), *NewBody.ToString()));
}

void UAstroUISubsystem::HandleFullPhysics(FName BodyID, bool bAvailable)
{
    ShowToast(bAvailable
        ? FString::Printf(TEXT("%s: full N-body physics resumed."), *BodyID.ToString())
        : FString::Printf(TEXT("%s: orbit too fast to integrate at this time scale - using its analytic orbit until you slow down."), *BodyID.ToString()));
}

void UAstroUISubsystem::HandleTravelStarted(FName BodyID)
{
    ShowToast(FString::Printf(TEXT("Travelling to %s"), *BodyID.ToString()));
}

void UAstroUISubsystem::HandleTravelArrived(FName BodyID)
{
    ShowToast(FString::Printf(TEXT("Arrived at %s"), *BodyID.ToString()));
}

void UAstroUISubsystem::HandleDomainChanged(EAstroScaleDomain Domain)
{
    ShowToast(Domain == EAstroScaleDomain::Galaxy
        ? TEXT("The Milky Way.  The marker is the Sun: 26,700 light-years from the galactic center.  M to return.")
        : TEXT("Back in the solar system."));
}
