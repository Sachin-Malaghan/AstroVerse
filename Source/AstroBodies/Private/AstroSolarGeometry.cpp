// See CLAUDE.md "Sun at a site".
#include "AstroSolarGeometry.h"
#include "AstroGeodesy.h"
#include "BodyRegistry.h"
#include "BodyTerrain.h"
#include "Math/AstroConstants.h"
#include <cmath>

namespace
{
    constexpr double SunSemiDiameterDeg = 0.2666; // at 1 AU; the few-% seasonal change is below our tolerance
}

FAstroSolarGeometry::FAstroSolarGeometry(const FBodyDefinition& InBody, const FOrbitalState& BodyRelSun, double MuSun, double NowSim)
    : Body(InBody), Mu(MuSun), Now(NowSim), NowState(BodyRelSun)
{
    Orbit = KeplerOrbit::ElementsFromState(BodyRelSun, MuSun, NowSim);
    const double MeanMotion = KeplerOrbit::MeanMotion(Orbit, MuSun);
    const double Spin = Body.RotationRateRadPerSec;
    // Synodic (solar) day: spin relative to the orbiting Sun direction.
    const double Relative = Spin - MeanMotion;
    SolarDaySeconds = FMath::Abs(Relative) > 1e-12 ? FMath::Abs(AstroConstants::TwoPi / Relative) : 86400.0;
    // Bodies with appreciable air bend the Sun's light at the horizon.
    bHasAtmosphere = Body.BodyID == TEXT("Earth") || Body.BodyID == TEXT("Mars") || Body.BodyID == TEXT("Venus") || Body.BodyID == TEXT("Titan");
    SetSite(0.0, 0.0, 0.0);
}

void FAstroSolarGeometry::SetSite(double LatDeg, double LonDeg, double HeightM)
{
    const bool bGeodetic = Body.Terrain.IsValid() && Body.Terrain->UsesGeodeticLatitude();
    const double A = Body.EquatorialRadiusMeters, C = Body.PolarRadiusMeters;
    const FAstroVector3d Dir = AstroGeodesy::SurfaceDirection(LatDeg, LonDeg, A, C, bGeodetic);
    const double R = 1.0 / FMath::Sqrt((Dir.X * Dir.X + Dir.Y * Dir.Y) / (A * A) + Dir.Z * Dir.Z / (C * C));
    SitePosBF = Dir * (R + HeightM);
    // Local vertical: the ellipsoid normal on geodetic bodies (what a plumb line shows), else radial.
    UpBF = bGeodetic ? AstroGeodesy::GeodeticNormal(LatDeg, LonDeg) : Dir;
    EastBF = FAstroVector3d(0.0, 0.0, 1.0).Cross(UpBF);
    EastBF = EastBF.Length() > 1e-9 ? EastBF.Normalized() : FAstroVector3d(0.0, 1.0, 0.0);
    NorthBF = UpBF.Cross(EastBF);
}

FAstroVector3d FAstroSolarGeometry::SunDirectionInertial(double SimSeconds) const
{
    // Body -> Sun, inertial (J2000 ecliptic), from the osculating heliocentric conic.
    const FOrbitalState State = FMath::IsNearlyEqual(SimSeconds, Now) ? NowState : KeplerOrbit::StateAtTime(Orbit, Mu, SimSeconds);
    return State.Position * -1.0;
}

FAstroSunPosition FAstroSolarGeometry::SunAt(double SimSeconds) const
{
    const FAstroMatrix3d Orientation = Body.GetOrientationAt(SimSeconds);
    // Topocentric: from the site, not the body's centre (parallax is tiny for the Sun, but free).
    const FAstroVector3d ToSunBF = Orientation.Transposed() * SunDirectionInertial(SimSeconds) - SitePosBF;
    const FAstroVector3d D = ToSunBF.Normalized();
    FAstroSunPosition Out;
    Out.DirectionENU = FAstroVector3d(D.Dot(EastBF), D.Dot(NorthBF), D.Dot(UpBF));
    Out.ElevationDeg = FMath::RadiansToDegrees(std::asin(FMath::Clamp(Out.DirectionENU.Z, -1.0, 1.0)));
    double Az = FMath::RadiansToDegrees(std::atan2(Out.DirectionENU.X, Out.DirectionENU.Y));
    Out.AzimuthDeg = Az < 0.0 ? Az + 360.0 : Az;
    Out.ApparentElevationDeg = Out.ElevationDeg + (bHasAtmosphere ? RefractionDeg(Out.ElevationDeg) : 0.0);
    return Out;
}

