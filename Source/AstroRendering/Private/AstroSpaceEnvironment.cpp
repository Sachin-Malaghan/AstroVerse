// See CLAUDE.md Phase 6.
#include "AstroSpaceEnvironment.h"
#include "AstroRenderingSettings.h"
#include "AstroSimulationSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "AstroBody.h"
#include "BodyShadingComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/AstroConstants.h"
#include "Math/AstroMatrix3d.h"
#include "Star.h"

static TAutoConsoleVariable<int32> CVarAstroMilkyWayGuide(
    TEXT("astro.Sky.MilkyWayGuide"), 0, TEXT("1 = brighter Milky Way band with labels (galactic centre, plane, the Sun's direction of motion)."));

static TAutoConsoleVariable<int32> CVarAstroMilkyWayMode(
    TEXT("astro.Sky.MilkyWay"), 1, TEXT("Sky: 0 = realistic (as photographed), 1 = enhanced Milky Way band (default)."));

int32 AAstroSpaceEnvironment::GetMilkyWayMode()
{
    return CVarAstroMilkyWayMode.GetValueOnGameThread();
}

void AAstroSpaceEnvironment::SetMilkyWayMode(int32 Mode)
{
    CVarAstroMilkyWayMode->Set(Mode, ECVF_SetByCode);
}

bool AAstroSpaceEnvironment::IsMilkyWayGuideOn()
{
    return CVarAstroMilkyWayGuide.GetValueOnGameThread() != 0;
}

static TAutoConsoleVariable<float> CVarAstroExposureCompensation(
    TEXT("astro.Render.ExposureCompensation"), 0.0f,
    TEXT("Extra exposure in stops on top of the sunlight meter (+ brighter)."));

namespace
{
    // IAU 1958 galactic frame in ICRF (J2000): north galactic pole and galactic center.
    FAstroVector3d RaDecToUnit(double RaDeg, double DecDeg)
    {
        const double Ra = RaDeg * AstroConstants::DegToRad, Dec = DecDeg * AstroConstants::DegToRad;
        return FAstroVector3d(FMath::Cos(Dec) * FMath::Cos(Ra), FMath::Cos(Dec) * FMath::Sin(Ra), FMath::Sin(Dec));
    }

    FLinearColor ToParam(const FVector& V)
    {
        return FLinearColor(V.X, V.Y, V.Z, 0.0f);
    }
}

AAstroSpaceEnvironment::AAstroSpaceEnvironment()
{
    // Solar-system presentation: hidden while the galaxy scale-domain is shown.
    Tags.Add(TEXT("AstroSolarSystem"));
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;

    PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
    RootComponent = PostProcess;
    PostProcess->bUnbound = true;
    PostProcess->Priority = 10.0f;

    FPostProcessSettings& S = PostProcess->Settings;
    S.bOverride_AutoExposureMethod = true;
    S.AutoExposureMethod = AEM_Manual;
    S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    S.AutoExposureApplyPhysicalCameraExposure = false;
    S.bOverride_AutoExposureBias = true;
    S.bOverride_MotionBlurAmount = true;
    S.MotionBlurAmount = 0.0f;
    S.bOverride_BloomIntensity = true;
    S.BloomIntensity = 0.5f;
    S.bOverride_VignetteIntensity = true;
    S.VignetteIntensity = 0.2f;
    S.bOverride_LensFlareIntensity = true;
    S.LensFlareIntensity = 0.3f;

    SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    SkyAtmosphere->SetupAttachment(PostProcess);
    SkyAtmosphere->SetUsingAbsoluteLocation(true);
    SkyAtmosphere->TransformMode = ESkyAtmosphereTransformMode::PlanetCenterAtComponentTransform;
    SkyAtmosphere->SetVisibility(false);

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(PostProcess);
    SkyLight->SetMobility(EComponentMobility::Movable);
    SkyLight->bRealTimeCapture = true;
    SkyLight->SetIntensity(1.0f);
    // The stars must not light the ground: capture only the atmosphere (and nothing when there is none).
    SkyLight->bLowerHemisphereIsBlack = true;
    SkyLight->SetVisibility(false); // on only while a SkyAtmosphere is active
}

