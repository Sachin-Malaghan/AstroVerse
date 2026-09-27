// Sun-at-a-site validation: the simulation-driven solar geometry (N-body Earth + IAU rotation
// + geodetic site) against the NOAA solar-position algorithm (Meeus-based spreadsheet, stated
// accuracy ~0.01 deg for this era). See CLAUDE.md "Sun at a site".
#include "Misc/AutomationTest.h"
#include "AstroSolarGeometry.h"
#include "BodyRegistry.h"
#include "SolarSystemSimulation.h"
#include "Math/AstroConstants.h"
#include "Misc/Paths.h"
#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace AstroSunTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    // Sim seconds are UTC seconds from J2000.0 (= 2000-01-01 11:58:55.816 UTC).
    double SimSecondsFor(int32 Y, int32 M, int32 D, double HourUtc)
    {
        const FDateTime J2000(2000, 1, 1, 11, 58, 55, 816);
        return (FDateTime(Y, M, D) - J2000).GetTotalSeconds() + HourUtc * 3600.0;
    }

    double JulianDayUtc(double SimSeconds) { return 2451545.0 - 64.184 / 86400.0 + SimSeconds / 86400.0; }

    // NOAA Solar Calculator (Global Monitoring Laboratory spreadsheet) formulas, UTC input.
    void NOAA(double SimSeconds, double LatDeg, double LonDeg, double& OutElevation, double& OutAzimuth, double* OutDecl = nullptr, double* OutGHA = nullptr)
    {
        auto Rad = [](double D) { return D * AstroConstants::DegToRad; };
        auto Deg = [](double R) { return R * AstroConstants::RadToDeg; };
        const double JD = JulianDayUtc(SimSeconds);
        const double JC = (JD - 2451545.0) / 36525.0;
        const double L0 = FMath::Fmod(280.46646 + JC * (36000.76983 + JC * 0.0003032), 360.0);
        const double M = 357.52911 + JC * (35999.05029 - 0.0001537 * JC);
        const double E = 0.016708634 - JC * (0.000042037 + 0.0000001267 * JC);
        const double C = FMath::Sin(Rad(M)) * (1.914602 - JC * (0.004817 + 0.000014 * JC)) + FMath::Sin(Rad(2 * M)) * (0.019993 - 0.000101 * JC)
                       + FMath::Sin(Rad(3 * M)) * 0.000289;
        const double AppLong = L0 + C - 0.00569 - 0.00478 * FMath::Sin(Rad(125.04 - 1934.136 * JC));
        const double MeanObl = 23.0 + (26.0 + (21.448 - JC * (46.815 + JC * (0.00059 - JC * 0.001813))) / 60.0) / 60.0;
        const double Obl = MeanObl + 0.00256 * FMath::Cos(Rad(125.04 - 1934.136 * JC));
        const double Decl = Deg(FMath::Asin(FMath::Sin(Rad(Obl)) * FMath::Sin(Rad(AppLong))));
        const double Y = FMath::Square(FMath::Tan(Rad(Obl / 2.0)));
        const double EqTime = 4.0 * Deg(Y * FMath::Sin(2 * Rad(L0)) - 2 * E * FMath::Sin(Rad(M)) + 4 * E * Y * FMath::Sin(Rad(M)) * FMath::Cos(2 * Rad(L0))
                                        - 0.5 * Y * Y * FMath::Sin(4 * Rad(L0)) - 1.25 * E * E * FMath::Sin(2 * Rad(M)));
        const double MinutesUtc = FMath::Fmod(FMath::Fmod(JD + 0.5, 1.0) * 1440.0 + 1440.0, 1440.0);
        if (OutDecl) { *OutDecl = Decl; }
        if (OutGHA) { *OutGHA = FMath::Fmod(MinutesUtc + EqTime + 1440.0, 1440.0) / 4.0 - 180.0; }
        const double TrueSolar = FMath::Fmod(MinutesUtc + EqTime + 4.0 * LonDeg + 1440.0, 1440.0);
        const double HA = TrueSolar / 4.0 < 0.0 ? TrueSolar / 4.0 + 180.0 : TrueSolar / 4.0 - 180.0;
        const double CosZen = FMath::Sin(Rad(LatDeg)) * FMath::Sin(Rad(Decl)) + FMath::Cos(Rad(LatDeg)) * FMath::Cos(Rad(Decl)) * FMath::Cos(Rad(HA));
        const double Zen = Deg(FMath::Acos(FMath::Clamp(CosZen, -1.0, 1.0)));
        OutElevation = 90.0 - Zen;
        const double A = Deg(FMath::Acos(FMath::Clamp((FMath::Sin(Rad(LatDeg)) * FMath::Cos(Rad(Zen)) - FMath::Sin(Rad(Decl)))
                                                       / (FMath::Cos(Rad(LatDeg)) * FMath::Sin(Rad(Zen))), -1.0, 1.0)));
        OutAzimuth = HA > 0.0 ? FMath::Fmod(A + 180.0, 360.0) : FMath::Fmod(540.0 - A, 360.0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroSunNOAATest, "AstroVerse.Sun.MatchesNOAA", AstroSunTests::Flags)
bool FAstroSunNOAATest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    FString Error;
    if (!TestTrue(TEXT("Registry loads"), Registry.LoadFromCSVDirectory(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Bodies/DataTables")), Error)))
    {
        return false;
    }
    const int32 Sun = Registry.GetStarIndex();
    const int32 Earth = Registry.FindIndex(TEXT("Earth"));

    struct FSite { const TCHAR* Name; double Lat, Lon; };
    const FSite Sites[] = { { TEXT("New Delhi"), 28.6139, 77.2090 }, { TEXT("London"), 51.5074, -0.1278 }, { TEXT("Sydney"), -33.8688, 151.2093 },
                            { TEXT("Tromso"), 69.6492, 18.9553 } };
    struct FDay { int32 Y, M, D; };
    const FDay Days[] = { { 2026, 3, 20 }, { 2026, 6, 21 }, { 2026, 9, 23 }, { 2026, 12, 21 } };

    double WorstEl = 0.0, WorstAz = 0.0;
    for (const FDay& Day : Days)
    {
        // Simulation state at this day's 00:00 UTC; the geometry propagates across the day.
        const double Start = AstroSunTests::SimSecondsFor(Day.Y, Day.M, Day.D, 0.0);
        FSolarSystemSimulation Sim;
        Sim.Initialize(Registry, Start, 3600.0);
        FOrbitalState Rel;
        Rel.Position = Sim.GetBodyState(Earth).Position - Sim.GetBodyState(Sun).Position;
        Rel.Velocity = Sim.GetBodyState(Earth).Velocity - Sim.GetBodyState(Sun).Velocity;
        FAstroSolarGeometry Geometry(Registry.Get(Earth), Rel, Registry.Get(Sun).GM + Registry.Get(Earth).GM, Start);
        for (const FSite& Site : Sites)
        {
            Geometry.SetSite(Site.Lat, Site.Lon);
            for (double Hour = 0.0; Hour < 24.0; Hour += 1.5)
            {
                const double T = Start + Hour * 3600.0;
                const FAstroSunPosition P = Geometry.SunAt(T);
                double El, Az;
                AstroSunTests::NOAA(T, Site.Lat, Site.Lon, El, Az);
                WorstEl = FMath::Max(WorstEl, FMath::Abs(P.ElevationDeg - El));
                if (El > 2.0 && El < 88.0) // azimuth is ill-conditioned near the zenith and meaningless below the horizon
                {
                    double DAz = FMath::Abs(P.AzimuthDeg - Az);
                    WorstAz = FMath::Max(WorstAz, FMath::Min(DAz, 360.0 - DAz));
                }
            }
        }
    }
    // Diagnostics: split the difference into declination (orbit / pole) and Greenwich hour angle (rotation).
    for (const FDay& Day : Days)
    {
        const double Start = AstroSunTests::SimSecondsFor(Day.Y, Day.M, Day.D, 0.0);
        FSolarSystemSimulation Sim;
        Sim.Initialize(Registry, Start, 3600.0);
        const FAstroVector3d SunRel = Sim.GetBodyState(Sun).Position - Sim.GetBodyState(Earth).Position;
        const FAstroVector3d BF = (Registry.Get(Earth).GetOrientationAt(Start).Transposed() * SunRel).Normalized();
        const double DecOurs = FMath::RadiansToDegrees(std::asin(BF.Z));
        const double GHAOurs = -FMath::RadiansToDegrees(std::atan2(BF.Y, BF.X));
        double El, Az, Dec, GHA;
        AstroSunTests::NOAA(Start, 0, 0, El, Az, &Dec, &GHA);
        AddInfo(FString::Printf(TEXT("%04d-%02d-%02d 00UTC: dec ours %.3f NOAA %.3f (d %.3f); GHA ours %.3f NOAA %.3f (d %.3f)"),
            Day.Y, Day.M, Day.D, DecOurs, Dec, DecOurs - Dec, GHAOurs, GHA, FMath::Fmod(GHAOurs - GHA + 540.0, 360.0) - 180.0));
    }
    AddInfo(FString::Printf(TEXT("Worst difference vs NOAA over 4 sites x 4 dates x 16 times: elevation %.3f deg, azimuth %.3f deg"), WorstEl, WorstAz));
    TestTrue(TEXT("Elevation within 0.1 deg of NOAA"), WorstEl < 0.1);
    TestTrue(TEXT("Azimuth within 0.15 deg of NOAA"), WorstAz < 0.15);

    // Day events, New Delhi on the June solstice (NOAA: sunrise ~05:24 IST, sunset ~19:22 IST).
    {
        const double Start = AstroSunTests::SimSecondsFor(2026, 6, 21, 0.0);
        FSolarSystemSimulation Sim;
        Sim.Initialize(Registry, Start, 3600.0);
        FOrbitalState Rel;
        Rel.Position = Sim.GetBodyState(Earth).Position - Sim.GetBodyState(Sun).Position;
        Rel.Velocity = Sim.GetBodyState(Earth).Velocity - Sim.GetBodyState(Sun).Velocity;
        FAstroSolarGeometry Geometry(Registry.Get(Earth), Rel, Registry.Get(Sun).GM + Registry.Get(Earth).GM, Start);
        Geometry.SetSite(28.6139, 77.2090);
        const double Midnight = FAstroSolarGeometry::LocalMidnight(Start + 12 * 3600.0, 5.5);
        const FAstroSunDay D = Geometry.DayEvents(Midnight);
        auto LocalHours = [&](double T) { return (T - Midnight) / 3600.0; };
        AddInfo(FString::Printf(TEXT("New Delhi 2026-06-21 (IST): sunrise %.3f h, noon %.3f h (elev %.2f), sunset %.3f h, day %.2f h"),
            LocalHours(D.SunriseSim), LocalHours(D.SolarNoonSim), D.NoonElevationDeg, LocalHours(D.SunsetSim), D.DayLengthHours));
        TestTrue(TEXT("Sun rises and sets"), D.bRises && D.bSets);
        TestEqual(TEXT("Sunrise ~05:24 IST"), LocalHours(D.SunriseSim), 5.0 + 24.0 / 60.0, 3.0 / 60.0);
        TestEqual(TEXT("Sunset ~19:22 IST"), LocalHours(D.SunsetSim), 19.0 + 22.0 / 60.0, 3.0 / 60.0);
        TestEqual(TEXT("Noon elevation 90 - |28.61 - 23.44|"), D.NoonElevationDeg, 90.0 - (28.6139 - 23.438), 0.1);
        TestEqual(TEXT("Solar day is 24 h on Earth"), Geometry.GetSolarDaySeconds(), 86400.0, 60.0);
    }
    // Midnight sun: Tromso has no sunset at the June solstice.
    {
        const double Start = AstroSunTests::SimSecondsFor(2026, 6, 21, 0.0);
        FSolarSystemSimulation Sim;
        Sim.Initialize(Registry, Start, 3600.0);
        FOrbitalState Rel;
        Rel.Position = Sim.GetBodyState(Earth).Position - Sim.GetBodyState(Sun).Position;
        Rel.Velocity = Sim.GetBodyState(Earth).Velocity - Sim.GetBodyState(Sun).Velocity;
        FAstroSolarGeometry Geometry(Registry.Get(Earth), Rel, Registry.Get(Sun).GM + Registry.Get(Earth).GM, Start);
        Geometry.SetSite(69.6492, 18.9553);
        const FAstroSunDay D = Geometry.DayEvents(FAstroSolarGeometry::LocalMidnight(Start, 2.0));
        TestTrue(TEXT("Tromso: polar day at the June solstice"), D.bPolarDay && !D.bSets);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
