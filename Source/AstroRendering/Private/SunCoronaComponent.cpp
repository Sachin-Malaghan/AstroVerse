// See CLAUDE.md Phase 6.
#include "SunCoronaComponent.h"
#include "AstroRenderingSettings.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"

USunCoronaComponent::USunCoronaComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
    SetUsingAbsoluteRotation(true);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetCastShadow(false);
    bNeverDistanceCull = true;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (Plane.Succeeded())
    {
        SetStaticMesh(Plane.Object);
    }
}

void USunCoronaComponent::OnRegister()
{
    Super::OnRegister();
    SetRelativeScale3D(FVector(CoronaExtent));
    if (!CoronaMID)
    {
        if (UMaterialInterface* Material = GetDefault<UAstroRenderingSettings>()->CoronaMaterial.LoadSynchronous())
        {
            CoronaMID = UMaterialInstanceDynamic::Create(Material, this);
            SetMaterial(0, CoronaMID);
            CoronaMID->SetScalarParameterValue(TEXT("CoronaExtent"), CoronaExtent);
        }
    }
}

void USunCoronaComponent::SetCoronaLuminance(double Luminance)
{
    if (CoronaMID)
    {
        CoronaMID->SetScalarParameterValue(TEXT("Luminance"), static_cast<float>(Luminance));
    }
}

void USunCoronaComponent::SetHaloLuminance(double Luminance, double Warmth)
{
    if (CoronaMID)
    {
        CoronaMID->SetScalarParameterValue(TEXT("HaloLuminance"), static_cast<float>(Luminance));
        CoronaMID->SetScalarParameterValue(TEXT("HaloWarmth"), static_cast<float>(Warmth));
    }
}

void USunCoronaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC && PC->PlayerCameraManager)
    {
        // The plane's +Z faces the camera.
        const FVector ToCamera = PC->PlayerCameraManager->GetCameraLocation() - GetComponentLocation();
        if (!ToCamera.IsNearlyZero())
        {
            SetWorldRotation(FRotationMatrix::MakeFromZ(ToCamera).Rotator());
        }
        // Far away the disk falls below ~2 px: carry its flux in a ~1 px Gaussian instead
        // (flux-conserving; widened rather than clipped where it would overflow FP16).
        FVector2D ViewSize(1600.0, 900.0);
        if (GEngine && GEngine->GameViewport)
        {
            GEngine->GameViewport->GetViewportSize(ViewSize);
        }
        const double RadiusEngine = GetComponentScale().X * 50.0 / FMath::Max(AppliedExtent > 0.0f ? AppliedExtent : CoronaExtent, 1e-3f);
        const double PixelsPerRadian = 0.5 * ViewSize.X / FMath::Tan(FMath::DegreesToRadians(0.5 * PC->PlayerCameraManager->GetFOVAngle()));
        const double DiskPx = RadiusEngine / ToCamera.Size() * PixelsPerRadian;
        float Extent = CoronaExtent, Sigma = 0.0f, Peak = 0.0f;
        const double Weight = FMath::Clamp((2.0 - DiskPx) / 1.5, 0.0, 1.0);
        if (Weight > 0.0 && DiskPx > 0.0 && DiskLuminance > 0.0)
        {
            double S = 1.2 / DiskPx; // solar radii per ~1.2 px
            const double MaxPeak = 6.0e4; // below FP16 max (65504)
            // Flux of the disk (L * pi R^2) = peak * 2 pi sigma^2.
            double P = Weight * DiskLuminance / (2.0 * S * S);
            if (P > MaxPeak)
            {
                // Too bright to hold in ~1 px: widen, but never past ~3 px - beyond that a
                // point source reads as a ball; bloom supplies the rest of the glare.
                S = FMath::Min(FMath::Sqrt(Weight * DiskLuminance / (2.0 * MaxPeak)), 3.0 / DiskPx);
                P = MaxPeak;
            }
            Sigma = static_cast<float>(S);
            Peak = static_cast<float>(P);
            Extent = FMath::Max(CoronaExtent, static_cast<float>(5.0 * S));
        }
        if (CoronaMID)
        {
            if (!FMath::IsNearlyEqual(Extent, AppliedExtent))
            {
                // Keep the quad's world size in solar radii: rescale relative to the parent sphere.
                SetRelativeScale3D(FVector(Extent));
                CoronaMID->SetScalarParameterValue(TEXT("CoronaExtent"), Extent);
                AppliedExtent = Extent;
            }
            CoronaMID->SetScalarParameterValue(TEXT("PointSigma"), Sigma);
            CoronaMID->SetScalarParameterValue(TEXT("PointLuminance"), Peak);
        }
    }
}
