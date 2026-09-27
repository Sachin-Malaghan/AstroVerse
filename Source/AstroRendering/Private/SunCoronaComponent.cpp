// See CLAUDE.md Phase 6.
#include "SunCoronaComponent.h"
#include "AstroRenderingSettings.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

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
    }
}
