// See CLAUDE.md Phase 1.
#include "Time/SimClock.h"

void FSimClock::Advance(double RealDeltaSeconds)
{
    if (bPaused)
    {
        return;
    }
    // Negative TimeScale is rewind; the Leapfrog integrator is time-reversible.
    SimulatedSeconds += RealDeltaSeconds * TimeScale;
}
