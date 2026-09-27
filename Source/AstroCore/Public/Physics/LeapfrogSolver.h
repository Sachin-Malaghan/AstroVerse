#pragma once
#include "Math/AstroVector3d.h"
#include <functional>
#include <vector>
// Symplectic velocity-Verlet / Leapfrog step: kick-drift-kick.
// Chosen over Runge-Kutta specifically for long-run energy conservation
// under heavy time-acceleration (god-mode clock can run centuries/second).
// See CLAUDE.md Phase 1.
//
// Operates on the whole system at once: in N-body, the second half-kick needs
// accelerations evaluated after *every* body has drifted, so a per-body step
// with a fixed acceleration would silently degrade to non-symplectic Euler.

namespace LeapfrogSolver
{
    // Fills Accelerations (same size as Positions) from the current Positions.
    using FComputeAccelerations = std::function<void(const std::vector<FAstroVector3d>& Positions,
                                                     std::vector<FAstroVector3d>& Accelerations)>;

    // v += a * dt
    ASTROCORE_API void Kick(std::vector<FAstroVector3d>& Velocities,
                            const std::vector<FAstroVector3d>& Accelerations, double DeltaSeconds);

    // x += v * dt
    ASTROCORE_API void Drift(std::vector<FAstroVector3d>& Positions,
                             const std::vector<FAstroVector3d>& Velocities, double DeltaSeconds);

    // Accelerations must hold a(Positions) on entry and holds a(new Positions)
    // on exit, so consecutive steps cost one force evaluation each.
    // Time-reversible: a negative DeltaSeconds exactly retraces a positive one.
    ASTROCORE_API void KickDriftKick(std::vector<FAstroVector3d>& Positions,
                                     std::vector<FAstroVector3d>& Velocities,
                                     std::vector<FAstroVector3d>& Accelerations,
                                     double DeltaSeconds,
                                     const FComputeAccelerations& ComputeAccelerations);
}
