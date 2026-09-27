// See CLAUDE.md Phase 2.
#include "Moon.h"
#include "Planet.h"
#include "Star.h"
#include "AstroSimulationSubsystem.h"
#include "Components/StaticMeshComponent.h"

AStar::AStar()
{
    // The Sun is the light source, not a lit surface; it never casts shadows onto itself.
    BodyMesh->SetCastShadow(false);
    BodyMesh->SetCollisionProfileName(TEXT("NoCollision"));
}

void AStar::OnBoundToDefinition(const FBodyDefinition& Definition)
{
    LuminosityWatts = Definition.LuminosityWatts;
}

void APlanet::OnBoundToDefinition(const FBodyDefinition& Definition)
{
    bHasRings = Definition.RingOuterRadiusMeters > 0.0;
    MoonBodyIDs.Reset();
    if (const UAstroSimulationSubsystem* Sim = Simulation.Get())
    {
        for (int32 Child : Definition.ChildIndices)
        {
            MoonBodyIDs.Add(Sim->GetRegistry().Get(Child).BodyID);
        }
    }
}

void AMoon::OnBoundToDefinition(const FBodyDefinition& Definition)
{
    const UAstroSimulationSubsystem* Sim = Simulation.Get();
    ParentPlanetBodyID = Sim && Definition.ParentIndex != INDEX_NONE ? Sim->GetRegistry().Get(Definition.ParentIndex).BodyID : NAME_None;
}
