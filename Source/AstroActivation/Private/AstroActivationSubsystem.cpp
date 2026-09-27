// See CLAUDE.md Phase 5.
#include "AstroActivationSubsystem.h"
#include "AstroBody.h"
#include "AstroSimulationSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroActivation, Log, All);

bool UAstroActivationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

UAstroActivationSubsystem* UAstroActivationSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UAstroActivationSubsystem>() : nullptr;
}

TStatId UAstroActivationSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UAstroActivationSubsystem, STATGROUP_Tickables);
}

void UAstroActivationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    Simulation = UAstroSimulationSubsystem::Get(&InWorld);
    // World subsystems begin play in no fixed order; the registry may not be loaded yet.
    TryInitialize();
}

void UAstroActivationSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!TryInitialize())
    {
        return;
    }
    TArray<FActivationBodyView> Views;
    GatherViews(Views);
    Manager.Tick(DeltaTime, Views);
    ApplyTiers();
}

void UAstroActivationSubsystem::GatherViews(TArray<FActivationBodyView>& OutViews) const
{
    const UAstroSimulationSubsystem* Sim = Simulation.Get();
    const FBodyRegistry& Registry = Sim->GetRegistry();
    OutViews.SetNum(Registry.Num());

    const APlayerController* PC = GetWorld()->GetFirstPlayerController();
    const APlayerCameraManager* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
    if (!Camera)
    {
        return;
    }
    const FVector CameraLocation = Camera->GetCameraLocation();
    const FRotationMatrix CameraAxes(Camera->GetCameraRotation());
    const FAstroVector3d CameraSim = Sim->EngineToSimPosition(CameraLocation);

    FVector2D ViewportSize(16.0, 9.0);
    if (const UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
    {
        Viewport->GetViewportSize(ViewportSize);
    }
    const double HalfHorizontal = FMath::DegreesToRadians(Camera->GetFOVAngle() * 0.5);
    const double HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) * ViewportSize.Y / FMath::Max(1.0, ViewportSize.X));

    for (int32 i = 0; i < Registry.Num(); ++i)
    {
        const FBodyDefinition& Body = Registry.Get(i);
        const FAstroVector3d ToBody = Sim->GetSimulation().GetBodyState(i).Position - CameraSim;
        const double Distance = ToBody.Length();
        const double Radius = Body.EquatorialRadiusMeters;
        const double AngularRadius = Distance > Radius ? FMath::Asin(Radius / Distance) : UE_HALF_PI;

        // Direction is preserved by scaled space, so test it in engine axes.
        const FVector Dir = UAstroSimulationSubsystem::SimToEngineDirection(ToBody.Normalized());
        const double Forward = FVector::DotProduct(Dir, CameraAxes.GetScaledAxis(EAxis::X));
        const double Right = FVector::DotProduct(Dir, CameraAxes.GetScaledAxis(EAxis::Y));
        const double Up = FVector::DotProduct(Dir, CameraAxes.GetScaledAxis(EAxis::Z));
        const bool bInside = Distance <= Radius
            || (Forward > 0.0
                && FMath::Abs(FMath::Atan2(Right, Forward)) <= HalfHorizontal + AngularRadius
                && FMath::Abs(FMath::Atan2(Up, Forward)) <= HalfVertical + AngularRadius);

        OutViews[i].bInFrustum = bInside;
        OutViews[i].AngularRadiusRad = AngularRadius;
    }
}

void UAstroActivationSubsystem::ApplyTiers()
{
    UAstroSimulationSubsystem* Sim = Simulation.Get();
    const FBodyRegistry& Registry = Sim->GetRegistry();
    for (int32 i = 0; i < Registry.Num(); ++i)
    {
        const FBodyDefinition& Body = Registry.Get(i);
        const EFidelityTier Tier = Manager.GetTierForIndex(i);

        if (Body.BodyType == EAstroBodyType::Moon)
        {
            const bool bWantsNBody = Tier == EFidelityTier::Active;
            const bool bResolvable = Sim->CanResolveMoonInNBody(i);
            Sim->SetMoonUsesNBody(i, bWantsNBody && bResolvable);

            const bool bBlocked = bWantsNBody && !bResolvable;
            if (bBlocked != FullPhysicsBlocked[i])
            {
                FullPhysicsBlocked[i] = bBlocked;
                UE_LOG(LogAstroActivation, Display, TEXT("%s: full N-body %s at the current time scale."),
                    *Body.BodyID.ToString(), bBlocked ? TEXT("paused (orbit too fast to resolve)") : TEXT("resumed"));
                OnFullPhysicsAvailabilityChanged.Broadcast(Body.BodyID, !bBlocked);
            }
        }

        if (AAstroBody* Actor = Sim->GetBodyActor(i))
        {
            Actor->SetHighDetail(Tier != EFidelityTier::Dormant);
        }
    }
}

bool UAstroActivationSubsystem::TryInitialize()
{
    if (!bInitialized && Simulation.IsValid() && Simulation->IsReady())
    {
        Manager.Initialize(&Simulation->GetRegistry());
        FullPhysicsBlocked.Init(false, Simulation->GetRegistry().Num());
        bInitialized = true;
    }
    return bInitialized && Simulation.IsValid();
}

FName UAstroActivationSubsystem::LockObservationTarget(FName BodyID)
{
    const FName Evicted = Manager.LockObservationTarget(BodyID);
    if (!Evicted.IsNone())
    {
        UE_LOG(LogAstroActivation, Display, TEXT("Observation lock limit (%d): %s released to lock %s."),
            FActivationManager::MaxConcurrentLocks, *Evicted.ToString(), *BodyID.ToString());
        OnLockEvicted.Broadcast(Evicted, BodyID);
    }
    return Evicted;
}

void UAstroActivationSubsystem::ReleaseObservationLock(FName BodyID)
{
    Manager.ReleaseObservationLock(BodyID);
}

void UAstroActivationSubsystem::SetReferenceFrame(FName BodyID)
{
    UAstroSimulationSubsystem* Sim = Simulation.Get();
    const int32 Index = Sim ? Sim->FindBodyIndex(BodyID) : INDEX_NONE;
    if (Index == INDEX_NONE)
    {
        return;
    }
    Manager.SetReferenceFrame(BodyID);
    // Keep the origin exactly where it is, but let it ride with the body from now on.
    const FAstroVector3d Origin = Sim->GetRenderOrigin();
    Sim->SetRenderOriginAnchor(Index, Origin - Sim->GetSimulation().GetBodyState(Index).Position);
}

void UAstroActivationSubsystem::ClearReferenceFrame()
{
    Manager.ClearReferenceFrame();
    if (UAstroSimulationSubsystem* Sim = Simulation.Get())
    {
        Sim->ClearRenderOriginAnchor();
    }
}