double FAstroSolarGeometry::RefractionDeg(double GeometricElevationDeg)
{
    // Bennett's formula takes the apparent altitude; one fixed-point step from the geometric
    // one is accurate to ~0.01 deg. Below -1 deg the Sun is set: no correction.
    if (GeometricElevationDeg < -1.0)
    {
        return 0.0;
    }
    double Apparent = GeometricElevationDeg;
    double R = 0.0;
    for (int32 i = 0; i < 2; ++i)
    {
        R = 1.0 / std::tan(FMath::DegreesToRadians(Apparent + 7.31 / (Apparent + 4.4))) / 60.0;
        Apparent = GeometricElevationDeg + R;
    }
    return R;
}

double FAstroSolarGeometry::LocalMidnight(double SimSeconds, double UtcOffsetHours)
{
    // Sim seconds are counted from J2000 = 2000-01-01 11:58:55.816 UTC; shift to civil days.
    constexpr double J2000FromMidnightUtc = 11.0 * 3600.0 + 58.0 * 60.0 + 55.816;
    const double Local = SimSeconds + J2000FromMidnightUtc + UtcOffsetHours * 3600.0;
    return std::floor(Local / 86400.0) * 86400.0 - J2000FromMidnightUtc - UtcOffsetHours * 3600.0;
}

FAstroSunDay FAstroSolarGeometry::DayEvents(double DayStartSim) const
{
    FAstroSunDay Day;
    const double Horizon = bHasAtmosphere ? -0.833 : -SunSemiDiameterDeg;
    const double Span = FMath::Min(SolarDaySeconds, 30.0 * 86400.0);
    const double Step = Span / 144.0; // 10 minutes on Earth
    auto Elev = [&](double T) { return SunAt(T).ElevationDeg; };
    // Crossing times by bisection to ~1 s.
    auto Refine = [&](double A, double B)
    {
        for (int32 i = 0; i < 40 && B - A > 0.5; ++i)
        {
            const double M = 0.5 * (A + B);
            ((Elev(A) - Horizon) * (Elev(M) - Horizon) <= 0.0 ? B : A) = M;
        }
        return 0.5 * (A + B);
    };
    double Prev = Elev(DayStartSim) - Horizon;
    double BestElev = -90.0, BestT = DayStartSim;
    for (int32 i = 1; i <= 144; ++i)
    {
        const double T0 = DayStartSim + (i - 1) * Step, T1 = DayStartSim + i * Step;
        const double E = Elev(T1);
        if (E > BestElev) { BestElev = E; BestT = T1; }
        const double Cur = E - Horizon;
        if (Prev < 0.0 && Cur >= 0.0 && !Day.bRises) { Day.bRises = true; Day.SunriseSim = Refine(T0, T1); }
        if (Prev >= 0.0 && Cur < 0.0 && !Day.bSets) { Day.bSets = true; Day.SunsetSim = Refine(T0, T1); }
        Prev = Cur;
    }
    // Solar noon: golden-section search around the best sample.
    double A = BestT - Step, B = BestT + Step;
    for (int32 i = 0; i < 40 && B - A > 0.5; ++i)
    {
        const double M1 = B - 0.618034 * (B - A), M2 = A + 0.618034 * (B - A);
        (Elev(M1) < Elev(M2) ? A : B) = (Elev(M1) < Elev(M2) ? M1 : M2);
    }
    Day.SolarNoonSim = 0.5 * (A + B);
    Day.NoonElevationDeg = Elev(Day.SolarNoonSim);
    if (!Day.bRises && !Day.bSets)
    {
        Day.bPolarDay = Day.NoonElevationDeg > Horizon && Elev(DayStartSim) > Horizon;
        Day.bPolarNight = !Day.bPolarDay;
    }
    if (Day.bRises) { Day.SunriseAzimuthDeg = SunAt(Day.SunriseSim).AzimuthDeg; }
    if (Day.bSets) { Day.SunsetAzimuthDeg = SunAt(Day.SunsetSim).AzimuthDeg; }
    if (Day.bRises && Day.bSets)
    {
        const double Length = Day.SunsetSim - Day.SunriseSim;
        Day.DayLengthHours = (Length > 0.0 ? Length : Length + Span) / 3600.0;
    }
    else
    {
        Day.DayLengthHours = Day.bPolarDay ? Span / 3600.0 : 0.0;
    }
    return Day;
}

TArray<TPair<double, FAstroSunPosition>> FAstroSolarGeometry::DayPath(double DayStartSim, double StepMinutes, bool bAboveHorizonOnly) const
{
    TArray<TPair<double, FAstroSunPosition>> Path;
    const double Span = FMath::Min(SolarDaySeconds, 30.0 * 86400.0);
    const double Step = FMath::Max(StepMinutes, 1.0) * 60.0;
    for (double T = DayStartSim; T <= DayStartSim + Span + 1.0; T += Step)
    {
        const FAstroSunPosition P = SunAt(T);
        if (!bAboveHorizonOnly || P.ElevationDeg > -1.0)
        {
            Path.Emplace(T, P);
        }
    }
    return Path;
}
