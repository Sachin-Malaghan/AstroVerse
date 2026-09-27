// See CLAUDE.md Phase 7.
#include "AstroRenderFrame.h"
#include "BodyRegistry.h"
#include "SolarSystemSimulation.h"

void FAstroRenderOriginState::SetFixed(const FAstroVector3d& SimOrigin)
{
    Mode = EMode::Fixed;
    BodyIndex = INDEX_NONE;
    FixedOrigin = SimOrigin;
}

void FAstroRenderOriginState::SetInertial(int32 InBodyIndex, const FAstroVector3d& EclipticOffset)
{
    Mode = EMode::Inertial;
    BodyIndex = InBodyIndex;
    Offset = EclipticOffset;
}

void FAstroRenderOriginState::SetBodyFixed(int32 InBodyIndex, const FAstroVector3d& BodyFixedOffset)
{
    Mode = EMode::BodyFixed;
    BodyIndex = InBodyIndex;
    Offset = BodyFixedOffset;

    // Local east / north / up at the anchor point, in body-fixed coordinates.
    const FAstroVector3d Up = BodyFixedOffset.LengthSquared() > 0.0 ? BodyFixedOffset.Normalized() : FAstroVector3d(0, 0, 1);
    FAstroVector3d East = FAstroVector3d(0, 0, 1).Cross(Up);
    East = East.LengthSquared() > 1e-12 ? East.Normalized() : FAstroVector3d(1, 0, 0); // at a pole
    const FAstroVector3d North = Up.Cross(East);
    SiteBasis = FAstroMatrix3d::FromColumns(East, North, Up).Transposed();
}

FAstroRenderFrame FAstroRenderOriginState::Compute(const FBodyRegistry& Registry, const FSolarSystemSimulation& Sim) const
{
    FAstroRenderFrame Frame;
    if (Mode == EMode::Fixed || !Registry.GetAll().IsValidIndex(BodyIndex))
    {
        Frame.Origin = FixedOrigin;
        return Frame;
    }
    const FAstroVector3d BodyPos = Sim.GetBodyState(BodyIndex).Position;
    if (Mode == EMode::Inertial)
    {
        Frame.Origin = BodyPos + Offset;
        return Frame;
    }
    // Co-rotating: sim -> body-fixed -> local east/north/up.
    const FAstroMatrix3d BodyToSim = Registry.Get(BodyIndex).GetOrientationAt(Sim.GetSimSeconds());
    Frame.Origin = BodyPos + BodyToSim * Offset;
    Frame.Basis = SiteBasis * BodyToSim.Transposed();
    return Frame;
}

void FAstroRenderOriginState::Shift(const FAstroVector3d& SimDelta, const FBodyRegistry& Registry, const FSolarSystemSimulation& Sim)
{
    switch (Mode)
    {
    case EMode::Fixed:
        FixedOrigin += SimDelta;
        break;
    case EMode::Inertial:
        Offset += SimDelta;
        break;
    case EMode::BodyFixed:
        // Keep the site basis: re-deriving it would tilt the world under the player.
        Offset += Registry.Get(BodyIndex).GetOrientationAt(Sim.GetSimSeconds()).Transposed() * SimDelta;
        break;
    }
}
