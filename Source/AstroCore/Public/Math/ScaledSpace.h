#pragma once
#include "Math/AstroVector3d.h"
#include <cmath>
// Scaled-space mapping for rendering true-scale distances inside the engine's
// finite world. Within LinearLimit a body renders at its true offset; beyond
// it, distance is log-compressed and the body is shrunk by exactly the same
// factor, so angular size and near/far ordering stay physically correct.
// Simulation never uses this — only render placement. See CLAUDE.md
// "Scale and precision" and Phase 7.

struct FScaledSpacePlacement
{
    FAstroVector3d RenderOffsetMeters; // where to draw it, relative to the render origin
    double ScaleFactor = 1.0;          // multiply the body's true radius by this
};

namespace ScaledSpace
{
    inline double CompressDistance(double Distance, double LinearLimit)
    {
        return Distance <= LinearLimit ? Distance : LinearLimit * (1.0 + std::log(Distance / LinearLimit));
    }

    inline FScaledSpacePlacement Place(const FAstroVector3d& TrueOffsetMeters, double LinearLimit)
    {
        const double Distance = TrueOffsetMeters.Length();
        if (Distance <= LinearLimit)
        {
            return FScaledSpacePlacement{ TrueOffsetMeters, 1.0 };
        }
        const double Scale = CompressDistance(Distance, LinearLimit) / Distance;
        return FScaledSpacePlacement{ TrueOffsetMeters * Scale, Scale };
    }
}
