// See CLAUDE.md "Sun at a site".
#include "AstroSiteActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    // Actor-local axes: X = east, Z = up, so Y = south (the engine is left-handed).
    FVector LocalFromENU(const FVector& ENU) { return FVector(ENU.X, -ENU.Y, ENU.Z); }

    const FLinearColor PathColors[] = {
        FLinearColor(1.0f, 0.85f, 0.25f),  // today
        FLinearColor(1.0f, 0.45f, 0.15f),  // June solstice
        FLinearColor(0.3f, 0.7f, 1.0f),    // December solstice
        FLinearColor(0.85f, 0.85f, 0.85f), // equinox
    };
}

AAstroSiteActor::AAstroSiteActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
    SkyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SkyRoot"));
    SkyRoot->SetupAttachment(Root);
    SkyRoot->SetUsingAbsoluteLocation(true); // follows the viewer, keeps the site's axes
    Tags.Add(TEXT("AstroSolarSystem")); // hidden with the solar system in the galaxy view

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

    for (int32 i = 0; i < 4; ++i)
    {
        UInstancedStaticMeshComponent* Dots = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("PathDots%d"), i));
        Dots->SetupAttachment(SkyRoot);
        Dots->SetStaticMesh(Sphere.Object);
        Dots->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Dots->SetCastShadow(false);
        PathDots.Add(Dots);
    }
    Compass = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Compass"));
    Compass->SetupAttachment(Root);
    Compass->SetStaticMesh(Cube.Object);
    Compass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Compass->SetCastShadow(false);

    SunMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SunMarker"));
    SunMarker->SetupAttachment(SkyRoot);
    SunMarker->SetStaticMesh(Sphere.Object);
    SunMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SunMarker->SetCastShadow(false);

    // The gnomon: a real 1 m stick, lit and shadow-casting (its shadow is the engine's).
    Gnomon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gnomon"));
    Gnomon->SetupAttachment(Root);
    Gnomon->SetStaticMesh(Cylinder.Object);
    Gnomon->SetRelativeScale3D(FVector(0.04, 0.04, 1.0)); // basic cylinder: 100 cm tall, 100 cm wide
    Gnomon->SetRelativeLocation(FVector(0, 0, 50));
    Gnomon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Gnomon->SetCastShadow(true);
}

UMaterialInstanceDynamic* AAstroSiteActor::MakeGlow(const FLinearColor& Color)
{
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Rendering/Materials/M_GalaxyMarker.M_GalaxyMarker"));
    if (!Base)
    {
        return nullptr;
    }
    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
    Glows.Add(MID);
    GlowColors.Add(Color);
    return MID;
}

void AAstroSiteActor::SetFrame(const FVector& InGround, const FVector& InEast, const FVector& InNorth, const FVector& InUp)
{
    Ground = InGround;
    East = InEast;
    North = InNorth;
    Up = InUp;
    SetActorLocationAndRotation(Ground, FRotationMatrix::MakeFromXZ(East, Up).ToQuat());
    if (Glows.Num() == 0)
    {
        for (int32 i = 0; i < 4; ++i)
        {
            PathDots[i]->SetMaterial(0, MakeGlow(PathColors[i]));
        }
        Compass->SetMaterial(0, MakeGlow(FLinearColor(0.8f, 0.85f, 0.95f)));
        SunMarker->SetMaterial(0, MakeGlow(FLinearColor(1.0f, 0.95f, 0.7f)));
        RebuildInstances();
    }
}

void AAstroSiteActor::SetViewer(const FVector& ViewerLocation)
{
    Viewer = ViewerLocation;
    SkyRoot->SetWorldLocation(ViewerLocation);
}

void AAstroSiteActor::SetPaths(const TArray<TArray<FVector>>& PathsENU)
{
    Paths = PathsENU;
    RebuildInstances();
}

void AAstroSiteActor::SetSun(const FVector& InSunENU, bool bAboveHorizon)
{
    SunENU = InSunENU;
    bSunUp = bAboveHorizon;
    SunMarker->SetVisibility(bAboveHorizon);
    SunMarker->SetRelativeLocation(LocalFromENU(SunENU) * DomeRadiusCm);
    SunMarker->SetRelativeScale3D(FVector(0.9));
}

void AAstroSiteActor::SetPathsVisible(bool bVisible)
{
    for (UInstancedStaticMeshComponent* Dots : PathDots)
    {
        Dots->SetVisibility(bVisible);
    }
    SunMarker->SetVisibility(bVisible && bSunUp);
}

void AAstroSiteActor::SetExposureEV100(double EV100)
{
    // Bright but not clipped: about half the luminance that saturates at this exposure
    // (saturation ~ 1.2 * 2^EV100 cd/m^2 for the engine's manual exposure).
    const float Nits = static_cast<float>(0.5 * FMath::Pow(2.0, EV100));
    for (int32 i = 0; i < Glows.Num(); ++i)
    {
        if (Glows[i])
        {
            Glows[i]->SetVectorParameterValue(TEXT("Glow"), GlowColors[i] * Nits);
        }
    }
}

void AAstroSiteActor::RebuildInstances()
{
    for (int32 i = 0; i < PathDots.Num(); ++i)
    {
        PathDots[i]->ClearInstances();
        if (!Paths.IsValidIndex(i))
        {
            continue;
        }
        const double Size = i == 0 ? 0.22 : 0.16; // today's path a little bolder
        for (const FVector& Dir : Paths[i])
        {
            PathDots[i]->AddInstance(FTransform(FQuat::Identity, LocalFromENU(Dir) * DomeRadiusCm, FVector(Size)));
        }
    }
    // Compass: eight spokes on the ground, north longest; a ring of ticks every 15 deg.
    Compass->ClearInstances();
    for (int32 k = 0; k < 8; ++k)
    {
        const double Az = FMath::DegreesToRadians(k * 45.0);
        const FVector Dir(FMath::Sin(Az), FMath::Cos(Az), 0.0); // ENU: az from north, clockwise
        const double Length = k == 0 ? 1200.0 : (k % 2 == 0 ? 900.0 : 600.0);
        const FQuat Rot = FRotationMatrix::MakeFromX(LocalFromENU(Dir)).ToQuat();
        Compass->AddInstance(FTransform(Rot, LocalFromENU(Dir) * (Length * 0.5 + 30.0) + FVector(0, 0, 2), FVector(Length / 100.0, 0.05, 0.02)));
    }
    for (int32 k = 0; k < 24; ++k)
    {
        const double Az = FMath::DegreesToRadians(k * 15.0);
        const FVector Dir(FMath::Sin(Az), FMath::Cos(Az), 0.0);
        const FQuat Rot = FRotationMatrix::MakeFromX(LocalFromENU(Dir)).ToQuat();
        Compass->AddInstance(FTransform(Rot, LocalFromENU(Dir) * 1250.0 + FVector(0, 0, 2), FVector(0.6, 0.04, 0.02)));
    }
}
