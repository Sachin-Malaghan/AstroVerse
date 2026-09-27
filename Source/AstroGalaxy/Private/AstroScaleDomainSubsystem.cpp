// See CLAUDE.md Phase 7.
#include "AstroScaleDomainSubsystem.h"
#include "AstroBody.h"
#include "AstroSimulationSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroScaleDomain, Log, All);

bool UAstroScaleDomainSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

UAstroScaleDomainSubsystem* UAstroScaleDomainSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UAstroScaleDomainSubsystem>() : nullptr;
}

TStatId UAstroScaleDomainSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UAstroScaleDomainSubsystem, STATGROUP_Tickables);
}

void UAstroScaleDomainSubsystem::RequestDomain(EAstroScaleDomain Target)
{
    if (Target == Domain || Phase != EPhase::Idle)
    {
        return;
    }
    PendingDomain = Target;
    Phase = EPhase::FadingOut;
    PhaseTime = 0.0f;
    StartFade(0.0f, 1.0f);
}

void UAstroScaleDomainSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (Phase == EPhase::Idle)
    {
        return;
    }
    PhaseTime += DeltaTime;
    if (PhaseTime < FadeSeconds)
    {
        return;
    }
    if (Phase == EPhase::FadingOut)
    {
        // Fully black: re-anchor while nothing is visible, then fade back in.
        Switch();
        Phase = EPhase::FadingIn;
        PhaseTime = 0.0f;
        StartFade(1.0f, 0.0f);
    }
    else
    {
        Phase = EPhase::Idle;
    }
}

void UAstroScaleDomainSubsystem::Switch()
{
    Domain = PendingDomain;
    if (Domain == EAstroScaleDomain::Galaxy)
    {
        SetSolarSystemVisible(false);
        GalaxyView.ActivateGalaxyScaleDomain(GetWorld());
    }
    else
    {
        GalaxyView.ReturnToSolarSystemScaleDomain(GetWorld());
        SetSolarSystemVisible(true);
    }
    UE_LOG(LogAstroScaleDomain, Display, TEXT("Scale domain: %s"), Domain == EAstroScaleDomain::Galaxy ? TEXT("Galaxy") : TEXT("Solar System"));
    OnDomainChanged.Broadcast(Domain);
}

void UAstroScaleDomainSubsystem::SetSolarSystemVisible(bool bVisible)
{
    // Hidden, not paused: the simulation keeps advancing so nothing pops on return.
    if (const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this))
    {
        for (int32 i = 0; i < Sim->GetRegistry().Num(); ++i)
        {
            if (AAstroBody* Body = Sim->GetBodyActor(i))
            {
                Body->SetActorHiddenInGame(!bVisible);
            }
        }
    }
}

void UAstroScaleDomainSubsystem::StartFade(float From, float To)
{
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
    {
        PC->PlayerCameraManager->StartCameraFade(From, To, FadeSeconds, FLinearColor::Black, false, To > 0.5f);
    }
}

namespace
{
    FAutoConsoleCommandWithWorldAndArgs GAstroCmdScaleDomain(
        TEXT("astro.Domain"), TEXT("astro.Domain <Galaxy|SolarSystem> - switch scale domain"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            if (UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(World); Domains && Args.Num() > 0)
            {
                Domains->RequestDomain(Args[0].Equals(TEXT("Galaxy"), ESearchCase::IgnoreCase) ? EAstroScaleDomain::Galaxy : EAstroScaleDomain::SolarSystem);
            }
        }));
}
