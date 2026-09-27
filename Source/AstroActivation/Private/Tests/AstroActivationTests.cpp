// Phase 5 validation: tier membership, the 3-lock cap, and reference frames. See CLAUDE.md Phase 5.
#include "Misc/AutomationTest.h"
#include "ActivationManager.h"
#include "BodyRegistry.h"
#include "Math/AstroConstants.h"
#include "Misc/Paths.h"
#include "SolarSystemSimulation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AstroActivationTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    bool Load(FAutomationTestBase& Test, FBodyRegistry& Registry)
    {
        FString Error;
        const bool bOk = Registry.LoadFromCSVDirectory(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Bodies/DataTables")), Error);
        Test.TestTrue(FString::Printf(TEXT("Registry loads (%s)"), *Error), bOk);
        return bOk;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroLockCapTest, "AstroVerse.Activation.LockCapDemotesOldest", AstroActivationTests::Flags)
bool FAstroLockCapTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    if (!AstroActivationTests::Load(*this, Registry))
    {
        return false;
    }
    FActivationManager Manager;
    Manager.Initialize(&Registry);

    TestEqual(TEXT("1st lock evicts nothing"), Manager.LockObservationTarget(TEXT("Moon")), FName(NAME_None));
    TestEqual(TEXT("2nd lock evicts nothing"), Manager.LockObservationTarget(TEXT("Io")), FName(NAME_None));
    TestEqual(TEXT("3rd lock evicts nothing"), Manager.LockObservationTarget(TEXT("Titan")), FName(NAME_None));
    TestEqual(TEXT("4th lock evicts the oldest"), Manager.LockObservationTarget(TEXT("Europa")), FName(TEXT("Moon")));
    TestEqual(TEXT("Never more than 3 locks"), Manager.GetLockedTargets().Num(), 3);

    // Re-locking refreshes Io to newest, so Titan becomes the oldest.
    TestEqual(TEXT("Re-lock evicts nothing"), Manager.LockObservationTarget(TEXT("Io")), FName(NAME_None));
    TestEqual(TEXT("Next eviction is Titan"), Manager.LockObservationTarget(TEXT("Phobos")), FName(TEXT("Titan")));
    TestEqual(TEXT("Unknown bodies are ignored"), Manager.LockObservationTarget(TEXT("Vulcan")), FName(NAME_None));
    TestEqual(TEXT("Still 3 locks"), Manager.GetLockedTargets().Num(), 3);

    Manager.Tick(0.016f, {});
    TestEqual(TEXT("Locked target is Active"), Manager.GetTierForBody(TEXT("Phobos")), EFidelityTier::Active);
    TestEqual(TEXT("Evicted target is no longer Active"), Manager.GetTierForBody(TEXT("Titan")), EFidelityTier::Dormant);

    Manager.ReleaseObservationLock(TEXT("Phobos"));
    Manager.Tick(0.016f, {});
    TestEqual(TEXT("Released target drops back"), Manager.GetTierForBody(TEXT("Phobos")), EFidelityTier::Dormant);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroReferenceFrameTest, "AstroVerse.Activation.ReferenceFrameForcesParent", AstroActivationTests::Flags)
bool FAstroReferenceFrameTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    if (!AstroActivationTests::Load(*this, Registry))
    {
        return false;
    }
    FActivationManager Manager;
    Manager.Initialize(&Registry);

    // Fill every lock slot; the landing frame must not consume one.
    Manager.LockObservationTarget(TEXT("Moon"));
    Manager.LockObservationTarget(TEXT("Titan"));
    Manager.LockObservationTarget(TEXT("Mars"));
    Manager.SetReferenceFrame(TEXT("Europa"));
    Manager.Tick(0.016f, {});

    TestEqual(TEXT("Landed moon is Active"), Manager.GetTierForBody(TEXT("Europa")), EFidelityTier::Active);
    TestEqual(TEXT("Its parent is Active"), Manager.GetTierForBody(TEXT("Jupiter")), EFidelityTier::Active);
    TestEqual(TEXT("Sibling moon is not"), Manager.GetTierForBody(TEXT("Io")), EFidelityTier::Dormant);
    TestEqual(TEXT("Locks are untouched by landing"), Manager.GetLockedTargets().Num(), 3);
    TestEqual(TEXT("Locks still Active"), Manager.GetTierForBody(TEXT("Moon")), EFidelityTier::Active);

    Manager.SetReferenceFrame(TEXT("Earth"));
    Manager.Tick(0.016f, {});
    TestEqual(TEXT("Landed planet is Active"), Manager.GetTierForBody(TEXT("Earth")), EFidelityTier::Active);
    TestEqual(TEXT("Landing on a planet doesn't promote the Sun"), Manager.GetTierForBody(TEXT("Sun")), EFidelityTier::Dormant);
    TestEqual(TEXT("Old frame's parent drops"), Manager.GetTierForBody(TEXT("Jupiter")), EFidelityTier::Dormant);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroFrustumTest, "AstroVerse.Activation.FrustumPromotesToAmbient", AstroActivationTests::Flags)
