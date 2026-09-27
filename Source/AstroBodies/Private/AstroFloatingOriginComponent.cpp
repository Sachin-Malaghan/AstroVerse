// See CLAUDE.md Phase 7.
#include "AstroFloatingOriginComponent.h"
#include "AstroSimulationSubsystem.h"
#include "GameFramework/Actor.h"

UAstroFloatingOriginComponent::UAstroFloatingOriginComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    // After the pawn has moved (PrePhysics/physics), before bodies place themselves (PostUpdateWork).
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UAstroFloatingOriginComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const AActor* Owner = GetOwner();
    if (Owner && Owner->GetActorLocation().SizeSquared() > RebaseDistanceCm * RebaseDistanceCm)
    {
        RebaseNow();
    }
}

namespace
{
    TFunction<void(const FVector&)>& RebaseOverride()
    {
        static TFunction<void(const FVector&)> Handler;
        return Handler;
    }
}

void UAstroFloatingOriginComponent::SetRebaseOverride(TFunction<void(const FVector&)> Handler)
{
    RebaseOverride() = MoveTemp(Handler);
}

void UAstroFloatingOriginComponent::ClearRebaseOverride()
{
    RebaseOverride() = nullptr;
}

void UAstroFloatingOriginComponent::RebaseNow()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    const FVector Offset = Owner->GetActorLocation();
    if (RebaseOverride())
    {
        RebaseOverride()(Offset);
    }
    else
    {
        UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
        if (!Sim || !Sim->IsReady())
        {
            return;
        }
        Sim->ShiftRenderOrigin(Offset);
    }
    // Velocity and rotation are untouched; only the engine-space position is re-centered.
    Owner->SetActorLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
    ++RebaseCount;
}