void AAstroSpaceEnvironment::BeginPlay()
{
    Super::BeginPlay();

    const UAstroRenderingSettings* Settings = GetDefault<UAstroRenderingSettings>();
    if (UMaterialInterface* Stars = Settings->StarFieldMaterial.LoadSynchronous())
    {
        StarFieldMID = UMaterialInstanceDynamic::Create(Stars, this);
        PostProcess->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, StarFieldMID));

        // Galactic axes -> J2000 ecliptic -> engine space.
        const FAstroMatrix3d EqToEcl = FAstroMatrix3d::RotationX(-AstroConstants::ObliquityJ2000Rad);
        const FAstroVector3d North = EqToEcl * RaDecToUnit(192.85948, 27.12825);
        FAstroVector3d Center = EqToEcl * RaDecToUnit(266.40499, -28.93617);
        Center = (Center - North * Center.Dot(North)).Normalized();
        GalacticX = Center;
        GalacticY = North.Cross(Center);
        GalacticZ = North;
    }
}

void AAstroSpaceEnvironment::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    const APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!Sim || !Sim->IsReady() || !PC || !PC->PlayerCameraManager)
    {
        return;
    }
    const UAstroRenderingSettings* Settings = GetDefault<UAstroRenderingSettings>();
    const FBodyRegistry& Registry = Sim->GetRegistry();
    const int32 StarIndex = Registry.GetStarIndex();
    const FAstroVector3d CameraSim = Sim->EngineToSimPosition(PC->PlayerCameraManager->GetCameraLocation());
    const double Distance = FMath::Max((Sim->GetSimulation().GetBodyState(StarIndex).Position - CameraSim).Length(), 1.0);
    SunLuxAtCamera = Registry.Get(StarIndex).LuminosityWatts / (4.0 * AstroConstants::Pi * Distance * Distance) * Settings->LuminousEfficacy;

    // Incident-light meter (calibration constant C = 250): EV100 = log2(E * 100 / C) = log2(E / 2.5).
    const double TargetEV = FMath::Log2(FMath::Max(SunLuxAtCamera, 1e-3) / 2.5)
        - Settings->ExposureCompensation - CVarAstroExposureCompensation.GetValueOnGameThread();
    if (!bExposureInitialized)
    {
        CurrentEV100 = TargetEV;
        bExposureInitialized = true;
    }
    const double Alpha = 1.0 - FMath::Exp(-Settings->ExposureAdaptSpeed * DeltaSeconds);
    CurrentEV100 = FMath::Lerp(CurrentEV100, TargetEV, Alpha);

    // Manual exposure without a physical camera scales the scene by 2^bias; the photographic
    // exposure for EV100 is 1 / (1.2 * 2^EV100).
    PostProcess->Settings.AutoExposureBias = static_cast<float>(-(CurrentEV100 + FMath::Log2(1.2)));

    // Lumen / sky-capture caches store lighting pre-exposed in FP16 (usable EV = value-12 ..
    // value+8). Exposure here spans ~EV 5 (Neptune) to ~27 (skimming the Sun), so keep the
    // cache centred on the metered exposure; move it in whole steps (a change resets caches).
    if (IConsoleVariable* PreExposure = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.CachedLightingPreExposure")))
    {
        const double Wanted = FMath::RoundToDouble(CurrentEV100 - 2.0);
        if (FMath::Abs(PreExposure->GetFloat() - Wanted) >= 3.0)
        {
            PreExposure->Set(static_cast<float>(Wanted), ECVF_SetByCode);
        }
    }

    double Daylight = 0.0;
    UpdateSkyAtmosphere(Sim, CameraSim, Daylight);

    if (StarFieldMID)
    {
        // The sky turns with the frame when landed on a rotating body.
        StarFieldMID->SetVectorParameterValue(TEXT("GalX"), ToParam(Sim->SimToEngineDirection(GalacticX)));
        StarFieldMID->SetVectorParameterValue(TEXT("GalY"), ToParam(Sim->SimToEngineDirection(GalacticY)));
        StarFieldMID->SetVectorParameterValue(TEXT("GalZ"), ToParam(Sim->SimToEngineDirection(GalacticZ)));

        // Display-referred brightness converted back to scene luminance at this exposure.
        // ...and a sunlit sky overhead washes them out.
        const bool bEnhanced = GetMilkyWayMode() != 0;
        const double Guide = IsMilkyWayGuideOn() ? 1.3 : 1.0; // teaching mode: brighter still
        const double Display = bEnhanced ? Settings->EnhancedStarDisplayBrightness : Settings->StarDisplayBrightness;
        const double StarNits = Guide * Display * 1.2 * FMath::Pow(2.0, CurrentEV100) * (1.0 - 0.995 * Daylight);
        StarFieldMID->SetScalarParameterValue(TEXT("BandStrength"), static_cast<float>(bEnhanced ? Settings->EnhancedBandStrength : 0.0));
        StarFieldMID->SetScalarParameterValue(TEXT("StarGain"), static_cast<float>(bEnhanced ? Settings->EnhancedStarGain : 1.0));
        StarFieldMID->SetScalarParameterValue(TEXT("StarNits"), static_cast<float>(StarNits));
    }

    // The star's directional light is only used for lit meshes near the camera (landed
    // terrain, craft); give it the physically right illuminance there.
    if (const AStar* Star = Cast<AStar>(Sim->GetBodyActor(StarIndex)))
    {
        Star->SunLight->SetIntensity(static_cast<float>(SunLuxAtCamera));
    }
}

