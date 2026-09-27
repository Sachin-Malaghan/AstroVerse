#pragma once
// Physical constants and unit conversions shared by every layer. These are
// laws-of-nature and unit definitions, not per-body data — body masses,
// radii and orbits belong in Data Tables (see CLAUDE.md coding conventions).
// See CLAUDE.md Phase 2.

namespace AstroConstants
{
    constexpr double Pi = 3.14159265358979323846;
    constexpr double TwoPi = 2.0 * Pi;
    constexpr double DegToRad = Pi / 180.0;
    constexpr double RadToDeg = 180.0 / Pi;

    constexpr double GravitationalConstant = 6.67430e-11;  // m^3 kg^-1 s^-2 (CODATA 2018)
    constexpr double AstronomicalUnit = 1.495978707e11;    // m (IAU 2012, exact)
    constexpr double SecondsPerDay = 86400.0;
    constexpr double DaysPerJulianCentury = 36525.0;
    constexpr double SecondsPerJulianYear = 365.25 * SecondsPerDay;

    // Mean obliquity of the ecliptic at J2000.0 (IAU 2006), for ICRF-equatorial -> ecliptic.
    constexpr double ObliquityJ2000Rad = 23.439279444 * DegToRad;

    // Unreal world units are centimeters.
    constexpr double UnrealUnitsPerMeter = 100.0;
}
