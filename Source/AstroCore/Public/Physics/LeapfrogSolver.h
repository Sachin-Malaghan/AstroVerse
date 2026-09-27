#pragma once
#include "Math/AstroVector3d.h"
// Symplectic velocity-Verlet / Leapfrog step: kick-drift-kick.
// Chosen over Runge-Kutta specifically for long-run energy conservation
// under heavy time-acceleration (god-mode clock can run centuries/second).
// See CLAUDE.md Phase 1.

namespace LeapfrogSolver
{
    void KickDriftKick(FAstroVector3d& Position, FAstroVector3d& Velocity,
                        const FAstroVector3d& Acceleration, double DeltaSeconds);
}