void AAstroSpaceEnvironment::UpdateSkyAtmosphere(const UAstroSimulationSubsystem* Sim, const FAstroVector3d& CameraSim, double& OutDaylight)
{
    // Pick the body whose atmosphere the camera is nearest to, within 10 atmosphere heights.
    const FBodyRegistry& Registry = Sim->GetRegistry();
    int32 Best = INDEX_NONE;
    double BestAltitudeFraction = TNumericLimits<double>::Max();
    const UBodyShadingComponent* BestShading = nullptr;
    for (int32 i = 0; i < Registry.Num(); ++i)
    {
        const AAstroBody* Body = Sim->GetBodyActor(i);
        const UBodyShadingComponent* Shading = Body ? Body->FindComponentByClass<UBodyShadingComponent>() : nullptr;
        if (!Shading || !Shading->HasAtmosphere())
        {
            continue;
        }
        const double Ground = Registry.Get(i).EquatorialRadiusMeters;
        const double Height = Shading->GetAtmosphereTopMeters() - Ground;
        const double Altitude = (CameraSim - Sim->GetSimulation().GetBodyState(i).Position).Length() - Ground;
        const double Fraction = Altitude / Height;
        if (Fraction < 10.0 && Fraction < BestAltitudeFraction)
        {
            Best = i;
            BestAltitudeFraction = Fraction;
            BestShading = Shading;
        }
    }

    // Hand shells over to / back from the engine sky.
    for (int32 i = 0; i < Registry.Num(); ++i)
    {
        if (const AAstroBody* Body = Sim->GetBodyActor(i))
        {
            if (UBodyShadingComponent* Shading = Body->FindComponentByClass<UBodyShadingComponent>())
            {
                Shading->SetShellVisible(i != Best);
            }
        }
    }

    OutDaylight = 0.0;
    if (Best == INDEX_NONE)
    {
        if (SkyBodyIndex != INDEX_NONE)
        {
            SkyAtmosphere->SetVisibility(false);
            SkyLight->SetVisibility(false); // real-time capture of an empty sky is black anyway
            SkyBodyIndex = INDEX_NONE;
        }
        return;
    }
    const FBodyDefinition& Def = Registry.Get(Best);
    const FAstroVector3d BodyPos = Sim->GetSimulation().GetBodyState(Best).Position;
    const FAstroVector3d Up = (CameraSim - BodyPos).Normalized();

    // SkyAtmosphere is a sphere but bodies are oblate: use the ellipsoid radius directly below
    // the camera, less a margin so the sky's own horizon always sits below the mesh horizon.
    const FAstroVector3d UpBodyFixed = Def.GetOrientationAt(Sim->GetSimulation().GetSimSeconds()).Transposed() * Up;
    const double A2 = Def.EquatorialRadiusMeters * Def.EquatorialRadiusMeters;
    const double C2 = Def.PolarRadiusMeters * Def.PolarRadiusMeters;
    const double LocalRadius = 1.0 / FMath::Sqrt((UpBodyFixed.X * UpBodyFixed.X + UpBodyFixed.Y * UpBodyFixed.Y) / A2 + UpBodyFixed.Z * UpBodyFixed.Z / C2);
    const double SkyGround = LocalRadius - 1000.0;

    if (SkyBodyIndex != Best)
    {
        ConfigureSkyFor(BestShading, SkyGround);
        SkyAtmosphere->SetVisibility(true);
        SkyLight->SetVisibility(true);
        SkyBodyIndex = Best;
        SkyGroundMeters = SkyGround;
    }
    else if (FMath::Abs(SkyGround - SkyGroundMeters) > 100.0)
    {
        SkyAtmosphere->SetBottomRadius(static_cast<float>(SkyGround / 1000.0));
        SkyGroundMeters = SkyGround;
    }
    SkyAtmosphere->SetWorldLocation(Sim->SimToEnginePosition(BodyPos));

    // Daylight at the camera: sun above the local horizon, fading with altitude.
    const FAstroVector3d ToSun = (Sim->GetSimulation().GetBodyState(Registry.GetStarIndex()).Position - CameraSim).Normalized();
    const double SunElevation = Up.Dot(ToSun);
    const double Thickness = FMath::Clamp(1.0 - BestAltitudeFraction, 0.0, 1.0);
    OutDaylight = FMath::SmoothStep(-0.12, 0.05, SunElevation) * Thickness;
}

