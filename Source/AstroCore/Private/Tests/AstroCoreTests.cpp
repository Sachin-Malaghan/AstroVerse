// Phase 1 validation for AstroCore math, integrator and clock. See CLAUDE.md Phase 1.
// Run: UnrealEditor-Cmd AstroVerse.uproject -ExecCmds="Automation RunTests AstroVerse.Core; Quit"
#include "Misc/AutomationTest.h"
#include "Math/AstroVector3d.h"
#include "Physics/NBodyIntegrator.h"
#include "Time/SimClock.h"
#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace AstroCoreTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    constexpr double SunMass = 1.98847e30;   // kg
    constexpr double EarthMass = 5.9722e24;  // kg
    constexpr double AU = 1.495978707e11;    // m

    // Sun + Earth on a circular orbit about their barycenter (zero net momentum).
    FNBodyIntegrator MakeSunEarth()
    {
        const double G = FNBodyIntegrator::GravitationalConstant;
        const double RelativeSpeed = std::sqrt(G * (SunMass + EarthMass) / AU);
        const double TotalMass = SunMass + EarthMass;

        FMassiveBodyState Sun;
        Sun.Mass = SunMass;
        Sun.Position = FAstroVector3d(-AU * EarthMass / TotalMass, 0.0, 0.0);
        Sun.Velocity = FAstroVector3d(0.0, -RelativeSpeed * EarthMass / TotalMass, 0.0);

        FMassiveBodyState Earth;
        Earth.Mass = EarthMass;
        Earth.Position = FAstroVector3d(AU * SunMass / TotalMass, 0.0, 0.0);
        Earth.Velocity = FAstroVector3d(0.0, RelativeSpeed * SunMass / TotalMass, 0.0);

        FNBodyIntegrator Integrator;
        Integrator.AddBody(Sun);
        Integrator.AddBody(Earth);
        return Integrator;
    }

    double OrbitalPeriod()
    {
        return 2.0 * PI * std::sqrt(AU * AU * AU / (FNBodyIntegrator::GravitationalConstant * (SunMass + EarthMass)));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroVector3dTest, "AstroVerse.Core.Vector3d", AstroCoreTests::Flags)
