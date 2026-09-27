// See CLAUDE.md Phase 10.
#include "AstroGalaxyActor.h"
#include "HAL/IConsoleManager.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

// Raymarch samples for the galaxy volume; the render budget lists set it (VR runs fewer).
static TAutoConsoleVariable<int32> CVarAstroGalaxyRaySteps(
    TEXT("astro.Galaxy.RaySteps"), 96, TEXT("Galaxy volume raymarch samples per pixel (8-256)."));

// Resolution of the (after-DOF) translucency pass while the galaxy domain is shown; the
// volume is smooth, so VR renders it at half resolution. Restored on leaving the domain.
static TAutoConsoleVariable<float> CVarAstroGalaxyTranslucencyPercentage(
    TEXT("astro.Galaxy.TranslucencyScreenPercentage"), 100.0f, TEXT("r.SeparateTranslucencyScreenPercentage applied in the galaxy domain."));

namespace
{
    const FVector HalfSizeKpc(25.0, 25.0, 3.0);
    // Sun's velocity about the galactic center (km/s): 233 km/s rotation plus the solar
    // peculiar motion (U, V, W) = (11.1, 12.24, 7.25) (Schonrich et al. 2010).
    const FVector SunVelocityKmS(11.1, 233.0 + 12.24, 7.25);
}

AAstroGalaxyActor::AAstroGalaxyActor()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("GalacticCenter"));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));

    // Volume box: cube half-size 50 cm scaled to the galaxy's bounds.
    Volume = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Volume"));
    Volume->SetupAttachment(RootComponent);
    Volume->SetStaticMesh(Cube.Object);
    Volume->SetRelativeScale3D(HalfSizeKpc * CmPerKpc / 50.0);
    Volume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Volume->SetCastShadow(false);
    Volume->bNeverDistanceCull = true;

    // Sun marker: a small bright sphere (a few light-years across so it is always findable).
    SunMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SunMarker"));
    SunMarker->SetupAttachment(RootComponent);
    SunMarker->SetStaticMesh(Sphere.Object);
    SunMarker->SetRelativeLocation(KpcToEngine(SunKpc()));
    SunMarker->SetRelativeScale3D(FVector(60.0 * CmPerLightYear / 50.0));
    SunMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Velocity arrow along the Sun's galactic motion.
    VelocityArrow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VelocityArrow"));
    VelocityArrow->SetupAttachment(SunMarker);
    VelocityArrow->SetStaticMesh(Cone.Object);
    VelocityArrow->SetUsingAbsoluteScale(true);
    VelocityArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    const FVector VelEngine = FVector(SunVelocityKmS.X, -SunVelocityKmS.Y, SunVelocityKmS.Z).GetSafeNormal();
    VelocityArrow->SetRelativeRotation(FRotationMatrix::MakeFromZ(VelEngine).Rotator());
    VelocityArrow->SetWorldScale3D(FVector(1.2, 1.2, 4.0) * (100.0 * CmPerLightYear / 100.0));

    SunLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SunLabel"));
    SunLabel->SetupAttachment(SunMarker);
    SunLabel->SetUsingAbsoluteScale(true);
    SunLabel->SetText(FText::FromString(TEXT("Sun  -  you are here\n233 km/s around the galactic center")));
    SunLabel->SetHorizontalAlignment(EHTA_Center);
    SunLabel->SetWorldSize(40.0f * static_cast<float>(CmPerLightYear));
    SunLabel->SetTextRenderColor(FColor(255, 220, 160));

    // Fixed exposure: the galaxy material is display-referred.
    PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("GalaxyPost"));
    PostProcess->SetupAttachment(RootComponent);
    PostProcess->bUnbound = true;
    PostProcess->Priority = 30.0f;
    FPostProcessSettings& S = PostProcess->Settings;
    S.bOverride_AutoExposureMethod = true;
    S.AutoExposureMethod = AEM_Manual;
    S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    S.AutoExposureApplyPhysicalCameraExposure = false;
    S.bOverride_AutoExposureBias = true;
    S.AutoExposureBias = 0.0f;
    S.bOverride_BloomIntensity = true;
    S.BloomIntensity = 0.9f;

    SetDomainActive(false);
}

void AAstroGalaxyActor::SetDomainActive(bool bActive)
{
    if (IConsoleVariable* Translucency = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SeparateTranslucencyScreenPercentage")))
    {
        if (bActive && !bTranslucencyOverridden)
        {
            SavedTranslucencyPercentage = Translucency->GetFloat();
            Translucency->Set(CVarAstroGalaxyTranslucencyPercentage.GetValueOnGameThread(), ECVF_SetByCode);
            bTranslucencyOverridden = true;
        }
        else if (!bActive && bTranslucencyOverridden)
        {
            Translucency->Set(SavedTranslucencyPercentage, ECVF_SetByCode);
            bTranslucencyOverridden = false;
        }
    }
    SetActorHiddenInGame(!bActive);
    PostProcess->bEnabled = bActive;
    SetActorTickEnabled(bActive);
    if (bActive && !VolumeMID)
    {
        if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Rendering/Materials/M_GalaxyVolume.M_GalaxyVolume")))
        {
            VolumeMID = UMaterialInstanceDynamic::Create(Material, this);
            Volume->SetMaterial(0, VolumeMID);
            // Mesh-local (+-50) -> kpc, with the engine's mirrored Y undone.
            VolumeMID->SetVectorParameterValue(TEXT("LocalToKpc"), FLinearColor(HalfSizeKpc.X / 50.0, -HalfSizeKpc.Y / 50.0, HalfSizeKpc.Z / 50.0, 0.0));
            VolumeMID->SetVectorParameterValue(TEXT("HalfSize"), FLinearColor(HalfSizeKpc.X, HalfSizeKpc.Y, HalfSizeKpc.Z, 0.0));
        }
        if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Rendering/Materials/M_GalaxyMarker.M_GalaxyMarker")))
        {
            SunMarker->SetMaterial(0, Glow);
            VelocityArrow->SetMaterial(0, Glow);
        }
    }
}

void AAstroGalaxyActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->PlayerCameraManager)
    {
        return;
    }
    const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
    // Is the camera inside the volume box? (selects which faces the raymarch uses)
    const FVector Local = (Camera - GetActorLocation()) / CmPerKpc;
    const bool bInside = FMath::Abs(Local.X) < HalfSizeKpc.X && FMath::Abs(Local.Y) < HalfSizeKpc.Y && FMath::Abs(Local.Z) < HalfSizeKpc.Z;
    if (VolumeMID)
    {
        VolumeMID->SetScalarParameterValue(TEXT("CameraInside"), bInside ? 1.0f : 0.0f);
        VolumeMID->SetScalarParameterValue(TEXT("Steps"), static_cast<float>(CVarAstroGalaxyRaySteps.GetValueOnGameThread()));
    }
    // Keep the marker readable at any zoom: grow with distance, and face the label to the camera.
    const double Distance = FVector::Dist(Camera, SunMarker->GetComponentLocation());
    const double Scale = FMath::Max(60.0 * CmPerLightYear, Distance * 0.004) / 50.0;
    SunMarker->SetWorldScale3D(FVector(Scale));
    VelocityArrow->SetWorldScale3D(FVector(0.3, 0.3, 1.0) * Scale * 2.0);
    SunLabel->SetWorldSize(static_cast<float>(Distance * 0.02));
    SunLabel->SetRelativeLocation(FVector(0.0, 0.0, 90.0));
    SunLabel->SetWorldRotation((Camera - SunLabel->GetComponentLocation()).Rotation());
}
