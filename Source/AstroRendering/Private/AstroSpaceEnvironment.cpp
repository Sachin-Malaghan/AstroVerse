// See CLAUDE.md Phase 6.
#include "AstroSpaceEnvironment.h"
#include "AstroRenderingSettings.h"
#include "AstroSimulationSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/AstroConstants.h"
#include "Math/AstroMatrix3d.h"
#include "Star.h"

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
        const FAstroVector3d Y = North.Cross(Center);
        StarFieldMID->SetVectorParameterValue(TEXT("GalX"), ToParam(UAstroSimulationSubsystem::SimToEngineDirection(Center)));
        StarFieldMID->SetVectorParameterValue(TEXT("GalY"), ToParam(UAstroSimulationSubsystem::SimToEngineDirection(Y)));
        StarFieldMID->SetVectorParameterValue(TEXT("GalZ"), ToParam(UAstroSimulationSubsystem::SimToEngineDirection(North)));
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

    if (StarFieldMID)
    {
        // Display-referred brightness converted back to scene luminance at this exposure.
        const double StarNits = Settings->StarDisplayBrightness * 1.2 * FMath::Pow(2.0, CurrentEV100);
        StarFieldMID->SetScalarParameterValue(TEXT("StarNits"), static_cast<float>(StarNits));
    }

    // The star's directional light is only used for lit meshes near the camera (landed
    // terrain, craft); give it the physically right illuminance there.
    if (const AStar* Star = Cast<AStar>(Sim->GetBodyActor(StarIndex)))
    {
        Star->SunLight->SetIntensity(static_cast<float>(SunLuxAtCamera));
    }
}
