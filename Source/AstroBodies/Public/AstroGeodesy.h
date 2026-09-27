#pragma once
#include "CoreMinimal.h"
#include "Math/AstroVector3d.h"
#include "Math/AstroConstants.h"
#include <cmath>
// Latitude conventions on an oblate body (equatorial radius A, polar C). GPS and Earth maps
// (WGS84, ETOPO, Blue Marble) use geodetic latitude - the angle of the surface normal; most
// planetary datasets (MOLA, LOLA) are planetocentric - the angle of the direction from the
// centre. On Earth the two differ by up to 0.19 deg (~21 km north-south at 45 deg), which
// matters when you ask for "this field". See CLAUDE.md "Sun at a site".

namespace AstroGeodesy
{
    // Body-fixed unit direction from the centre to the surface point at (lat, east lon).
    inline FAstroVector3d SurfaceDirection(double LatDeg, double LonDeg, double A, double C, bool bGeodetic)
    {
        const double Lat = LatDeg * AstroConstants::DegToRad, Lon = LonDeg * AstroConstants::DegToRad;
        // Geodetic: the surface point is (N cos, N cos, N (C/A)^2 sin) - same longitude, flattened z.
        const double ZScale = bGeodetic ? (C * C) / (A * A) : 1.0;
        return FAstroVector3d(FMath::Cos(Lat) * FMath::Cos(Lon), FMath::Cos(Lat) * FMath::Sin(Lon), FMath::Sin(Lat) * ZScale).Normalized();
    }

    // Inverse: latitude (deg) of a body-fixed direction in the chosen convention.
    inline double LatitudeDeg(const FAstroVector3d& Dir, double A, double C, bool bGeodetic)
    {
        const double Rxy = FMath::Sqrt(Dir.X * Dir.X + Dir.Y * Dir.Y);
        const double K = bGeodetic ? (A * A) / (C * C) : 1.0;
        return FMath::RadiansToDegrees(std::atan2(Dir.Z * K, Rxy));
    }

    inline double LongitudeDeg(const FAstroVector3d& Dir)
    {
        return FMath::RadiansToDegrees(std::atan2(Dir.Y, Dir.X));
    }

    // Outward surface normal (the local "up" a plumb line / spirit level defines) at a geodetic latitude.
    inline FAstroVector3d GeodeticNormal(double LatDeg, double LonDeg)
    {
        const double Lat = LatDeg * AstroConstants::DegToRad, Lon = LonDeg * AstroConstants::DegToRad;
        return FAstroVector3d(FMath::Cos(Lat) * FMath::Cos(Lon), FMath::Cos(Lat) * FMath::Sin(Lon), FMath::Sin(Lat));
    }
}
