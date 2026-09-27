// See CLAUDE.md Phase 9.
#include "AstroWarpEffects.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    const TCHAR* WarpPostPath = TEXT("/Game/Rendering/Materials/M_WarpPost.M_WarpPost");
    const TCHAR* WarpTunnelPath = TEXT("/Game/Rendering/Materials/M_WarpTunnel.M_WarpTunnel");
    const TCHAR* ShipHullPath = TEXT("/Game/Rendering/Materials/M_ShipHull.M_ShipHull");
}

AAstroWarpEffects::AAstroWarpEffects()
{
    // Solar-system presentation: hidden while the galaxy scale-domain is shown.
    Tags.Add(TEXT("AstroSolarSystem"));
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("WarpPost"));
    PostProcess->SetupAttachment(RootComponent);
    PostProcess->bUnbound = true;
    PostProcess->Priority = 20.0f;
    PostProcess->bEnabled = false;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Tunnel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tunnel"));
    Tunnel->SetupAttachment(RootComponent);
    Tunnel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tunnel->SetCastShadow(false);
    Tunnel->bNeverDistanceCull = true;
    if (Cylinder.Succeeded())
    {
        Tunnel->SetStaticMesh(Cylinder.Object);
    }
    // Lie along the view (cylinder axis is local Z), long and wide around the camera.
    Tunnel->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
    Tunnel->SetRelativeScale3D(FVector(40.0, 40.0, 600.0));
    Tunnel->SetVisibility(false);

    // Placeholder cockpit: dash below the view, a canopy frame, two side struts (cm, viewer-relative).
    AddPart(TEXT("Dash"), FVector(80.0, 0.0, -45.0), FVector(0.6, 1.6, 0.25), FRotator(-12.0, 0.0, 0.0));
    AddPart(TEXT("CanopyTop"), FVector(70.0, 0.0, 55.0), FVector(0.1, 1.8, 0.06));
    AddPart(TEXT("StrutLeft"), FVector(75.0, -85.0, 0.0), FVector(0.08, 0.08, 1.1), FRotator(0.0, 0.0, -18.0));
    AddPart(TEXT("StrutRight"), FVector(75.0, 85.0, 0.0), FVector(0.08, 0.08, 1.1), FRotator(0.0, 0.0, 18.0));
    AddPart(TEXT("Nose"), FVector(160.0, 0.0, -60.0), FVector(1.2, 0.8, 0.12), FRotator(-6.0, 0.0, 0.0));
}

UStaticMeshComponent* AAstroWarpEffects::AddPart(const TCHAR* Name, const FVector& Location, const FVector& Scale, const FRotator& Rotation)
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Part->SetupAttachment(RootComponent);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetRelativeLocationAndRotation(Location, Rotation);
    Part->SetRelativeScale3D(Scale);
    Part->SetVisibility(false);
    if (Cube.Succeeded())
    {
        Part->SetStaticMesh(Cube.Object);
    }
    CockpitParts.Add(Part);
    return Part;
}

void AAstroWarpEffects::Begin(AActor* Viewer, bool bInPiloted)
{
    ViewerActor = Viewer;
    bPiloted = bInPiloted;
    AttachToActor(Viewer, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

    if (!PostMID)
    {
        if (UMaterialInterface* Post = LoadObject<UMaterialInterface>(nullptr, WarpPostPath))
        {
            PostMID = UMaterialInstanceDynamic::Create(Post, this);
            PostProcess->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, PostMID));
        }
    }
    if (!TunnelMID)
    {
        if (UMaterialInterface* TunnelMaterial = LoadObject<UMaterialInterface>(nullptr, WarpTunnelPath))
        {
            TunnelMID = UMaterialInstanceDynamic::Create(TunnelMaterial, this);
            Tunnel->SetMaterial(0, TunnelMID);
        }
        if (UMaterialInterface* Hull = LoadObject<UMaterialInterface>(nullptr, ShipHullPath))
        {
            for (UStaticMeshComponent* Part : CockpitParts)
            {
                Part->SetMaterial(0, Hull);
            }
        }
    }
    PostProcess->bEnabled = true;
    Tunnel->SetVisibility(true);
    for (UStaticMeshComponent* Part : CockpitParts)
    {
        Part->SetVisibility(bPiloted);
    }
    Update(0.0f, 0.0f, FVector2D::ZeroVector);
}

void AAstroWarpEffects::Update(float Intensity, float DistanceMeters, const FVector2D& Steer)
{
    if (PostMID)
    {
        PostMID->SetScalarParameterValue(TEXT("Intensity"), Intensity);
    }
    if (TunnelMID)
    {
        TunnelMID->SetScalarParameterValue(TEXT("Intensity"), Intensity);
    }
    // The cockpit leans into the turn a little more than the view does.
    SetActorRelativeRotation(FRotator(0.0, 0.0, -Steer.X * 8.0));
}

void AAstroWarpEffects::End()
{
    PostProcess->bEnabled = false;
    Tunnel->SetVisibility(false);
    for (UStaticMeshComponent* Part : CockpitParts)
    {
        Part->SetVisibility(false);
    }
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
}
