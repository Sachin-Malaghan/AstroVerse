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

void UAstroFloatingOriginComponent::RebaseNow()
{
    AActor* Owner = GetOwner();
    UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (!Owner || !Sim || !Sim->IsReady())
    {
        return;
    }
    const FVector Offset = Owner->GetActorLocation();
    Sim->ShiftRenderOrigin(Offset);
    // Velocity and rotation are untouched; only the engine-space position is re-centered.
    Owner->SetActorLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
    ++RebaseCount;
}
