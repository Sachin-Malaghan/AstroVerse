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
#include "HAL/IConsoleManager.h"
#include "AstroTourSubsystem.h"
#include "AstroSiteSubsystem.h"
#include "AstroMissionSubsystem.h"
#include "AstroScaleDomainSubsystem.h"
#include "AstroSimulationSubsystem.h"

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
    ApplyGameInputMode();

    if (UAstroUISubsystem* UI = UAstroUISubsystem::Get(this))
    {
        const bool bVR = GEngine && GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabled();
        UI->Create(this, bVR);
        UI->OnSiteRequested.AddWeakLambda(this, [this](double Lat, double Lon, double Zone, const FString& Name)
        {
            GoToSite(TEXT("Earth"), Lat, Lon, Zone, Name);
        });
        UI->OnCommand.AddUObject(this, &AAstroPlayerController::HandleUICommand);
        UI->OnMissionRequested.AddWeakLambda(this, [this](FName Body, bool bPilot) { StartMission(Body, bPilot); });
        UI->OnBodyRequested.AddWeakLambda(this, [this](FName Body)
        {
            SelectBody(Body);
            if (AAstroPawnBase* Viewer = Cast<AAstroPawnBase>(GetPawn()))
            {
                Viewer->FocusOn(Body);
            }
        });
    }
}

void AAstroPlayerController::HandleUICommand(FName Command)
{
    AAstroPawnBase* Viewer = Cast<AAstroPawnBase>(GetPawn());
    if (Command == TEXT("Tour"))
    {
        ToggleTour();
    }
    else if (Command == TEXT("Home") && Viewer)
    {
        Viewer->GoHome();
    }
    else if (Command == TEXT("ClearSite"))
    {
        ClearSite();
    }
    else if (Command == TEXT("Mission"))
    {
        StartMission(NAME_None); // the destination is chosen in Earth orbit
    }
}

void AAstroPlayerController::GoToSite(FName Body, double LatDeg, double LonDeg, double UtcOffsetHours, const FString& Name)
{
    PendingSite = FPendingSite{ Body, LatDeg, LonDeg, UtcOffsetHours, Name };
    if (UAstroTourSubsystem* Tour = UAstroTourSubsystem::Get(this); Tour && Tour->IsRunning())
    {
        Tour->Stop();
    }
    if (UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this); Domains && Domains->GetDomain() == EAstroScaleDomain::Galaxy)
    {
        Domains->RequestDomain(EAstroScaleDomain::SolarSystem); // finishes in PlayerTick once back
        return;
    }
    FinishGoToSite();
}

void AAstroPlayerController::FinishGoToSite()
{
    UAstroSiteSubsystem* Site = UAstroSiteSubsystem::Get(this);
    UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    AAstroPawnBase* Viewer = Cast<AAstroPawnBase>(GetPawn());
    if (!PendingSite.IsSet() || !Site || !Sim || !Sim->IsReady() || !Viewer)
    {
        return;
    }
    const FPendingSite P = PendingSite.GetValue();
    PendingSite.Reset();
    Site->SetSite(P.Body, P.Lat, P.Lon, P.Zone, P.Name);
    if (UAstroUISubsystem* UI = UAstroUISubsystem::Get(this))
    {
        UI->SetSiteTimeZone(true, P.Zone);
    }
    // Stand 4 m south of the gnomon, facing north over the compass, then walk.
    FVector Ground, East, North, Up;
    if (Site->GetSiteFrame(Ground, East, North, Up))
    {
        Viewer->StopOrbiting();
        Viewer->StopMotion();
        const FVector Stand = Ground - North * 400.0 + Up * 180.0;
        Viewer->TeleportToSim(Sim->EngineToSimPosition(Stand), FRotationMatrix::MakeFromXZ(North, Up).ToQuat());
        Viewer->ToggleLanding();
    }
    if (UAstroUISubsystem* UI = UAstroUISubsystem::Get(this))
    {
        UI->ShowToast(FString::Printf(TEXT("At %s: look up for the Sun's paths; the stick's shadow is real.  [ ] run time faster, L back to live."), *P.Name), 8.0f);
    }
}

