// See CLAUDE.md Phase 2.
#include "CelestialBody.h"

ACelestialBody::ACelestialBody()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ACelestialBody::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // TODO Phase 2: read this body's state from the integrator output
}
