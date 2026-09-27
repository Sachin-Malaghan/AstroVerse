// Phase 8 validation: the terrain model the walker and the terrain mesh share. See CLAUDE.md Phase 8.
#include "Misc/AutomationTest.h"
#include "BodyRegistry.h"
#include "Math/AstroConstants.h"
#include "Misc/Paths.h"

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
    auto WorstStep = [&](double StepMeters, int32 Steps, double& OutMin, double& OutMax, double& OutAt)
    {
        double Worst = 0.0;
        OutMin = 1e30; OutMax = -1e30;
        const double StepDeg = StepMeters / Moon.EquatorialRadiusMeters * AstroConstants::RadToDeg;
        double Prev = Moon.Terrain->HeightAt(AstroTerrainTests::LatLon(10.0, 30.0));
        for (int32 i = 1; i <= Steps; ++i)
        {
            const double H = Moon.Terrain->HeightAt(AstroTerrainTests::LatLon(10.0 + i * StepDeg, 30.0));
            if (FMath::Abs(H - Prev) > Worst)
            {
                Worst = FMath::Abs(H - Prev);
                OutAt = i * StepMeters;
            }
            OutMin = FMath::Min(OutMin, H);
            OutMax = FMath::Max(OutMax, H);
            Prev = H;
        }
        return Worst;
    };
    double MinH, MaxH, At = 0.0, Unused, UnusedAt;
    const double Coarse = WorstStep(1.0, 20000, MinH, MaxH, At);
    const double Fine = WorstStep(0.25, 80000, Unused, Unused, UnusedAt);
    AddInfo(FString::Printf(TEXT("Moon, 20 km transect: height %.0f .. %.0f m; worst step %.2f m per 1 m (at %.0f m), %.2f m per 0.25 m"),
        MinH, MaxH, Coarse, At, Fine));
    TestTrue(TEXT("No discontinuities (worst step scales with sample spacing)"), Fine < Coarse * 0.4 + 0.05);
    TestTrue(TEXT("No cliffs steeper than ~70 deg"), Coarse < 3.0);

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

#endif // WITH_DEV_AUTOMATION_TESTS
