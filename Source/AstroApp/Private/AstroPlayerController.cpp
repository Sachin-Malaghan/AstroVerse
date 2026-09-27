// See CLAUDE.md Phase 8.
#include "AstroPlayerController.h"
#include "AstroActivationSubsystem.h"
#include "AstroInputActions.h"
#include "AstroScaleDomainSubsystem.h"
#include "AstroSimulationSubsystem.h"
#include "AstroTravelSubsystem.h"
#include "AstroPawnBase.h"
#include "AstroUIFormat.h"
#include "AstroUISubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "StereoRendering.h"
#include "TimeController.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroController, Log, All);

void AAstroPlayerController::EnsureInputActions()
{
    if (!InputActions)
    {
        InputActions = UAstroInputActions::Create(this);
    }
}

void AAstroPlayerController::BeginPlay()
{
    Super::BeginPlay();
    EnsureInputActions();
    if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        const bool bVR = GEngine && GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabled();
        Input->AddMappingContext(bVR ? InputActions->VRContext : InputActions->DesktopContext, 0);
        Input->AddMappingContext(InputActions->GlobalContext, 1);
        if (bVR)
        {
            // Gamepads still work alongside motion controllers.
            Input->AddMappingContext(InputActions->DesktopContext, -1);
        }
    }
    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;

    if (UAstroUISubsystem* UI = UAstroUISubsystem::Get(this))
    {
        const bool bVR = GEngine && GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabled();
        UI->Create(this, bVR);
    }
}

void AAstroPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    UAstroUISubsystem* UI = UAstroUISubsystem::Get(this);
    const AAstroPawnBase* Viewer = Cast<AAstroPawnBase>(GetPawn());
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (!UI || !Viewer || !Sim)
    {
        return;
    }
    const UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this);
    if (Domains && Domains->GetDomain() == EAstroScaleDomain::Galaxy)
    {
        UI->SetViewerStatus(TEXT("Milky Way  -  galaxy scale"));
        return;
    }
    const FName Body = Viewer->GetReferenceBody();
    const int32 Index = Sim->FindBodyIndex(Body);
    const FString Where = Index != INDEX_NONE ? Sim->GetRegistry().Get(Index).DisplayName.ToString() : Body.ToString();
    UI->SetViewerStatus(FString::Printf(TEXT("%s %s   -   altitude %s   -   %s   -   %s"),
        Viewer->GetLocomotion() == EAstroLocomotion::Walking ? TEXT("On") : TEXT("Near"), *Where,
        *AstroUIFormat::Distance(Viewer->GetAltitude()), *AstroUIFormat::Speed(Viewer->GetSpeedMetersPerSecond()),
        Sim->IsRotatingFrame() ? TEXT("surface frame") : TEXT("orbital frame")));
}

void AAstroPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    EnsureInputActions();
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
    {
        EIC->BindAction(InputActions->Select, ETriggerEvent::Started, this, &AAstroPlayerController::OnSelect);
        EIC->BindAction(InputActions->TimeFaster, ETriggerEvent::Started, this, &AAstroPlayerController::OnTimeFaster);
        EIC->BindAction(InputActions->TimeSlower, ETriggerEvent::Started, this, &AAstroPlayerController::OnTimeSlower);
        EIC->BindAction(InputActions->TimePause, ETriggerEvent::Started, this, &AAstroPlayerController::OnTimePause);
        EIC->BindAction(InputActions->TimeRewind, ETriggerEvent::Started, this, &AAstroPlayerController::OnTimeRewind);
        EIC->BindAction(InputActions->ToggleGalaxy, ETriggerEvent::Started, this, &AAstroPlayerController::OnToggleGalaxy);
        EIC->BindAction(InputActions->ToggleHUD, ETriggerEvent::Started, this, &AAstroPlayerController::OnToggleHUDAction);
        EIC->BindAction(InputActions->Menu, ETriggerEvent::Started, this, &AAstroPlayerController::OnMenuAction);
        EIC->BindAction(InputActions->Travel, ETriggerEvent::Started, this, &AAstroPlayerController::OnTravelAction);
        EIC->BindAction(InputActions->Help, ETriggerEvent::Started, this, &AAstroPlayerController::OnHelpAction);
    }
}