bool FAstroVector3dTest::RunTest(const FString& Parameters)
{
    const FAstroVector3d A(1.0, 2.0, 3.0);
    const FAstroVector3d B(4.0, -5.0, 6.0);

    const FAstroVector3d Sum = A + B;
    TestTrue(TEXT("Addition"), Sum.X == 5.0 && Sum.Y == -3.0 && Sum.Z == 9.0);
    const FAstroVector3d Diff = A - B;
    TestTrue(TEXT("Subtraction"), Diff.X == -3.0 && Diff.Y == 7.0 && Diff.Z == -3.0);
    const FAstroVector3d Scaled = A * 2.0;
    TestTrue(TEXT("Scalar multiply"), Scaled.X == 2.0 && Scaled.Y == 4.0 && Scaled.Z == 6.0);
    TestEqual(TEXT("Dot"), A.Dot(B), 12.0);
    const FAstroVector3d Cross = FAstroVector3d(1, 0, 0).Cross(FAstroVector3d(0, 1, 0));
    TestTrue(TEXT("Cross X*Y = Z"), Cross.X == 0.0 && Cross.Y == 0.0 && Cross.Z == 1.0);
    TestEqual(TEXT("Length"), FAstroVector3d(3.0, 4.0, 0.0).Length(), 5.0);
    TestEqual(TEXT("Normalized length"), B.Normalized().Length(), 1.0, 1e-15);
    TestEqual(TEXT("Normalized zero stays zero"), FAstroVector3d().Normalized().Length(), 0.0);

    // Astronomical magnitudes must not lose precision the way float would.
    const FAstroVector3d Far(4.5e12, 0.0, 0.0); // ~Neptune distance in meters
    TestEqual(TEXT("Meter precision at 30 AU"), (Far + FAstroVector3d(1.0, 0.0, 0.0) - Far).X, 1.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroOrbitClosureTest, "AstroVerse.Core.NBody.OrbitClosesAfterOnePeriod", AstroCoreTests::Flags)
bool FAstroOrbitClosureTest::RunTest(const FString& Parameters)
{
    FNBodyIntegrator Integrator = AstroCoreTests::MakeSunEarth();
    const FAstroVector3d Start = Integrator.GetStates()[1].Position;

    const double Period = AstroCoreTests::OrbitalPeriod();
    const double Dt = 3600.0;
    const int32 FullSteps = static_cast<int32>(Period / Dt);
    for (int32 i = 0; i < FullSteps; ++i)
    {
        Integrator.Step(Dt);
    }
    Integrator.Step(Period - FullSteps * Dt);

    const double Error = (Integrator.GetStates()[1].Position - Start).Length();
    AddInfo(FString::Printf(TEXT("Earth position error after one year: %.3f km"), Error / 1000.0));
    TestTrue(TEXT("Earth returns within 1e-5 AU of its start"), Error < 1e-5 * AstroCoreTests::AU);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroEnergyConservationTest, "AstroVerse.Core.NBody.EnergyBoundedOverCentury", AstroCoreTests::Flags)
bool FAstroEnergyConservationTest::RunTest(const FString& Parameters)
{
    FNBodyIntegrator Integrator = AstroCoreTests::MakeSunEarth();
    const double E0 = Integrator.ComputeTotalEnergy();

    // 100 years at 1-hour steps. RK4 at this step size drifts secularly;
    // Leapfrog's error stays bounded, which is the whole reason it was chosen.
    const double Dt = 3600.0;
    const int32 Steps = static_cast<int32>(100.0 * AstroCoreTests::OrbitalPeriod() / Dt);
    double MaxRelativeError = 0.0;
    for (int32 i = 0; i < Steps; ++i)
    {
        Integrator.Step(Dt);
        if (i % 1000 == 0)
        {
            MaxRelativeError = FMath::Max(MaxRelativeError, std::abs((Integrator.ComputeTotalEnergy() - E0) / E0));
        }
    }
    const double FinalRelativeError = std::abs((Integrator.ComputeTotalEnergy() - E0) / E0);
    AddInfo(FString::Printf(TEXT("Relative energy error: max %.3e, final %.3e"), MaxRelativeError, FinalRelativeError));
    TestTrue(TEXT("Energy error stays below 1e-6 over 100 years"), MaxRelativeError < 1e-6 && FinalRelativeError < 1e-6);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroTimeReversalTest, "AstroVerse.Core.NBody.TimeReversible", AstroCoreTests::Flags)
bool FAstroTimeReversalTest::RunTest(const FString& Parameters)
{
    FNBodyIntegrator Integrator = AstroCoreTests::MakeSunEarth();
    const FAstroVector3d Start = Integrator.GetStates()[1].Position;

    const double Dt = 3600.0;
    const int32 Steps = 24 * 365 * 10;
    for (int32 i = 0; i < Steps; ++i)
    {
        Integrator.Step(Dt);
    }
    for (int32 i = 0; i < Steps; ++i)
    {
        Integrator.Step(-Dt);
    }

    const double Error = (Integrator.GetStates()[1].Position - Start).Length();
    AddInfo(FString::Printf(TEXT("Round-trip error after 10 years forward + back: %.6f m"), Error));
    TestTrue(TEXT("Rewind retraces to within 1e-6 AU"), Error < 1e-6 * AstroCoreTests::AU);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroSimClockTest, "AstroVerse.Core.SimClock", AstroCoreTests::Flags)
bool FAstroSimClockTest::RunTest(const FString& Parameters)
{
    FSimClock Clock;
    Clock.Advance(2.0);
    TestEqual(TEXT("Default scale is real time"), Clock.GetSimulatedSeconds(), 2.0);

    Clock.SetTimeScale(86400.0);
    Clock.Advance(0.5);
    TestEqual(TEXT("Time scale multiplies"), Clock.GetSimulatedSeconds(), 2.0 + 43200.0);

    Clock.SetPaused(true);
    Clock.Advance(10.0);
    TestEqual(TEXT("Paused clock holds"), Clock.GetSimulatedSeconds(), 2.0 + 43200.0);

    Clock.SetPaused(false);
    Clock.SetTimeScale(-86400.0);
    Clock.Advance(0.5);
    TestEqual(TEXT("Negative scale rewinds"), Clock.GetSimulatedSeconds(), 2.0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