void AAstroPlayerController::StartMission(FName Destination, bool bPiloted)
{
    if (Destination == TEXT("Earth") || Destination == TEXT("Sun"))
    {
        Destination = NAME_None; // chosen in orbit instead
    }
    if (UAstroTourSubsystem* Tour = UAstroTourSubsystem::Get(this); Tour && Tour->IsRunning())
    {
        Tour->Stop();
    }
    if (UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this); Domains && Domains->GetDomain() == EAstroScaleDomain::Galaxy)
    {
        Domains->RequestDomain(EAstroScaleDomain::SolarSystem);
    }
    double Lat = 13.7199, Lon = 80.2304;
    FString Name = TEXT("Satish Dhawan Space Centre, Sriharikota");
    if (const UAstroSiteSubsystem* Site = UAstroSiteSubsystem::Get(this); Site && Site->HasSite() && Site->GetReport().Body == TEXT("Earth"))
    {
        Lat = Site->GetReport().LatDeg;
        Lon = Site->GetReport().LonDeg;
        Name = Site->GetReport().Name;
        ClearSite();
    }
    if (UAstroMissionSubsystem* Mission = UAstroMissionSubsystem::Get(this))
    {
        Mission->Launch(Destination, Lat, Lon, Name, bPiloted);
    }
}

void AAstroPlayerController::ClearSite()
{
    if (UAstroSiteSubsystem* Site = UAstroSiteSubsystem::Get(this))
    {
        Site->ClearSite();
    }
    if (UAstroUISubsystem* UI = UAstroUISubsystem::Get(this))
    {
        UI->SetSiteTimeZone(false, 0.0);
    }
}

void AAstroPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (PendingSite.IsSet())
    {
        const UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this);
        if (!Domains || (!Domains->IsTransitioning() && Domains->GetDomain() == EAstroScaleDomain::SolarSystem))
        {
            FinishGoToSite();
        }
    }
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
    if (const UAstroMissionSubsystem* Mission = UAstroMissionSubsystem::Get(this); Mission && Mission->IsControllingCamera())
    {
        UI->SetViewerStatus(FString::Printf(TEXT("Mission  -  %s"), *Mission->GetStatus().PhaseText));
        return;
    }
    if (const UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this); Travel && Travel->IsTravelling())
    {
        UI->SetViewerStatus(FString::Printf(TEXT("In transit to %s"), *Travel->GetDestination().ToString()));
        return;
    }
    const FName Body = Viewer->GetReferenceBody();
    const int32 Index = Sim->FindBodyIndex(Body);
    const FString Where = Index != INDEX_NONE ? Sim->GetRegistry().Get(Index).DisplayName.ToString() : Body.ToString();
    UI->SetViewerStatus(FString::Printf(TEXT("%s %s   -   altitude %s   -   %s   -   %s"),
        Viewer->GetLocomotion() == EAstroLocomotion::Walking ? TEXT("On") : TEXT("Near"), *Where,
        *AstroUIFormat::Distance(Viewer->GetAltitude()), *AstroUIFormat::Speed(Viewer->GetSpeedMetersPerSecond()),
        Sim->IsRotatingFrame() ? TEXT("surface frame") : TEXT("orbital frame"))
        + (Viewer->GetTelescopeZoom() > 1.05f ? FString::Printf(TEXT("   -   telescope %.0fx"), Viewer->GetTelescopeZoom()) : FString()));
}

void AAstroPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    EnsureInputActions();
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
    {
        EIC->BindAction(InputActions->Select, ETriggerEvent::Started, this, &AAstroPlayerController::OnSelect);
        EIC->BindAction(InputActions->Select, ETriggerEvent::Completed, this, &AAstroPlayerController::OnSelectReleased);
        EIC->BindAction(InputActions->LockTarget, ETriggerEvent::Started, this, &AAstroPlayerController::OnLockAction);
        EIC->BindAction(InputActions->TimeFaster, ETriggerEvent::Started, this, &AAstroPlayerController::OnTimeFaster);
        EIC->BindAction(InputActions->TimeSlower, ETriggerEvent::Started, this, &AAstroPlayerController::OnTimeSlower);
        EIC->BindAction(InputActions->TimePause, ETriggerEvent::Started, this, &AAstroPlayerController::OnTimePause);
        EIC->BindAction(InputActions->TimeRewind, ETriggerEvent::Started, this, &AAstroPlayerController::OnTimeRewind);
        EIC->BindAction(InputActions->ToggleGalaxy, ETriggerEvent::Started, this, &AAstroPlayerController::OnToggleGalaxy);
        EIC->BindAction(InputActions->ToggleHUD, ETriggerEvent::Started, this, &AAstroPlayerController::OnToggleHUDAction);
        EIC->BindAction(InputActions->Menu, ETriggerEvent::Started, this, &AAstroPlayerController::OnMenuAction);
        EIC->BindAction(InputActions->Travel, ETriggerEvent::Started, this, &AAstroPlayerController::OnTravelAction);
        EIC->BindAction(InputActions->Help, ETriggerEvent::Started, this, &AAstroPlayerController::OnHelpAction);
        EIC->BindAction(InputActions->Focus, ETriggerEvent::Started, this, &AAstroPlayerController::OnFocusAction);
        EIC->BindAction(InputActions->Home, ETriggerEvent::Started, this, &AAstroPlayerController::OnHomeAction);
        EIC->BindAction(InputActions->GoLive, ETriggerEvent::Started, this, &AAstroPlayerController::OnGoLiveAction);
        EIC->BindAction(InputActions->Tour, ETriggerEvent::Started, this, &AAstroPlayerController::OnTourAction);
        EIC->BindAction(InputActions->TourNext, ETriggerEvent::Started, this, &AAstroPlayerController::OnTourNextAction);
        EIC->BindAction(InputActions->MilkyWayGuide, ETriggerEvent::Started, this, &AAstroPlayerController::OnMilkyWayGuideAction);
        EIC->BindAction(InputActions->FaceSun, ETriggerEvent::Started, this, &AAstroPlayerController::OnFaceSunAction);
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

void AAstroPlayerController::ApplyGameInputMode()
{
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    SetInputMode(Mode);
    bShowMouseCursor = true;
    DefaultMouseCursor = EMouseCursor::Default;
}

FName AAstroPlayerController::FindBodyAtScreen(FVector2D ScreenPosition, float MaxPixels) const
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (!Sim || !Sim->IsReady() || !PlayerCameraManager)
    {
        return NAME_None;
    }
    int32 SizeX = 0, SizeY = 0;
    GetViewportSize(SizeX, SizeY);
    const double PixelsPerRadian = 0.5 * SizeX / FMath::Tan(FMath::DegreesToRadians(0.5 * PlayerCameraManager->GetFOVAngle()));
    const FAstroVector3d Eye = Sim->EngineToSimPosition(PlayerCameraManager->GetCameraLocation());
    FName Best;
    double BestScore = MaxPixels;
    double BestDistance = TNumericLimits<double>::Max();
    const FBodyRegistry& Registry = Sim->GetRegistry();
    for (int32 i = 0; i < Registry.Num(); ++i)
    {
        const FAstroVector3d Position = Sim->GetSimulation().GetBodyState(i).Position;
        FVector2D Screen;
        if (!ProjectWorldLocationToScreen(Sim->SimToScaledEnginePosition(Position), Screen, false))
        {
            continue;
        }
        const double Distance = (Position - Eye).Length();
        const double DiscPx = FMath::Asin(FMath::Min(1.0, Registry.Get(i).EquatorialRadiusMeters / Distance)) * PixelsPerRadian;
        double Score = FMath::Max(0.0, FVector2D::Distance(Screen, ScreenPosition) - DiscPx);
        // The HUD label sits just right of the dot: clicking its text counts too.
        const FVector2D Offset = ScreenPosition - Screen;
        if (Offset.X > 0.0 && Offset.X < 160.0 && FMath::Abs(Offset.Y) < 10.0)
        {
            Score = FMath::Min(Score, 1.0);
        }
        // Inside a disc beats a near miss; among discs the nearest body (drawn in front) wins.
        if (Score < BestScore || (Score == 0.0 && BestScore == 0.0 && Distance < BestDistance))
        {
            BestScore = Score;
            BestDistance = Distance;
            Best = Registry.Get(i).BodyID;
        }
    }
    return Best;
}

void AAstroPlayerController::OnSelect(const FInputActionValue& Value)
{
    // Press: remember where, so a drag (camera turn) isn't taken for a click.
    float X = 0.0f, Y = 0.0f;
    bPressWithCursor = bShowMouseCursor && GetMousePosition(X, Y);
    PressPosition = FVector2D(X, Y);
    if (!bPressWithCursor)
    {
        // Gamepad / no cursor: select what the reticle is on, straight away.
        SelectBody(FindBodyUnderReticle());
    }
}

void AAstroPlayerController::OnSelectReleased(const FInputActionValue& Value)
{
    float X = 0.0f, Y = 0.0f;
    if (!bPressWithCursor || !GetMousePosition(X, Y) || FVector2D::Distance(FVector2D(X, Y), PressPosition) > 6.0f)
    {
        return; // that was a drag
    }
    const FName Under = FindBodyAtScreen(FVector2D(X, Y));
    const double Now = FPlatformTime::Seconds();
    const bool bDouble = !Under.IsNone() && Under == LastClickBody && Now - LastClickTime < 0.4;
    LastClickTime = Now;
    LastClickBody = Under;
    SelectBody(Under);
    if (bDouble)
    {
        // Double-click: fly into orbit around it (Solar System Scope style).
        if (AAstroPawnBase* Viewer = Cast<AAstroPawnBase>(GetPawn()))
        {
            Viewer->FocusOn(Under);
        }
        LastClickBody = NAME_None;
    }
}