bool FAstroFrustumTest::RunTest(const FString& Parameters)
{
    FBodyRegistry Registry;
    if (!AstroActivationTests::Load(*this, Registry))
    {
        return false;
    }
    FActivationManager Manager;
    Manager.Initialize(&Registry);

    TArray<FActivationBodyView> Views;
    Views.SetNum(Registry.Num());
    const int32 Mars = Registry.FindIndex(TEXT("Mars"));
    const int32 Deimos = Registry.FindIndex(TEXT("Deimos"));
    const int32 Venus = Registry.FindIndex(TEXT("Venus"));
    Views[Mars] = FActivationBodyView{ true, 0.01 };
    Views[Deimos] = FActivationBodyView{ true, 1e-7 };  // in view but sub-pixel
    Views[Venus] = FActivationBodyView{ false, 0.02 };  // big but behind the camera
    Manager.Tick(0.016f, Views);

    TestEqual(TEXT("Visible body is Ambient"), Manager.GetTierForIndex(Mars), EFidelityTier::Ambient);
    TestEqual(TEXT("Sub-pixel body stays Dormant"), Manager.GetTierForIndex(Deimos), EFidelityTier::Dormant);
    TestEqual(TEXT("Off-screen body stays Dormant"), Manager.GetTierForIndex(Venus), EFidelityTier::Dormant);

    Manager.LockObservationTarget(TEXT("Mars"));
    Manager.Tick(0.016f, Views);
    TestEqual(TEXT("Lock outranks frustum"), Manager.GetTierForIndex(Mars), EFidelityTier::Active);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstroActivePhobosTest, "AstroVerse.Activation.ActivePhobosStaysBound", AstroActivationTests::Flags)
bool FAstroActivePhobosTest::RunTest(const FString& Parameters)
{
    // An Active fast moon under full N-body with a step that resolves its orbit
    // (~100 steps per 7.65 h orbit) must stay on its real orbit.
    FBodyRegistry Registry;
    if (!AstroActivationTests::Load(*this, Registry))
    {
        return false;
    }
    FSolarSystemSimulation Sim;
    Sim.Initialize(Registry, 0.0, 240.0);
    Sim.SetMaxStepsPerAdvance(MAX_int32);
    const int32 Mars = Registry.FindIndex(TEXT("Mars"));
    const int32 Phobos = Registry.FindIndex(TEXT("Phobos"));
    Sim.SetMoonUsesNBody(Phobos, true);

    double MinKm = 1e30, MaxKm = 0.0;
    for (int32 Minute = 0; Minute <= 10 * 24 * 60; Minute += 7)
    {
        Sim.AdvanceTo(Minute * 60.0);
        const double D = (Sim.GetBodyState(Phobos).Position - Sim.GetBodyState(Mars).Position).Length() / 1000.0;
        MinKm = FMath::Min(MinKm, D);
        MaxKm = FMath::Max(MaxKm, D);
    }
    AddInfo(FString::Printf(TEXT("N-body Phobos distance over 10 days: %.1f - %.1f km (real 9234 - 9518)"), MinKm, MaxKm));
    TestTrue(TEXT("Phobos stays on its orbit"), MinKm > 9150.0 && MaxKm < 9600.0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
