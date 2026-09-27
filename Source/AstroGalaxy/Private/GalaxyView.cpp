// See CLAUDE.md Phase 10.
#include "GalaxyView.h"
#include "AstroFloatingOriginComponent.h"
#include "AstroGalaxyActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
    AActor* ViewActorOf(UWorld* World)
    {
        APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
        return PC ? (PC->GetPawn() ? static_cast<AActor*>(PC->GetPawn()) : PC->GetViewTarget()) : nullptr;
    }

    double SmootherStep(double X)
    {
        X = FMath::Clamp(X, 0.0, 1.0);
        return X * X * X * (X * (X * 6.0 - 15.0) + 10.0);
    }
}

void FGalaxyView::ActivateGalaxyScaleDomain(UWorld* World)
{
    if (!Actor.IsValid())
    {
        FActorSpawnParameters Params;
        Params.Name = TEXT("AstroGalaxy");
        Actor = World->SpawnActor<AAstroGalaxyActor>(Params);
    }
    AActor* View = ViewActorOf(World);
    if (!Actor.IsValid() || !View)
    {
        return;
    }
    bActive = true;
    SolarRotation = View->GetActorQuat();

    // Put the Sun at the engine origin, where the viewer already is.
    Actor->SetActorLocation(-AAstroGalaxyActor::KpcToEngine(AAstroGalaxyActor::SunKpc()));
    Actor->SetDomainActive(true);

    // In this domain the floating origin shifts the galaxy, not the solar-system frame.
    TWeakObjectPtr<AAstroGalaxyActor> WeakActor = Actor;
    UAstroFloatingOriginComponent::SetRebaseOverride([WeakActor](const FVector& Offset)
    {
        if (WeakActor.IsValid())
        {
            WeakActor->AddActorWorldOffset(-Offset);
        }
    });

    // Intro: start 300 ly from the Sun and pull back to a view of the whole disc.
    const FVector Sun = AAstroGalaxyActor::KpcToEngine(AAstroGalaxyActor::SunKpc());
    const FVector Near = Sun + AAstroGalaxyActor::KpcToEngine(FVector(-0.06, -0.08, 0.03));
    const FVector Far = AAstroGalaxyActor::KpcToEngine(FVector(-14.0, -24.0, 26.0));
    StartMove(World, Near, Far, Sun, FVector::ZeroVector, 6.0f);
}

void FGalaxyView::BeginOutro(UWorld* World, TFunction<void()> OnDone)
{
    if (!Actor.IsValid())
    {
        if (OnDone) OnDone();
        return;
    }
    AActor* View = ViewActorOf(World);
    const FVector Here = View ? View->GetActorLocation() - Actor->GetActorLocation() : FVector::ZeroVector;
    const FVector Sun = AAstroGalaxyActor::KpcToEngine(AAstroGalaxyActor::SunKpc());
    const FVector Approach = Sun + (Here - Sun).GetSafeNormal() * AAstroGalaxyActor::CmPerLightYear * 150.0;
    StartMove(World, Here, Approach, Sun, Sun, 3.0f);
    OnMoveDone = MoveTemp(OnDone);
}

void FGalaxyView::ReturnToSolarSystemScaleDomain(UWorld* World)
{
    bActive = false;
    MoveTime = -1.0f;
    UAstroFloatingOriginComponent::ClearRebaseOverride();
    if (Actor.IsValid())
    {
        Actor->SetDomainActive(false);
    }
    // Back where we were: the solar-system frame never moved while we were away.
    if (AActor* View = ViewActorOf(World))
    {
        View->SetActorLocationAndRotation(FVector::ZeroVector, SolarRotation);
    }
}

void FGalaxyView::StartMove(UWorld* World, const FVector& From, const FVector& To, const FVector& InLookFrom, const FVector& InLookTo, float Seconds)
{
    MoveFrom = From;
    MoveTo = To;
    LookFrom = InLookFrom;
    LookTo = InLookTo;
    MoveDuration = Seconds;
    MoveTime = 0.0f;
    OnMoveDone = nullptr;
    Tick(World, 0.0f);
}

bool FGalaxyView::Tick(UWorld* World, float DeltaSeconds)
{
    if (!bActive || MoveTime < 0.0f || !Actor.IsValid())
    {
        return false;
    }
    AActor* View = ViewActorOf(World);
    if (!View)
    {
        return false;
    }
    MoveTime += DeltaSeconds;
    const double S = SmootherStep(MoveTime / MoveDuration);
    // Distance interpolates in log space so the pull-back accelerates like a zoom.
    const FVector Center = Actor->GetActorLocation();
    const FVector FromRel = MoveFrom - LookFrom, ToRel = MoveTo - LookTo;
    const FVector Target = FMath::Lerp(LookFrom, LookTo, S);
    const double Distance = FMath::Exp(FMath::Lerp(FMath::Loge(FromRel.Size()), FMath::Loge(ToRel.Size()), S));
    const FVector Dir = FMath::Lerp(FromRel.GetSafeNormal(), ToRel.GetSafeNormal(), S).GetSafeNormal();
    const FVector Location = Center + Target + Dir * Distance;
    View->SetActorLocationAndRotation(Location, FRotationMatrix::MakeFromXZ(Center + Target - Location, FVector::UpVector).ToQuat());

    if (MoveTime >= MoveDuration)
    {
        MoveTime = -1.0f;
        if (OnMoveDone)
        {
            TFunction<void()> Done = MoveTemp(OnMoveDone);
            OnMoveDone = nullptr;
            Done();
        }
    }
    return true;
}

double FGalaxyView::GetFlightScaleCm(const FVector& EngineLocation) const
{
    if (!Actor.IsValid())
    {
        return 1e5;
    }
    const double ToSun = FVector::Dist(EngineLocation, Actor->GetSunLocation());
    const double ToPlane = FMath::Abs(EngineLocation.Z - Actor->GetActorLocation().Z);
    return FMath::Max(FMath::Min(ToSun, ToPlane + AAstroGalaxyActor::CmPerKpc * 0.3), AAstroGalaxyActor::CmPerLightYear * 5.0);
}
