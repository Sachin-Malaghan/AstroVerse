// Phase 7 validation: floating origin, rotating frames, and precision at true scale.
// See CLAUDE.md Phase 7 and "Scale and precision".
#include "Misc/AutomationTest.h"
#include "AstroRenderFrame.h"
#include "BodyRegistry.h"
#include "Math/AstroConstants.h"
#include "Math/ScaledSpace.h"
#include "Misc/Paths.h"
#include "SolarSystemSimulation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AstroFrameTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    bool Setup(FAutomationTestBase& Test, FBodyRegistry& Registry, FSolarSystemSimulation& Sim)
    {
        FString Error;
        const bool bOk = Registry.LoadFromCSVDirectory(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Bodies/DataTables")), Error);
        Test.TestTrue(FString::Printf(TEXT("Registry loads (%s)"), *Error), bOk);
        if (bOk)
        {
            Sim.Initialize(Registry, 0.0, 3600.0);
            Sim.SetMaxStepsPerAdvance(MAX_int32);
        }
        return bOk;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroFramePrecisionTest, "AstroVerse.Frames.PrecisionAt30AU", AstroFrameTests::Flags)
bool FAstroFramePrecisionTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    FSolarSystemSimulation Sim;
    if (!AstroFrameTests::Setup(*this, Registry, Sim))
    {
        return false;
    }
    const int32 Neptune = Registry.FindIndex(TEXT("Neptune"));

    // Origin 1000 km from Neptune (~30 AU from the barycenter), inertial axes.
    FAstroRenderOriginState Origin;
    Origin.SetInertial(Neptune, FAstroVector3d(1.0e6, 0.0, 0.0));
    const FAstroRenderFrame Frame = Origin.Compute(Registry, Sim);

    // A point 2 m from the viewer must survive sim -> engine (float, as the GPU sees it) -> sim.
    const FAstroVector3d Point = Frame.Origin + FAstroVector3d(1.234, -0.567, 1.89);
    const FVector EngineD = Frame.ToEnginePosition(Point);
    const FVector3f EngineF(EngineD);
    const FAstroVector3d Back = Frame.ToSimPosition(FVector(EngineF));
    const double ErrorMm = (Back - Point).Length() * 1000.0;
    AddInfo(FString::Printf(TEXT("Round trip through float at 30 AU: %.6f mm"), ErrorMm));
    TestTrue(TEXT("Sub-millimeter near the viewer at 30 AU"), ErrorMm < 0.01);

    // Neptune itself, 1000 km away, lands in engine space with float-level relative precision.
    const FVector NeptuneEngine = Frame.ToEnginePosition(Sim.GetBodyState(Neptune).Position);
    TestEqual(TEXT("Neptune is 1000 km (1e8 cm) from the origin"), NeptuneEngine.Size(), 1.0e8, 1.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroBodyFixedFrameTest, "AstroVerse.Frames.LandedGroundHoldsStill", AstroFrameTests::Flags)
bool FAstroBodyFixedFrameTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    FSolarSystemSimulation Sim;
    if (!AstroFrameTests::Setup(*this, Registry, Sim))
    {
        return false;
    }
    const int32 Earth = Registry.FindIndex(TEXT("Earth"));
    const int32 Sun = Registry.GetStarIndex();
    const double R = Registry.Get(Earth).EquatorialRadiusMeters;

    // Stand 2 m above the equator at longitude 0 (body-fixed +X).
    FAstroRenderOriginState Origin;
    Origin.SetBodyFixed(Earth, FAstroVector3d(R + 2.0, 0.0, 0.0));

    // A rock 30 m east of us, fixed to the ground.
    const FAstroVector3d RockBodyFixed(R, 30.0, 0.0);
    auto RockEngine = [&]()
    {
        const FAstroRenderFrame F = Origin.Compute(Registry, Sim);
        const FAstroVector3d RockSim = Sim.GetBodyState(Earth).Position + Registry.Get(Earth).GetOrientationAt(Sim.GetSimSeconds()) * RockBodyFixed;
        return F.ToEnginePosition(RockSim);
    };
    auto SunDirEngine = [&]()
    {
        const FAstroRenderFrame F = Origin.Compute(Registry, Sim);
        return F.ToEngineDirection((Sim.GetBodyState(Sun).Position - F.Origin).Normalized());
    };

    // Independent expectation: the Sun's direction in Earth's body-fixed frame.
    auto SunDirBodyFixed = [&]()
    {
        const FAstroVector3d ToSun = (Sim.GetBodyState(Sun).Position - Sim.GetBodyState(Earth).Position).Normalized();
        return Registry.Get(Earth).GetOrientationAt(Sim.GetSimSeconds()).Transposed() * ToSun;
    };

    const FVector RockStart = RockEngine();
    const FVector SunStart = SunDirEngine();
    const FAstroVector3d SunBodyStart = SunDirBodyFixed();
    AddInfo(FString::Printf(TEXT("Rock at %s cm"), *RockStart.ToString()));
    TestTrue(TEXT("Local up is engine +Z (rock is 2 m below us)"), FMath::IsNearlyEqual(RockStart.Z, -200.0, 1.0));
    TestTrue(TEXT("The rock is 30 m away horizontally"), FMath::IsNearlyEqual(FVector2D(RockStart).Size(), 3000.0, 1.0));

    Sim.AdvanceTo(6.0 * 3600.0);
    const FVector RockLater = RockEngine();
    const double DriftCm = (RockLater - RockStart).Size();
    const double SunTurnDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(SunStart, SunDirEngine()), -1.0, 1.0)));
    const double ExpectedDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(SunBodyStart.Dot(SunDirBodyFixed()), -1.0, 1.0)));
    AddInfo(FString::Printf(TEXT("After 6 hours: rock drifted %.4f cm; Sun moved %.2f deg across the sky (expected %.2f)"), DriftCm, SunTurnDeg, ExpectedDeg));
    TestTrue(TEXT("The ground holds still in engine space"), DriftCm < 0.1);
    // A quarter turn with the Sun at ~-23 deg declination sweeps ~81 deg of sky.
    TestTrue(TEXT("The sky turns exactly as Earth's rotation model says"), FMath::IsNearlyEqual(SunTurnDeg, ExpectedDeg, 0.05));
    TestTrue(TEXT("...which is a quarter-day's sweep"), SunTurnDeg > 75.0 && SunTurnDeg < 92.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroRebaseTest, "AstroVerse.Frames.RebaseIsSeamless", AstroFrameTests::Flags)