void AAstroSpaceEnvironment::ConfigureSkyFor(const UBodyShadingComponent* Shading, double GroundRadiusMeters)
{
    // DT_Appearance holds per-megameter coefficients; SkyAtmosphere wants per-kilometer, as color * scale.
    const FAstroBodyAppearanceRow& A = Shading->GetAppearance();
    const FVector RayleighPerKm = A.RayleighPerMm * 1e-3;
    const double RayleighScale = FMath::Max(RayleighPerKm.GetMax(), 1e-9);
    SkyAtmosphere->SetBottomRadius(static_cast<float>(GroundRadiusMeters / 1000.0));
    SkyAtmosphere->SetAtmosphereHeight(static_cast<float>(A.AtmosphereHeightKm));
    SkyAtmosphere->SetRayleighScattering(FLinearColor(RayleighPerKm.X / RayleighScale, RayleighPerKm.Y / RayleighScale, RayleighPerKm.Z / RayleighScale));
    SkyAtmosphere->SetRayleighScatteringScale(static_cast<float>(RayleighScale));
    SkyAtmosphere->SetRayleighExponentialDistribution(static_cast<float>(A.RayleighScaleHeightKm));
    SkyAtmosphere->SetMieScattering(A.MieTint);
    SkyAtmosphere->SetMieScatteringScale(static_cast<float>(A.MiePerMm * 1e-3));
    SkyAtmosphere->SetMieAbsorptionScale(static_cast<float>(A.MiePerMm * 1e-3 * 0.11));
    SkyAtmosphere->SetMieAnisotropy(static_cast<float>(A.MieAnisotropy));
    SkyAtmosphere->SetMieExponentialDistribution(static_cast<float>(A.MieScaleHeightKm));
    SkyAtmosphere->SetMultiScatteringFactor(1.0f);
}
