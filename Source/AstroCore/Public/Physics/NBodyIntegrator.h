#pragma once
#include "Math/AstroVector3d.h"
#include <vector>
// Full N-body gravitational integrator. Runs on a fixed timestep, independent
// of render framerate; the render thread interpolates between physics steps.
// Uses LeapfrogSolver (symplectic) rather than Runge-Kutta — RK-family
// integrators leak energy over long runs at high time-acceleration.
// See CLAUDE.md Phase 1.

struct FMassiveBodyState
{
    double Mass = 0.0;           // kg
    FAstroVector3d Position;     // meters, relative to current scale-domain origin
    FAstroVector3d Velocity;     // meters/second
};

class ASTROCORE_API FNBodyIntegrator
{
public:
    static constexpr double GravitationalConstant = 6.67430e-11; // m^3 kg^-1 s^-2 (CODATA 2018)

    void AddBody(const FMassiveBodyState& Body);
    // One kick-drift-kick step. Negative DeltaSimSeconds runs the system backward.
    void Step(double DeltaSimSeconds);
    const std::vector<FMassiveBodyState>& GetStates() const { return Bodies; }

    // Kinetic + gravitational potential energy (J). Diagnostic: bounded
    // oscillation under Leapfrog, secular drift would indicate a bug.
    double ComputeTotalEnergy() const;

private:
    void ComputeAccelerations(const std::vector<FAstroVector3d>& InPositions,
                              std::vector<FAstroVector3d>& OutAccelerations) const;

    std::vector<FMassiveBodyState> Bodies;

    // Scratch arrays in structure-of-arrays form for the solver.
    std::vector<FAstroVector3d> Positions;
    std::vector<FAstroVector3d> Velocities;
    std::vector<FAstroVector3d> Accelerations;
    bool bAccelerationsValid = false; // cleared whenever the body set changes
};
