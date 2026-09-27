#pragma once
#include "CoreMinimal.h"
#include "Physics/KeplerOrbit.h"
#include "Math/AstroVector3d.h"
// Where the Sun is in the sky at a place on a body: azimuth / elevation, sunrise, solar noon,
// sunset, day length, and whole-day sun paths - for site planning (vastu, orientation of a
// building, sun hours on a field). Built from the running simulation: the body's current
// heliocentric state is propagated as an osculating conic for other times of day or year
// (arcminute-level over months), and the body's real rotation model gives the local sky.
// Validated against the NOAA solar-position algorithm (AstroVerse.Sun.MatchesNOAA).
// See CLAUDE.md "Sun at a site".

struct FBodyDefinition;

struct FAstroSunPosition
{
    double AzimuthDeg = 0.0;        // from true north, clockwise (90 = east)
    double ElevationDeg = 0.0;      // geometric, above the horizon plane
    double ApparentElevationDeg = 0.0; // with standard atmospheric refraction (bodies with air)
    FAstroVector3d DirectionENU;    // unit vector: x east, y north, z up
};

struct FAstroSunDay
{
    bool bRises = false, bSets = false;
    bool bPolarDay = false, bPolarNight = false;
    double SunriseSim = 0.0, SolarNoonSim = 0.0, SunsetSim = 0.0; // sim seconds
    double NoonElevationDeg = 0.0;
    double SunriseAzimuthDeg = 0.0, SunsetAzimuthDeg = 0.0;
    double DayLengthHours = 0.0;
};

class ASTROBODIES_API FAstroSolarGeometry
{
public:
    // BodyRelSun: the body's state relative to the star at NowSim; MuSun: G(M_star + m_body).
    FAstroSolarGeometry(const FBodyDefinition& Body, const FOrbitalState& BodyRelSun, double MuSun, double NowSim);

    // Site latitude in the body's map convention (geodetic on Earth), east longitude, and
    // height above the reference surface (m).
    void SetSite(double LatDeg, double LonDeg, double HeightM = 0.0);

    FAstroSunPosition SunAt(double SimSeconds) const;

    // Events of the civil day starting at DayStartSim (e.g. local midnight). Sunrise/sunset
    // use the standard -0.833 deg (refraction + solar semi-diameter) on bodies with air,
    // the geometric horizon minus the semi-diameter on airless ones.
    FAstroSunDay DayEvents(double DayStartSim) const;

    // Sun positions every StepMinutes across a 24 h day (only above-horizon points if asked).
    TArray<TPair<double, FAstroSunPosition>> DayPath(double DayStartSim, double StepMinutes, bool bAboveHorizonOnly) const;

    // Local midnight (sim seconds) of the day containing SimSeconds, for a UTC offset.
    static double LocalMidnight(double SimSeconds, double UtcOffsetHours);

    // Bennett (1982) refraction for an observed elevation near sea level (deg).
    static double RefractionDeg(double GeometricElevationDeg);

    // Rotation period of the body's day relative to the Sun (s): 86400 on Earth.
    double GetSolarDaySeconds() const { return SolarDaySeconds; }

private:
    FAstroVector3d SunDirectionInertial(double SimSeconds) const;

    const FBodyDefinition& Body;
    FKeplerElements Orbit;
    double Mu = 0.0;
    double Now = 0.0;
    FOrbitalState NowState;
    double SolarDaySeconds = 86400.0;
    bool bHasAtmosphere = false;
    // Site, body-fixed.
    FAstroVector3d SitePosBF, UpBF, EastBF, NorthBF;
};
