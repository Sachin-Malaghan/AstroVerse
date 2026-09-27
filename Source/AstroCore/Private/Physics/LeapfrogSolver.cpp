// See CLAUDE.md Phase 1.
#include "Physics/LeapfrogSolver.h"

namespace LeapfrogSolver
{
    void Kick(std::vector<FAstroVector3d>& Velocities,
              const std::vector<FAstroVector3d>& Accelerations, double DeltaSeconds)
    {
        for (size_t i = 0; i < Velocities.size(); ++i)
        {
            Velocities[i] += Accelerations[i] * DeltaSeconds;
        }
    }

    void Drift(std::vector<FAstroVector3d>& Positions,
               const std::vector<FAstroVector3d>& Velocities, double DeltaSeconds)
    {
        for (size_t i = 0; i < Positions.size(); ++i)
        {
            Positions[i] += Velocities[i] * DeltaSeconds;
        }
    }

    void KickDriftKick(std::vector<FAstroVector3d>& Positions,
                       std::vector<FAstroVector3d>& Velocities,
                       std::vector<FAstroVector3d>& Accelerations,
                       double DeltaSeconds,
                       const FComputeAccelerations& ComputeAccelerations)
    {
        const double HalfStep = 0.5 * DeltaSeconds;
        Kick(Velocities, Accelerations, HalfStep);
        Drift(Positions, Velocities, DeltaSeconds);
        ComputeAccelerations(Positions, Accelerations);
        Kick(Velocities, Accelerations, HalfStep);
    }
}