bool FAstroRebaseTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    FSolarSystemSimulation Sim;
    if (!AstroFrameTests::Setup(*this, Registry, Sim))
    {
        return false;
    }
    const int32 Moon = Registry.FindIndex(TEXT("Moon"));
    const int32 Earth = Registry.FindIndex(TEXT("Earth"));

    for (int32 ModeIndex = 0; ModeIndex < 2; ++ModeIndex)
    {
        FAstroRenderOriginState Origin;
        if (ModeIndex == 0)
        {
            Origin.SetInertial(Moon, FAstroVector3d(5.0e6, 0.0, 0.0));
        }
        else
        {
            Origin.SetBodyFixed(Moon, FAstroVector3d(Registry.Get(Moon).EquatorialRadiusMeters + 10.0, 0.0, 0.0));
        }
        const FAstroRenderFrame Before = Origin.Compute(Registry, Sim);
        const FVector EarthBefore = Before.ToEnginePosition(Sim.GetBodyState(Earth).Position);

        // The pawn walked 1.5 km in engine space; rebase by that offset.
        const FVector PawnOffset(120000.0, -80000.0, 5000.0);
        Origin.Shift(Before.ToSimDirection(PawnOffset) / 100.0, Registry, Sim);
        const FAstroRenderFrame After = Origin.Compute(Registry, Sim);
        const FVector EarthAfter = After.ToEnginePosition(Sim.GetBodyState(Earth).Position);

        // Everything else shifts by exactly -offset (relative precision at 384,000 km).
        const double ErrorCm = (EarthAfter - (EarthBefore - PawnOffset)).Size();
        AddInfo(FString::Printf(TEXT("%s rebase: Earth moved by -offset within %.4f cm"), ModeIndex == 0 ? TEXT("Inertial") : TEXT("Body-fixed"), ErrorCm));
        TestTrue(TEXT("Rebase shifts the world by exactly the pawn offset"), ErrorCm < 1.0);
        TestTrue(TEXT("Rebase keeps the axes (no tilt)"), (After.ToEngineDirection(FAstroVector3d(0, 0, 1)) - Before.ToEngineDirection(FAstroVector3d(0, 0, 1))).Size() < 1e-9);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
