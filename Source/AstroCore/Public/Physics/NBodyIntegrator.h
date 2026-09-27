#pragma once
#include "Math/AstroVector3d.h"
#include <vector>
// Full N-body gravitational integrator. Runs on a fixed timestep, independent
// of render framerate; the render thread interpolates between physics steps.
// Uses FLeapfrogSolver (symplectic) rather than Runge-Kutta — RK-family
// integrators leak energy over long runs at high time-acceleration.
// See CLAUDE.md Phase 1.

struct FMassiveBodyState
{
    double Mass = 0.0;           // kg
    FAstroVector3d Position;     // meters, relative to current scale-domain origin
    FAstroVector3d Velocity;     // meters/second
};

class FNBodyIntegrator
{
public:
    void AddBody(const FMassiveBodyState& Body);
    void Step(double DeltaSimSeconds);
    const std::vector<FMassiveBodyState>& GetStates() const { return Bodies; }

private:
    std::vector<FMassiveBodyState> Bodies;
    // TODO Phase 1: gravitational constant, force accumulation, Leapfrog kick-drift-kick
};