void AAstroPlayerController::OnLockAction(const FInputActionValue& Value)
{
    UAstroActivationSubsystem* Activation = UAstroActivationSubsystem::Get(this);
    if (!Activation || SelectedBody.IsNone())
    {
        return;
    }
    const bool bLocked = Activation->GetLockedTargets().Contains(SelectedBody);
    if (bLocked)
    {
        Activation->ReleaseObservationLock(SelectedBody);
    }
    else
    {
        Activation->LockObservationTarget(SelectedBody);
    }
    if (UAstroUISubsystem* UI = UAstroUISubsystem::Get(this))
    {
        UI->ShowToast(FString::Printf(TEXT("%s: observation lock %s."), *SelectedBody.ToString(), bLocked ? TEXT("released") : TEXT("on (full N-body precision)")));
    }
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

void AAstroPlayerController::OnFocusAction(const FInputActionValue& Value)
{
    AAstroPawnBase* Viewer = Cast<AAstroPawnBase>(GetPawn());
    if (!Viewer)
    {
        return;
    }
    UAstroUISubsystem* UI = UAstroUISubsystem::Get(this);
    // F toggles: orbit the selection (or the nearest body), or go back to free flight.
    const FName Target = !SelectedBody.IsNone() ? SelectedBody : Viewer->GetReferenceBody();
    if (Viewer->IsOrbiting() && (SelectedBody.IsNone() || Viewer->GetOrbitBody() == SelectedBody))
    {
        Viewer->StopOrbiting();
        if (UI) { UI->ShowToast(TEXT("Free flight: WASD to move, mouse to look, wheel sets speed.  F to orbit again.")); }
        return;
    }
    Viewer->FocusOn(Target);
    if (UI) { UI->ShowToast(FString::Printf(TEXT("Orbiting %s: mouse or A/D to circle, wheel or W/S to zoom, F for free flight."), *Target.ToString())); }
}

void AAstroPlayerController::OnHomeAction(const FInputActionValue& Value)
{
    if (AAstroPawnBase* Viewer = Cast<AAstroPawnBase>(GetPawn()))
    {
        Viewer->GoHome();
        if (UAstroUISubsystem* UI = UAstroUISubsystem::Get(this)) { UI->ShowToast(TEXT("Home: back to Earth.  (Home / Backspace any time you're lost.)")); }
    }
}

void AAstroPlayerController::OnGoLiveAction(const FInputActionValue& Value)
{
    if (UTimeController* Time = UTimeController::Get(this))
    {
        Time->GoLive();
        if (UAstroUISubsystem* UI = UAstroUISubsystem::Get(this))
        {
            UI->ShowToast(FString::Printf(TEXT("Live: the clock follows real UTC (%s)."), *Time->GetNetworkTime().GetSourceDescription()));
        }
    }
}

void AAstroPlayerController::OnTourAction(const FInputActionValue& Value)
{
    ToggleTour();
}

void AAstroPlayerController::OnTourNextAction(const FInputActionValue& Value)
{
    if (UAstroMissionSubsystem* Mission = UAstroMissionSubsystem::Get(this); Mission && Mission->IsPiloting())
    {
        Mission->RequestAutoDock(); // N during a piloted docking hands it to the autopilot
        return;
    }
    if (UAstroTourSubsystem* Tour = UAstroTourSubsystem::Get(this))
    {
        Tour->Next();
    }
}

void AAstroPlayerController::OnMilkyWayGuideAction(const FInputActionValue& Value)
{
    if (IConsoleVariable* Guide = IConsoleManager::Get().FindConsoleVariable(TEXT("astro.Sky.MilkyWayGuide")))
    {
        const bool bOn = Guide->GetInt() == 0;
        Guide->Set(bOn ? 1 : 0, ECVF_SetByCode);
        if (UAstroUISubsystem* UI = UAstroUISubsystem::Get(this))
        {
            UI->ShowToast(bOn ? TEXT("Milky Way guide on: the band is our galaxy's disc seen edge-on from inside.  V to hide.")
                              : TEXT("Milky Way guide off."));
        }
    }
}

void AAstroPlayerController::OnFaceSunAction(const FInputActionValue& Value)
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (AAstroPawnBase* Viewer = Cast<AAstroPawnBase>(GetPawn()); Viewer && Sim && Sim->IsReady())
    {
        Viewer->FaceBody(Sim->GetRegistry().Get(Sim->GetRegistry().GetStarIndex()).BodyID);
    }
}

void AAstroPlayerController::ToggleTour()
{
    if (UAstroTourSubsystem* Tour = UAstroTourSubsystem::Get(this))
    {
        Tour->Toggle();
    }
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