FName AAstroPlayerController::FindBodyUnderReticle(float MaxAngleDeg) const
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (!Sim || !Sim->IsReady() || !PlayerCameraManager)
    {
        return NAME_None;
    }
    const FAstroVector3d Eye = Sim->EngineToSimPosition(PlayerCameraManager->GetCameraLocation());
    const FAstroVector3d Look = Sim->EngineToSimDirection(PlayerCameraManager->GetCameraRotation().Vector());
    FName Best;
    double BestScore = FMath::DegreesToRadians(MaxAngleDeg);
    for (const FBodyDefinition& Body : Sim->GetRegistry().GetAll())
    {
        const FAstroVector3d To = Sim->GetSimulation().GetBodyState(Sim->FindBodyIndex(Body.BodyID)).Position - Eye;
        const double Distance = To.Length();
        const double Angle = FMath::Acos(FMath::Clamp(To.Dot(Look) / Distance, -1.0, 1.0));
        const double Disk = FMath::Asin(FMath::Min(1.0, Body.EquatorialRadiusMeters / Distance));
        // Inside the disk counts as a direct hit; otherwise the nearest direction wins.
        const double Score = FMath::Max(0.0, Angle - Disk);
        if (Score < BestScore || (Score == 0.0 && BestScore == 0.0 && Disk > 0.0))
        {
            BestScore = Score;
            Best = Body.BodyID;
        }
    }
    return Best;
}

void AAstroPlayerController::SelectBody(FName BodyID)
{
    if (SelectedBody != BodyID)
    {
        SelectedBody = BodyID;
        UE_LOG(LogAstroController, Display, TEXT("Selected %s"), *BodyID.ToString());
        if (UAstroUISubsystem* UI = UAstroUISubsystem::Get(this))
        {
            UI->SetSelectedBody(BodyID);
        }
        OnSelectionChanged.Broadcast(BodyID);
    }
}

void AAstroPlayerController::OnSelect(const FInputActionValue& Value)
{
    const FName Under = FindBodyUnderReticle();
    if (Under.IsNone())
    {
        SelectBody(NAME_None);
        return;
    }
    // Selecting what's already selected toggles an observation lock on it.
    if (Under == SelectedBody)
    {
        if (UAstroActivationSubsystem* Activation = UAstroActivationSubsystem::Get(this))
        {
            if (Activation->GetLockedTargets().Contains(Under))
            {
                Activation->ReleaseObservationLock(Under);
            }
            else
            {
                Activation->LockObservationTarget(Under);
            }
        }
        return;
    }
    SelectBody(Under);
}

void AAstroPlayerController::OnTimeFaster(const FInputActionValue& Value)
{
    if (UTimeController* Time = UTimeController::Get(this)) { Time->StepTimeScale(+1); }
}

void AAstroPlayerController::OnTimeSlower(const FInputActionValue& Value)
{
    if (UTimeController* Time = UTimeController::Get(this)) { Time->StepTimeScale(-1); }
}

void AAstroPlayerController::OnTimePause(const FInputActionValue& Value)
{
    if (UTimeController* Time = UTimeController::Get(this)) { Time->TogglePause(); }
}

void AAstroPlayerController::OnTimeRewind(const FInputActionValue& Value)
{
    if (UTimeController* Time = UTimeController::Get(this))
    {
        Time->IsRewinding() ? Time->PlayForward() : Time->Rewind();
    }
}

void AAstroPlayerController::OnToggleGalaxy(const FInputActionValue& Value)
{
    if (UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this))
    {
        Domains->RequestDomain(Domains->GetDomain() == EAstroScaleDomain::Galaxy ? EAstroScaleDomain::SolarSystem : EAstroScaleDomain::Galaxy);
    }
}

void AAstroPlayerController::OnToggleHUDAction(const FInputActionValue& Value)
{
    if (UAstroUISubsystem* UI = UAstroUISubsystem::Get(this)) { UI->ToggleHUD(); }
    OnToggleHUD.Broadcast();
}

void AAstroPlayerController::OnMenuAction(const FInputActionValue& Value)
{
    if (UAstroUISubsystem* UI = UAstroUISubsystem::Get(this)) { UI->ToggleMenu(); }
    OnMenu.Broadcast();
}

void AAstroPlayerController::OnHelpAction(const FInputActionValue& Value)
{
    if (UAstroUISubsystem* UI = UAstroUISubsystem::Get(this)) { UI->ToggleHelp(); }
}
void AAstroPlayerController::OnTravelAction(const FInputActionValue& Value)
{
    // Travel to the selected body (or the one under the reticle); pressing again mid-transit skips ahead.
    UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this);
    if (!Travel)
    {
        return;
    }
    if (Travel->IsTravelling())
    {
        Travel->FinishNow();
        return;
    }
    const FName Target = SelectedBody.IsNone() ? FindBodyUnderReticle() : SelectedBody;
    if (!Target.IsNone())
    {
        Travel->BeginTravel(Target);
    }
    OnTravelRequested.Broadcast();
}
