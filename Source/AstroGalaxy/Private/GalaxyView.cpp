// See CLAUDE.md Phase 10.
#include "GalaxyView.h"

void FGalaxyView::ActivateGalaxyScaleDomain(UWorld* World)
{
    bActive = true;
    // Phase 10: spawn/show the Milky Way representation in galaxy units and place the
    // camera at the Sun's galactocentric position.
}

void FGalaxyView::ReturnToSolarSystemScaleDomain(UWorld* World)
{
    bActive = false;
}
