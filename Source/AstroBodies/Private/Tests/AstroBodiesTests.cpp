// Phase 2 validation: data tables, seeding, and the solar-system simulation
// against known astronomical facts. See CLAUDE.md Phase 2.
#include "Misc/AutomationTest.h"
#include "BodyRegistry.h"
#include "Math/AstroConstants.h"
#include "Misc/Paths.h"
#include "SolarSystemSimulation.h"
#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace AstroBodiesTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
    constexpr double AU = AstroConstants::AstronomicalUnit;
    constexpr double Day = AstroConstants::SecondsPerDay;

    bool LoadRegistry(FAutomationTestBase& Test, FBodyRegistry& Registry)
    {
        FString Error;
        const bool bOk = Registry.LoadFromCSVDirectory(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Bodies/DataTables")), Error);
        Test.TestTrue(FString::Printf(TEXT("Registry loads (%s)"), *Error), bOk);
        return bOk;
    }

    FAstroVector3d Relative(const FSolarSystemSimulation& Sim, int32 Body, int32 To)
    {
        return Sim.GetBodyState(Body).Position - Sim.GetBodyState(To).Position;
    }

    double EclipticLongitudeDeg(const FAstroVector3d& V)
    {
        double Deg = std::atan2(V.Y, V.X) * AstroConstants::RadToDeg;
        return Deg < 0.0 ? Deg + 360.0 : Deg;
    }

    double AngleBetweenDeg(const FAstroVector3d& A, const FAstroVector3d& B)
    {
        return std::acos(FMath::Clamp(A.Normalized().Dot(B.Normalized()), -1.0, 1.0)) * AstroConstants::RadToDeg;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroRegistryTest, "AstroVerse.Bodies.RegistryLoads", AstroBodiesTests::Flags)
bool FAstroRegistryTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    if (!AstroBodiesTests::LoadRegistry(*this, Registry))
    {
        return false;
    }
    TestEqual(TEXT("Sun + 8 planets + 8 moons"), Registry.Num(), 17);

    const int32 Sun = Registry.FindIndex(TEXT("Sun"));
    const int32 Earth = Registry.FindIndex(TEXT("Earth"));
    const int32 Moon = Registry.FindIndex(TEXT("Moon"));
    const int32 Titan = Registry.FindIndex(TEXT("Titan"));
    TestEqual(TEXT("Sun is the star"), Registry.GetStarIndex(), Sun);
    TestEqual(TEXT("Earth orbits the Sun"), Registry.Get(Earth).ParentIndex, Sun);
    TestEqual(TEXT("Moon orbits Earth"), Registry.Get(Moon).ParentIndex, Earth);
    TestEqual(TEXT("Titan orbits Saturn"), Registry.Get(Titan).ParentIndex, Registry.FindIndex(TEXT("Saturn")));
    TestEqual(TEXT("Jupiter has 4 moons"), Registry.Get(Registry.FindIndex(TEXT("Jupiter"))).ChildIndices.Num(), 4);

    TestEqual(TEXT("Earth mass from GM/G (kg)"), Registry.Get(Earth).MassKg, 5.9722e24, 0.001e24);
    TestEqual(TEXT("Sun mass from GM/G (kg)"), Registry.Get(Sun).MassKg, 1.98841e30, 0.0001e30);

    // Earth's axis is tilted 23.44 deg from the ecliptic pole. Uranus's IAU north pole (north side of the
    // invariable plane) is 82.2 deg from ecliptic north; it spins retrograde about it, so its obliquity is ~97.8 deg.
    const FAstroVector3d EclipticNorth(0.0, 0.0, 1.0);
    TestEqual(TEXT("Earth obliquity (deg)"), AstroBodiesTests::AngleBetweenDeg(Registry.Get(Earth).EquatorFrameToEcliptic.GetColumn(2), EclipticNorth), 23.44, 0.01);
    TestEqual(TEXT("Uranus IAU pole vs ecliptic north (deg)"),
        AstroBodiesTests::AngleBetweenDeg(Registry.Get(Registry.FindIndex(TEXT("Uranus"))).EquatorFrameToEcliptic.GetColumn(2), EclipticNorth), 82.23, 0.2);
    // Titan orbits in Saturn's equatorial plane, ~26.7 deg to Saturn's orbit.
    const FOrbitalState TitanRel = Registry.ComputeRelativeStateAt(Titan, 0.0);
    const FAstroVector3d TitanNormal = TitanRel.Position.Cross(TitanRel.Velocity);
    TestTrue(TEXT("Titan's orbit is tilted ~27 deg to the ecliptic"),
        FMath::IsNearlyEqual(AstroBodiesTests::AngleBetweenDeg(TitanNormal, EclipticNorth), 27.0, 1.5));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroJ2000PositionsTest, "AstroVerse.Bodies.J2000Positions", AstroBodiesTests::Flags)
