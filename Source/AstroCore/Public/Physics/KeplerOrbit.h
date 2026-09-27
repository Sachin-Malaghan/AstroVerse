#pragma once
#include "Math/AstroVector3d.h"
// Analytic two-body (conic) propagation: the Dormant-tier motion model and the
// way initial N-body states are seeded from published orbital elements.
// Elliptic orbits only (0 <= e < 1) — every body in scope is bound.
// See CLAUDE.md Phase 2 and the fidelity tier system.

struct FKeplerElements
{
    double SemiMajorAxis = 0.0;        // m
    double Eccentricity = 0.0;
    double Inclination = 0.0;          // rad, relative to the reference plane of the frame
    double LongitudeOfAscendingNode = 0.0; // rad
    double ArgumentOfPeriapsis = 0.0;  // rad
    double MeanAnomalyAtEpoch = 0.0;   // rad
    double EpochSeconds = 0.0;         // sim seconds (since J2000) at which MeanAnomalyAtEpoch applies
};

struct FOrbitalState
{
    FAstroVector3d Position; // m, relative to the primary
    FAstroVector3d Velocity; // m/s, relative to the primary
};

namespace KeplerOrbit
{
    // Solves M = E - e sin E for E (Newton iteration, converges to ~1e-15).
    ASTROCORE_API double SolveEccentricAnomaly(double MeanAnomaly, double Eccentricity);

    // Mu is the standard gravitational parameter G*(M_primary + m_body), m^3/s^2.
    ASTROCORE_API double MeanMotion(const FKeplerElements& Elements, double Mu);
    ASTROCORE_API double OrbitalPeriod(const FKeplerElements& Elements, double Mu);

    // State at the given sim time, in the frame the elements are expressed in.
    ASTROCORE_API FOrbitalState StateAtTime(const FKeplerElements& Elements, double Mu, double SimSeconds);

    // Inverse of StateAtTime: osculating elements with epoch = SimSeconds.
    // Used to re-seed a Dormant body from its last N-body state on demotion.
    ASTROCORE_API FKeplerElements ElementsFromState(const FOrbitalState& State, double Mu, double SimSeconds);
}
