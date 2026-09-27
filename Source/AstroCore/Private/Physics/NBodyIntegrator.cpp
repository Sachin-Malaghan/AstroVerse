// See CLAUDE.md Phase 1.
#include "Physics/NBodyIntegrator.h"
#include "Physics/LeapfrogSolver.h"
#include <cmath>

void FNBodyIntegrator::AddBody(const FMassiveBodyState& Body)
{
    Bodies.push_back(Body);
    bAccelerationsValid = false;
}

void FNBodyIntegrator::Step(double DeltaSimSeconds)
{
    const size_t Count = Bodies.size();
    if (Count == 0 || DeltaSimSeconds == 0.0)
    {
        return;
    }

    Positions.resize(Count);
    Velocities.resize(Count);
    for (size_t i = 0; i < Count; ++i)
    {
        Positions[i] = Bodies[i].Position;
        Velocities[i] = Bodies[i].Velocity;
    }

    if (!bAccelerationsValid)
    {
        ComputeAccelerations(Positions, Accelerations);
        bAccelerationsValid = true;
    }

    LeapfrogSolver::KickDriftKick(Positions, Velocities, Accelerations, DeltaSimSeconds,
        [this](const std::vector<FAstroVector3d>& InPositions, std::vector<FAstroVector3d>& OutAccelerations)
        {
            ComputeAccelerations(InPositions, OutAccelerations);
        });

    for (size_t i = 0; i < Count; ++i)
    {
        Bodies[i].Position = Positions[i];
        Bodies[i].Velocity = Velocities[i];
    }
}

void FNBodyIntegrator::ComputeAccelerations(const std::vector<FAstroVector3d>& InPositions,
                                            std::vector<FAstroVector3d>& OutAccelerations) const
{
    const size_t Count = InPositions.size();
    OutAccelerations.assign(Count, FAstroVector3d());

    // Direct O(N^2) pairwise sum, visiting each pair once (Newton's third law).
    // Fine for the Sun + planets + major moons; revisit if body count grows large.
    for (size_t i = 0; i < Count; ++i)
    {
        for (size_t j = i + 1; j < Count; ++j)
        {
            const FAstroVector3d Delta = InPositions[j] - InPositions[i];
            const double DistSq = Delta.LengthSquared();
            if (DistSq <= 0.0)
            {
                continue; // coincident bodies: no defined direction
            }
            const double InvDistCubed = 1.0 / (DistSq * std::sqrt(DistSq));
            const FAstroVector3d Scaled = Delta * (GravitationalConstant * InvDistCubed);
            OutAccelerations[i] += Scaled * Bodies[j].Mass;
            OutAccelerations[j] -= Scaled * Bodies[i].Mass;
        }
    }
}

double FNBodyIntegrator::ComputeTotalEnergy() const
{
    double Kinetic = 0.0;
    double Potential = 0.0;
    for (size_t i = 0; i < Bodies.size(); ++i)
    {
        Kinetic += 0.5 * Bodies[i].Mass * Bodies[i].Velocity.LengthSquared();
        for (size_t j = i + 1; j < Bodies.size(); ++j)
        {
            const double Dist = (Bodies[j].Position - Bodies[i].Position).Length();
            if (Dist > 0.0)
            {
                Potential -= GravitationalConstant * Bodies[i].Mass * Bodies[j].Mass / Dist;
            }
        }
    }
    return Kinetic + Potential;
}

void FNBodyIntegrator::SetStates(const std::vector<FMassiveBodyState>& States)
{
    Bodies = States;
    bAccelerationsValid = false;
}