bool FAstroJ2000PositionsTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    if (!AstroBodiesTests::LoadRegistry(*this, Registry))
    {
        return false;
    }
    FSolarSystemSimulation Sim;
    Sim.Initialize(Registry, 0.0, 3600.0);

    const int32 Sun = Registry.FindIndex(TEXT("Sun"));
    const int32 Earth = Registry.FindIndex(TEXT("Earth"));
    const int32 Moon = Registry.FindIndex(TEXT("Moon"));

    // Reference (JPL Horizons, J2000 heliocentric ecliptic): Earth at longitude ~100.4 deg, 0.9833 AU.
    const FAstroVector3d EarthHelio = AstroBodiesTests::Relative(Sim, Earth, Sun);
    AddInfo(FString::Printf(TEXT("Earth at J2000: lon %.3f deg, r %.5f AU"), AstroBodiesTests::EclipticLongitudeDeg(EarthHelio), EarthHelio.Length() / AstroBodiesTests::AU));
    TestEqual(TEXT("Earth heliocentric longitude at J2000 (deg)"), AstroBodiesTests::EclipticLongitudeDeg(EarthHelio), 100.4, 0.3);
    TestEqual(TEXT("Earth-Sun distance at J2000 (AU)"), EarthHelio.Length() / AstroBodiesTests::AU, 0.9833, 0.001);

    // Moon at J2000: ~402,000 km from Earth.
    const double MoonDistKm = AstroBodiesTests::Relative(Sim, Moon, Earth).Length() / 1000.0;
    AddInfo(FString::Printf(TEXT("Earth-Moon distance at J2000: %.0f km"), MoonDistKm));
    TestTrue(TEXT("Moon distance is between perigee and apogee"), MoonDistKm > 356000.0 && MoonDistKm < 407000.0);

    // The barycenter stays at the origin with zero momentum.
    FAstroVector3d Momentum;
    for (int32 i = 0; i < Registry.Num(); ++i)
    {
        Momentum += Sim.GetBodyState(i).Velocity * Registry.Get(i).MassKg;
    }
    TestTrue(TEXT("Net momentum ~0"), Momentum.Length() < 1e-9 * Registry.Get(Sun).MassKg);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroOrbitalPeriodsTest, "AstroVerse.Bodies.OrbitalPeriods", AstroBodiesTests::Flags)
bool FAstroOrbitalPeriodsTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    if (!AstroBodiesTests::LoadRegistry(*this, Registry))
    {
        return false;
    }
    FSolarSystemSimulation Sim;
    Sim.Initialize(Registry, 0.0, 3600.0);
    Sim.SetMaxStepsPerAdvance(MAX_int32);

    const int32 Sun = Registry.FindIndex(TEXT("Sun"));
    const int32 Earth = Registry.FindIndex(TEXT("Earth"));
    const int32 Jupiter = Registry.FindIndex(TEXT("Jupiter"));
    const int32 Moon = Registry.FindIndex(TEXT("Moon"));

    const FAstroVector3d EarthStart = AstroBodiesTests::Relative(Sim, Earth, Sun);
    const FAstroVector3d JupiterStart = AstroBodiesTests::Relative(Sim, Jupiter, Sun);

    // Moon over one month: stays bound between perigee and apogee.
    double MinMoon = 1e30, MaxMoon = 0.0;
    for (int32 Hour = 0; Hour < 24 * 30; ++Hour)
    {
        Sim.AdvanceTo(Hour * 3600.0);
        const double D = AstroBodiesTests::Relative(Sim, Moon, Earth).Length() / 1000.0;
        MinMoon = FMath::Min(MinMoon, D);
        MaxMoon = FMath::Max(MaxMoon, D);
    }
    AddInfo(FString::Printf(TEXT("Moon distance over 30 days: %.0f - %.0f km"), MinMoon, MaxMoon));
    TestTrue(TEXT("Moon perigee/apogee in real range"), MinMoon > 355000.0 && MaxMoon < 408000.0 && MaxMoon - MinMoon > 30000.0);

    // Earth after one sidereal year (365.256 d) is back at its starting direction.
    Sim.AdvanceTo(365.256363 * AstroBodiesTests::Day);
    const double EarthMiss = AstroBodiesTests::AngleBetweenDeg(AstroBodiesTests::Relative(Sim, Earth, Sun), EarthStart);
    AddInfo(FString::Printf(TEXT("Earth direction error after a sidereal year: %.4f deg"), EarthMiss));
    TestTrue(TEXT("Earth returns within 0.1 deg after a sidereal year"), EarthMiss < 0.1);

    // Jupiter after 11.862 years is back within ~1 deg (Saturn perturbs it).
    Sim.AdvanceTo(11.862 * 365.25 * AstroBodiesTests::Day);
    const double JupiterMiss = AstroBodiesTests::AngleBetweenDeg(AstroBodiesTests::Relative(Sim, Jupiter, Sun), JupiterStart);
    AddInfo(FString::Printf(TEXT("Jupiter direction error after 11.862 years: %.4f deg"), JupiterMiss));
    TestTrue(TEXT("Jupiter period ~11.86 years"), JupiterMiss < 1.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroRewindTest, "AstroVerse.Bodies.RewindRetraces", AstroBodiesTests::Flags)
