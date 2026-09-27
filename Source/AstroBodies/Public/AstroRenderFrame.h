#pragma once
#include "CoreMinimal.h"
#include "Math/AstroMatrix3d.h"
// Mapping between simulation space (J2000 ecliptic, right-handed, meters, barycentric)
// and engine space (left-handed Z-up, centimeters) for one frame. The render origin
// is the sim point that sits at engine (0,0,0); the basis rotates sim directions into
// the frame's axes — identity for an inertial frame, or a landed body's co-rotating
// local east/north/up frame so the ground holds still under the player.
// See CLAUDE.md "Scale and precision" and Phase 7.

struct ASTROBODIES_API FAstroRenderFrame
{
    FAstroVector3d Origin;   // sim meters
    FAstroMatrix3d Basis;    // sim axes -> frame axes (rotation)

    FVector ToEngineDirection(const FAstroVector3d& V) const
    {
        const FAstroVector3d F = Basis * V;
        return FVector(F.X, -F.Y, F.Z); // right-handed -> engine left-handed
    }

    FAstroVector3d ToSimDirection(const FVector& V) const
    {
        return Basis.Transposed() * FAstroVector3d(V.X, -V.Y, V.Z);
    }

    FVector ToEnginePosition(const FAstroVector3d& SimMeters) const
    {
        return ToEngineDirection((SimMeters - Origin) * 100.0);
    }

    FAstroVector3d ToSimPosition(const FVector& EngineCm) const
    {
        return Origin + ToSimDirection(EngineCm) / 100.0;
    }

    // Body-fixed -> sim orientation to an engine rotation (conjugated by the handedness flip).
    FQuat ToEngineRotation(const FAstroMatrix3d& BodyToSim) const
    {
        const FAstroMatrix3d M = Basis * BodyToSim;
        const FVector X(M.M[0][0], -M.M[1][0], M.M[2][0]);
        const FVector Y(-M.M[0][1], M.M[1][1], -M.M[2][1]);
        const FVector Z(M.M[0][2], -M.M[1][2], M.M[2][2]);
        return FMatrix(FPlane(X, 0.0), FPlane(Y, 0.0), FPlane(Z, 0.0), FPlane(0.0, 0.0, 0.0, 1.0)).ToQuat();
    }
};

class FBodyRegistry;
class FSolarSystemSimulation;

// Where the render origin is and how it moves: fixed in sim space, riding with a body in
// inertial axes, or in a body's co-rotating frame aligned to local east/north/up.
// Plain data + math so it is unit-testable; UAstroSimulationSubsystem owns one.
struct ASTROBODIES_API FAstroRenderOriginState
{
    enum class EMode : uint8 { Fixed, Inertial, BodyFixed };

    EMode Mode = EMode::Fixed;
    FAstroVector3d FixedOrigin;     // Fixed: sim meters
    int32 BodyIndex = INDEX_NONE;
    FAstroVector3d Offset;          // Inertial: ecliptic meters; BodyFixed: body-fixed meters
    FAstroMatrix3d SiteBasis;       // BodyFixed: body-fixed -> local east/north/up (rows)

    void SetFixed(const FAstroVector3d& SimOrigin);
    void SetInertial(int32 InBodyIndex, const FAstroVector3d& EclipticOffset);
    void SetBodyFixed(int32 InBodyIndex, const FAstroVector3d& BodyFixedOffset);

    // Frame for the simulation's current instant.
    FAstroRenderFrame Compute(const FBodyRegistry& Registry, const FSolarSystemSimulation& Sim) const;
    // Moves the origin by a sim-space delta, keeping the current mode (and site basis).
    void Shift(const FAstroVector3d& SimDelta, const FBodyRegistry& Registry, const FSolarSystemSimulation& Sim);
};
