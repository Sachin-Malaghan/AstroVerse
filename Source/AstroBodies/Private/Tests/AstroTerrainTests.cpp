// Phase 8 validation: the terrain model the walker and the terrain mesh share. See CLAUDE.md Phase 8.
#include "Misc/AutomationTest.h"
#include "BodyRegistry.h"
#include "Math/AstroConstants.h"
#include "Misc/Paths.h"
#include "AstroGeodesy.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AstroTerrainTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    FAstroVector3d LatLon(double LatDeg, double LonDeg)
    {
        const double Lat = LatDeg * AstroConstants::DegToRad, Lon = LonDeg * AstroConstants::DegToRad;
        return FAstroVector3d(FMath::Cos(Lat) * FMath::Cos(Lon), FMath::Cos(Lat) * FMath::Sin(Lon), FMath::Sin(Lat));
    }

    bool Load(FAutomationTestBase& Test, FBodyRegistry& Registry)
    {
        FString Error;
        const bool bOk = Registry.LoadFromCSVDirectory(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Bodies/DataTables")), Error);
        Test.TestTrue(FString::Printf(TEXT("Registry loads (%s)"), *Error), bOk);
        return bOk;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroTerrainBasicsTest, "AstroVerse.Terrain.ModelBasics", AstroTerrainTests::Flags)
bool FAstroTerrainBasicsTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    if (!AstroTerrainTests::Load(*this, Registry))
    {
        return false;
    }
    const FBodyDefinition& Moon = Registry.Get(Registry.FindIndex(TEXT("Moon")));
    const FBodyDefinition& Earth = Registry.Get(Registry.FindIndex(TEXT("Earth")));
    const FBodyDefinition& Jupiter = Registry.Get(Registry.FindIndex(TEXT("Jupiter")));
    if (!TestTrue(TEXT("Terrain attached to Moon, Earth, Jupiter"), Moon.Terrain.IsValid() && Earth.Terrain.IsValid() && Jupiter.Terrain.IsValid()))
    {
        return false;
    }
    TestFalse(TEXT("Jupiter has no solid surface"), Jupiter.Terrain->HasSolidSurface());
    TestTrue(TEXT("The Moon does"), Moon.Terrain->HasSolidSurface());

    // Deterministic, and continuous: sampled 4x finer, the worst step must shrink ~4x.
    // A true discontinuity (a cliff that isn't there) keeps its size at any resolution.
    const FAstroVector3d P = AstroTerrainTests::LatLon(12.3, 45.6);
    TestEqual(TEXT("Same point, same height"), Moon.Terrain->HeightAt(P), Moon.Terrain->HeightAt(P));
    struct FProfileStats { double Worst = 0.0; double RmsSlope = 0.0; double MinH = 1e30; double MaxH = -1e30; };
    auto Profile = [&](double StepMeters, int32 Steps)
    {
        FProfileStats Stats;
        double SumSq = 0.0;
        const double StepDeg = StepMeters / Moon.EquatorialRadiusMeters * AstroConstants::RadToDeg;
        double Prev = Moon.Terrain->HeightAt(AstroTerrainTests::LatLon(10.0, 30.0));
        for (int32 i = 1; i <= Steps; ++i)
        {
            const double H = Moon.Terrain->HeightAt(AstroTerrainTests::LatLon(10.0 + i * StepDeg, 30.0));
            const double Step = FMath::Abs(H - Prev);
            Stats.Worst = FMath::Max(Stats.Worst, Step);
            SumSq += FMath::Square(Step / StepMeters);
            Stats.MinH = FMath::Min(Stats.MinH, H);
            Stats.MaxH = FMath::Max(Stats.MaxH, H);
            Prev = H;
        }
        Stats.RmsSlope = FMath::Sqrt(SumSq / Steps);
        return Stats;
    };
    const FProfileStats Coarse = Profile(1.0, 20000);
    const FProfileStats Fine = Profile(0.25, 80000);
    const double RmsDeg = FMath::RadiansToDegrees(FMath::Atan(Coarse.RmsSlope));
    AddInfo(FString::Printf(TEXT("Moon, 20 km transect: height %.0f .. %.0f m; RMS slope %.1f deg; worst step %.2f m per 1 m, %.2f m per 0.25 m"),
        Coarse.MinH, Coarse.MaxH, RmsDeg, Coarse.Worst, Fine.Worst));
    TestTrue(TEXT("No discontinuities (worst step scales with sample spacing)"), Fine.Worst < Coarse.Worst * 0.4 + 0.05);
    // Lunar regolith: RMS slopes of roughly 10-20 deg at metre baselines.
    TestTrue(TEXT("Metre-scale RMS slope is lunar-like (5-25 deg)"), RmsDeg > 5.0 && RmsDeg < 25.0);
    TestTrue(TEXT("No near-vertical cliffs"), Coarse.Worst < 3.0);

    // Global relief range stays physically plausible (real lunar range ~ -9 .. +11 km).
    double GlobalMin = 1e30, GlobalMax = -1e30;
    for (int32 Lat = -80; Lat <= 80; Lat += 4)
    {
        for (int32 Lon = -180; Lon < 180; Lon += 4)
        {
            const double H = Moon.Terrain->HeightAt(AstroTerrainTests::LatLon(Lat, Lon), 100.0);
            GlobalMin = FMath::Min(GlobalMin, H);
            GlobalMax = FMath::Max(GlobalMax, H);
        }
    }
    AddInfo(FString::Printf(TEXT("Moon global relief sample: %.0f .. %.0f m"), GlobalMin, GlobalMax));
    TestTrue(TEXT("Moon relief within +-12 km"), GlobalMin > -12000.0 && GlobalMax < 12000.0 && GlobalMax - GlobalMin > 1000.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroTerrainOceanTest, "AstroVerse.Terrain.EarthOceansAreFlat", AstroTerrainTests::Flags)
bool FAstroTerrainOceanTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    if (!AstroTerrainTests::Load(*this, Registry))
    {
        return false;
    }
    const FBodyTerrain& Earth = *Registry.Get(Registry.FindIndex(TEXT("Earth"))).Terrain;
    // Mid-Pacific (0 N, 160 W) is ocean; central Sahara (23 N, 12 E) and central Asia (45 N, 90 E) are land.
    const FAstroVector3d Pacific = AstroTerrainTests::LatLon(0.0, -160.0);
    const FAstroVector3d Sahara = AstroTerrainTests::LatLon(23.0, 12.0);
    const FAstroVector3d Asia = AstroTerrainTests::LatLon(45.0, 90.0);
    TestTrue(TEXT("Mid-Pacific is ocean"), Earth.IsOcean(Pacific));
    TestEqual(TEXT("Ocean is at sea level"), Earth.HeightAt(Pacific), 0.0);
    TestFalse(TEXT("Sahara is land"), Earth.IsOcean(Sahara));
    TestFalse(TEXT("Central Asia is land"), Earth.IsOcean(Asia));
    TestTrue(TEXT("Land stands above the sea"), Earth.HeightAt(Sahara) > 0.0 && Earth.HeightAt(Asia) > 0.0);
    AddInfo(FString::Printf(TEXT("Sahara %.0f m, central Asia %.0f m"), Earth.HeightAt(Sahara), Earth.HeightAt(Asia)));

    // Oblate ellipsoid: polar radius ~21 km less than equatorial.
    TestEqual(TEXT("Equatorial radius (m)"), Earth.EllipsoidRadius(AstroTerrainTests::LatLon(0, 0)), 6378137.0, 1.0);
    TestEqual(TEXT("Polar radius (m)"), Earth.EllipsoidRadius(AstroTerrainTests::LatLon(90, 0)), 6356752.0, 1.0);
    return true;
}