bool FAstroRewindTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    if (!AstroBodiesTests::LoadRegistry(*this, Registry))
    {
        return false;
    }
    FSolarSystemSimulation Sim;
    Sim.Initialize(Registry, 0.0, 3600.0);
    Sim.SetMaxStepsPerAdvance(MAX_int32);

    TArray<FBodyInstantState> Start = Sim.GetBodyStates();
    // Forward 2 years in irregular frame-like increments, then back to 0.
    double T = 0.0;
    while (T < 2.0 * 365.25 * AstroBodiesTests::Day)
    {
        T += 5000.0 + 37.0 * FMath::Fmod(T, 11.0);
        Sim.AdvanceTo(T);
    }
    while (T > 0.0)
    {
        T = FMath::Max(0.0, T - 7919.0);
        Sim.AdvanceTo(T);
    }

    double WorstKm = 0.0;
    for (int32 i = 0; i < Start.Num(); ++i)
    {
        WorstKm = FMath::Max(WorstKm, (Sim.GetBodyState(i).Position - Start[i].Position).Length() / 1000.0);
    }
    AddInfo(FString::Printf(TEXT("Worst position error after 2 years forward + back: %.6f km"), WorstKm));
    TestTrue(TEXT("Rewind retraces within 1 km"), WorstKm < 1.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroInterpolationTest, "AstroVerse.Bodies.InterpolationIsSmooth", AstroBodiesTests::Flags)
bool FAstroInterpolationTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    if (!AstroBodiesTests::LoadRegistry(*this, Registry))
    {
        return false;
    }
    FSolarSystemSimulation Sim;
    Sim.Initialize(Registry, 0.0, 3600.0);
    const int32 Earth = Registry.FindIndex(TEXT("Earth"));

    // Sample finely across several step boundaries: finite-difference velocity must match the reported velocity.
    double WorstRelative = 0.0;
    const double H = 60.0;
    for (double T = 1000.0; T < 4.0 * 3600.0; T += 613.0)
    {
        Sim.AdvanceTo(T);
        const FAstroVector3d P0 = Sim.GetBodyState(Earth).Position;
        const FAstroVector3d V0 = Sim.GetBodyState(Earth).Velocity;
        Sim.AdvanceTo(T + H);
        const FAstroVector3d FiniteDiff = (Sim.GetBodyState(Earth).Position - P0) / H;
        WorstRelative = FMath::Max(WorstRelative, (FiniteDiff - V0).Length() / V0.Length());
    }
    AddInfo(FString::Printf(TEXT("Worst velocity mismatch: %.3e"), WorstRelative));
    TestTrue(TEXT("Interpolated motion is smooth and consistent"), WorstRelative < 1e-4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroAnalyticFallbackTest, "AstroVerse.Bodies.AnalyticFallbackOnHugeJump", AstroBodiesTests::Flags)
bool FAstroAnalyticFallbackTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    if (!AstroBodiesTests::LoadRegistry(*this, Registry))
    {
        return false;
    }
    FSolarSystemSimulation Sim;
    Sim.Initialize(Registry, 0.0, 3600.0);
    Sim.SetMaxStepsPerAdvance(100);

    // Reference: the same 50-year span integrated properly.
    FSolarSystemSimulation Reference;
    Reference.Initialize(Registry, 50.0 * 365.25 * AstroBodiesTests::Day, 3600.0);

    Sim.AdvanceTo(50.0 * 365.25 * AstroBodiesTests::Day);
    TestTrue(TEXT("Jump used the analytic fallback"), Sim.UsedAnalyticFallbackLastAdvance());

    const int32 Sun = Registry.FindIndex(TEXT("Sun"));
    for (const TCHAR* Name : { TEXT("Earth"), TEXT("Jupiter"), TEXT("Neptune") })
    {
        const int32 Body = Registry.FindIndex(Name);
        const double Miss = AstroBodiesTests::AngleBetweenDeg(AstroBodiesTests::Relative(Sim, Body, Sun), AstroBodiesTests::Relative(Reference, Body, Sun));
        AddInfo(FString::Printf(TEXT("%s: analytic vs N-body after 50 years differs by %.3f deg"), Name, Miss));
        TestTrue(FString::Printf(TEXT("%s stays close to the N-body answer"), Name), Miss < 5.0);
    }

    // Normal stepping resumes afterwards.
    Sim.AdvanceTo(50.0 * 365.25 * AstroBodiesTests::Day + 3600.0);
    TestFalse(TEXT("Small advance uses N-body again"), Sim.UsedAnalyticFallbackLastAdvance());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroMoonPromotionTest, "AstroVerse.Bodies.MoonPromotionIsSeamless", AstroBodiesTests::Flags)
bool FAstroMoonPromotionTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    if (!AstroBodiesTests::LoadRegistry(*this, Registry))
    {
        return false;
    }
    FSolarSystemSimulation Sim;
    Sim.Initialize(Registry, 0.0, 600.0);
    Sim.SetMaxStepsPerAdvance(MAX_int32);
    const int32 Earth = Registry.FindIndex(TEXT("Earth"));
    const int32 Moon = Registry.FindIndex(TEXT("Moon"));

    Sim.AdvanceTo(10.0 * AstroBodiesTests::Day);
    const FAstroVector3d MoonBefore = Sim.GetBodyState(Moon).Position;
    const FAstroVector3d EarthBefore = Sim.GetBodyState(Earth).Position;

    Sim.SetMoonUsesNBody(Moon, true);
    TestTrue(TEXT("Moon is now an N-body particle"), Sim.DoesBodyUseNBody(Moon));
    TestTrue(TEXT("Promotion does not move the Moon"), (Sim.GetBodyState(Moon).Position - MoonBefore).Length() < 1.0);
    TestTrue(TEXT("Promotion does not move Earth"), (Sim.GetBodyState(Earth).Position - EarthBefore).Length() < 1.0);

    // Under full N-body the Moon stays bound for a month.
    double MaxKm = 0.0, MinKm = 1e30;
    for (int32 Hour = 1; Hour <= 24 * 30; ++Hour)
    {
        Sim.AdvanceTo(10.0 * AstroBodiesTests::Day + Hour * 3600.0);
        const double D = AstroBodiesTests::Relative(Sim, Moon, Earth).Length() / 1000.0;
        MaxKm = FMath::Max(MaxKm, D);
        MinKm = FMath::Min(MinKm, D);
    }
    AddInfo(FString::Printf(TEXT("N-body Moon distance over 30 days: %.0f - %.0f km"), MinKm, MaxKm));
    // Real range is ~356,400-406,700 km. Seeding N-body from *mean* elements (not osculating state
    // vectors) widens it by ~1%; replacing the Moon row with a JPL Horizons state vector removes that.
    TestTrue(TEXT("N-body Moon stays near its real distance range"), MinKm > 350000.0 && MaxKm < 415000.0);

    const FAstroVector3d MoonBeforeDemote = Sim.GetBodyState(Moon).Position;
    Sim.SetMoonUsesNBody(Moon, false);
    TestFalse(TEXT("Moon demoted"), Sim.DoesBodyUseNBody(Moon));
    TestTrue(TEXT("Demotion does not move the Moon"), (Sim.GetBodyState(Moon).Position - MoonBeforeDemote).Length() < 1.0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