// Real elevation (DEMs from Tools/Data/fetch_dems.py): landmarks land where they should, at
// the right height. Skipped with a warning when the data hasn't been fetched.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroTerrainRealElevationTest, "AstroVerse.Terrain.RealElevation", AstroTerrainTests::Flags)
bool FAstroTerrainRealElevationTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    if (!AstroTerrainTests::Load(*this, Registry))
    {
        return false;
    }
    const FBodyTerrain& Earth = *Registry.Get(Registry.FindIndex(TEXT("Earth"))).Terrain;
    const FBodyTerrain& Moon = *Registry.Get(Registry.FindIndex(TEXT("Moon"))).Terrain;
    const FBodyTerrain& Mars = *Registry.Get(Registry.FindIndex(TEXT("Mars"))).Terrain;
    if (!Earth.HasRealElevation() || !Moon.HasRealElevation() || !Mars.HasRealElevation())
    {
        AddWarning(TEXT("DEMs not present (run Tools/Data/fetch_dems.py): real-elevation checks skipped."));
        return true;
    }
    using AstroTerrainTests::LatLon;
    struct FCheck { const TCHAR* Name; const FBodyTerrain* Body; double Lat, Lon, Min, Max; };
    const FCheck Checks[] = {
        // Earth, ETOPO 2022 at 2' (~3.7 km cells average peaks down): Everest 8,849 m, the
        // Tibetan plateau ~4.5-5 km, Delhi ~215 m, the Mariana Trench ~-10.9 km.
        { TEXT("Everest region"), &Earth, 27.988, 86.925, 6000.0, 8900.0 },
        { TEXT("Tibetan plateau"), &Earth, 33.0, 88.0, 4200.0, 5600.0 },
        { TEXT("Delhi"), &Earth, 28.61, 77.21, 150.0, 300.0 },
        { TEXT("Mariana Trench"), &Earth, 11.35, 142.2, -11000.0, -9000.0 },
        // Mars, MOLA: Olympus Mons summit ~21.2 km above the areoid, Hellas floor ~-7 km.
        { TEXT("Olympus Mons"), &Mars, 18.65, -133.8, 18000.0, 22000.0 },
        { TEXT("Hellas Planitia"), &Mars, -42.4, 70.5, -7500.0, -5000.0 },
        // Moon, LOLA: Tycho's floor lies ~3 km below the reference sphere.
        { TEXT("Tycho floor"), &Moon, -43.3, -11.2, -3800.0, -2000.0 },
    };
    for (const FCheck& C : Checks)
    {
        // Each body's own latitude convention (geodetic on Earth, as maps and GPS give it).
        const FAstroVector3d Dir = AstroGeodesy::SurfaceDirection(C.Lat, C.Lon, C.Body->GetEquatorialRadius(), C.Body->GetPolarRadius(), C.Body->UsesGeodeticLatitude());
        const double H = C.Body->DEMHeightAt(Dir);
        AddInfo(FString::Printf(TEXT("%s: %.0f m"), C.Name, H));
        TestTrue(FString::Printf(TEXT("%s elevation %.0f m in [%.0f, %.0f]"), C.Name, H, C.Min, C.Max), H >= C.Min && H <= C.Max);
    }
    // Plains stay flat: over the Indo-Gangetic plain the sub-DEM detail must be damped.
    double MinH = 1e30, MaxH = -1e30;
    for (int32 i = 0; i <= 200; ++i)
    {
        const double H = Earth.HeightAt(LatLon(28.0, 77.0 + i * 0.0005)); // ~10 km transect near Delhi
        MinH = FMath::Min(MinH, H);
        MaxH = FMath::Max(MaxH, H);
    }
    AddInfo(FString::Printf(TEXT("Indo-Gangetic plain, 10 km transect: %.0f .. %.0f m"), MinH, MaxH));
    TestTrue(TEXT("Plain relief under 60 m over 10 km"), MaxH - MinH < 60.0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
