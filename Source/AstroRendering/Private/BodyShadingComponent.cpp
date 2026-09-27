// See CLAUDE.md Phase 6.
#include "BodyShadingComponent.h"
#include "AstroBody.h"
#include "AstroRenderingSettings.h"
#include "AstroSimulationSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/AstroConstants.h"
#include "SunCoronaComponent.h"

namespace
{
    UStaticMesh* LoadBodySphere()
    {
        UStaticMesh* Mesh = GetDefault<UAstroRenderingSettings>()->BodySphereMesh.LoadSynchronous();
        return Mesh ? Mesh : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    }

    FLinearColor ToParam(const FVector& V)
    {
        return FLinearColor(V.X, V.Y, V.Z, 0.0f);
    }
}

UBodyShadingComponent::UBodyShadingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UBodyShadingComponent::Configure(const FAstroBodyAppearanceRow* InAppearance)
{
    bHasAppearance = InAppearance != nullptr;
    if (InAppearance)
    {
        Appearance = *InAppearance;
    }
}

void UBodyShadingComponent::OnRegister()
{
    Super::OnRegister();
    if (AActor* Owner = GetOwner())
    {
        // Shade after the body has moved this frame.
        AddTickPrerequisiteActor(Owner);
    }
    if (!SurfaceMID)
    {
        BuildComponents();
    }
}

void UBodyShadingComponent::BuildComponents()
{
    AAstroBody* Body = Cast<AAstroBody>(GetOwner());
    const FBodyDefinition* Definition = Body ? Body->GetDefinition() : nullptr;
    if (!Definition)
    {
        return;
    }
    const UAstroRenderingSettings* Settings = GetDefault<UAstroRenderingSettings>();
    UStaticMesh* Sphere = LoadBodySphere();
    Body->BodyMesh->SetStaticMesh(Sphere);

    UMaterialInterface* Surface = bHasAppearance ? Appearance.SurfaceMaterial.LoadSynchronous() : nullptr;
    if (!Surface)
    {
        Surface = Settings->DefaultSurfaceMaterial.LoadSynchronous();
    }
    if (Surface)
    {
        SurfaceMID = UMaterialInstanceDynamic::Create(Surface, this);
        Body->BodyMesh->SetMaterial(0, SurfaceMID);
    }

    const double Req = Definition->EquatorialRadiusMeters;
    if (SurfaceMID && Definition->RingOuterRadiusMeters > 0.0)
    {
        SurfaceMID->SetScalarParameterValue(TEXT("RingInner"), Definition->RingInnerRadiusMeters / Req);
        SurfaceMID->SetScalarParameterValue(TEXT("RingOuter"), Definition->RingOuterRadiusMeters / Req);
    }

    // Atmosphere: a uniformly scaled shell around the (possibly oblate) body mesh.
    if (bHasAppearance && Appearance.AtmosphereHeightKm > 0.0)
    {
        if (UMaterialInterface* AtmoMaterial = Settings->AtmosphereMaterial.LoadSynchronous())
        {
            AtmosphereTopMeters = Req + Appearance.AtmosphereHeightKm * 1000.0;
            const double Top = AtmosphereTopMeters;

            AtmosphereShell = NewObject<UStaticMeshComponent>(Body, TEXT("AtmosphereShell"));
            AtmosphereShell->SetStaticMesh(Sphere);
            AtmosphereShell->SetupAttachment(Body->BodyMesh);
            // The parent is scaled (Req, Req, Rpolar); undo the polar squash so the shell is round.
            AtmosphereShell->SetRelativeScale3D(FVector(Top / Req, Top / Req, Top / Definition->PolarRadiusMeters));
            AtmosphereShell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            AtmosphereShell->SetCastShadow(false);
            AtmosphereShell->bNeverDistanceCull = true;
            AtmosphereShell->RegisterComponent();

            AtmosphereMID = UMaterialInstanceDynamic::Create(AtmoMaterial, this);
            AtmosphereShell->SetMaterial(0, AtmosphereMID);
            AtmosphereMID->SetScalarParameterValue(TEXT("GroundRadius"), Req / Top);
            AtmosphereMID->SetVectorParameterValue(TEXT("RayleighBeta"), ToParam(Appearance.RayleighPerMm * 1e-6 * Top));
            AtmosphereMID->SetScalarParameterValue(TEXT("RayleighH"), Appearance.RayleighScaleHeightKm * 1000.0 / Top);
            AtmosphereMID->SetScalarParameterValue(TEXT("MieBeta"), Appearance.MiePerMm * 1e-6 * Top);
            AtmosphereMID->SetScalarParameterValue(TEXT("MieH"), Appearance.MieScaleHeightKm * 1000.0 / Top);
            AtmosphereMID->SetScalarParameterValue(TEXT("MieG"), Appearance.MieAnisotropy);
            AtmosphereMID->SetVectorParameterValue(TEXT("MieTint"), Appearance.MieTint);
        }
    }

    // Rings: the engine Plane (100 x 100, local XY) scaled so its half-size is the outer edge,
    // lying in the equatorial plane (the body mesh's local XY).
    if (bHasAppearance && Definition->RingOuterRadiusMeters > 0.0)
    {
        if (UMaterialInterface* RingMaterial = Appearance.RingMaterial.LoadSynchronous())
        {
            const double Rout = Definition->RingOuterRadiusMeters;
            Rings = NewObject<UStaticMeshComponent>(Body, TEXT("Rings"));
            Rings->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
            Rings->SetupAttachment(Body->BodyMesh);
            Rings->SetRelativeScale3D(FVector(Rout / Req, Rout / Req, 1.0));
            Rings->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Rings->SetCastShadow(false);
            Rings->bNeverDistanceCull = true;
            Rings->RegisterComponent();

            RingMID = UMaterialInstanceDynamic::Create(RingMaterial, this);
            Rings->SetMaterial(0, RingMID);
            RingMID->SetScalarParameterValue(TEXT("RingInner"), Definition->RingInnerRadiusMeters / Rout);
            RingMID->SetScalarParameterValue(TEXT("PlanetRadius"), Req / Rout);
            RingMID->SetScalarParameterValue(TEXT("PlanetPolar"), Definition->PolarRadiusMeters / Rout);
        }
    }

    if (Definition->BodyType == EAstroBodyType::Star)
    {
        Corona = NewObject<USunCoronaComponent>(Body, TEXT("Corona"));
        Corona->SetupAttachment(Body->BodyMesh);
        Corona->RegisterComponent();
    }
}

void UBodyShadingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    const AAstroBody* Body = Cast<AAstroBody>(GetOwner());
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    const FBodyDefinition* Definition = Body ? Body->GetDefinition() : nullptr;
    if (!Definition || !Sim || !Sim->IsReady())
    {
        return;
    }
    const FSolarSystemSimulation& State = Sim->GetSimulation();
    const FBodyRegistry& Registry = Sim->GetRegistry();
    const int32 StarIndex = Registry.GetStarIndex();
    const FBodyDefinition& Star = Registry.Get(StarIndex);
    const double Efficacy = GetDefault<UAstroRenderingSettings>()->LuminousEfficacy;
    const float Time = static_cast<float>(FMath::Fmod(State.GetSimSeconds(), 1.0e6));

    if (Definition->BodyType == EAstroBodyType::Star)
    {
        // Photosphere luminance: radiant exitance L / (4 pi R^2), to lux, / pi for cd/m^2.
        const double R = Definition->GetMeanRadiusMeters();
        const double Luminance = Definition->LuminosityWatts / (4.0 * AstroConstants::Pi * R * R) * Efficacy / AstroConstants::Pi;
        const UAstroRenderingSettings* Settings = GetDefault<UAstroRenderingSettings>();
        const double Disk = FMath::Min(Luminance, Settings->SunDiskLuminanceCap);
        if (SurfaceMID)
        {
            SurfaceMID->SetScalarParameterValue(TEXT("Luminance"), Disk);
            SurfaceMID->SetScalarParameterValue(TEXT("Time"), Time);
        }
        if (Corona)
        {
            Corona->SetCoronaLuminance(Disk * Settings->CoronaLuminanceFraction);
        }
        return;
    }

    const FAstroVector3d BodyPos = State.GetBodyState(Body->GetBodyIndex()).Position;
    const FAstroVector3d ToStar = State.GetBodyState(StarIndex).Position - BodyPos;
    const double Distance = ToStar.Length();
    SunLux = Star.LuminosityWatts / (4.0 * AstroConstants::Pi * Distance * Distance) * Efficacy;
    const FLinearColor SunDir = ToParam(UAstroSimulationSubsystem::SimToEngineDirection(ToStar / Distance));

    if (SurfaceMID)
    {
        SurfaceMID->SetVectorParameterValue(TEXT("SunDirectionWS"), SunDir);
        SurfaceMID->SetScalarParameterValue(TEXT("SunLux"), SunLux);
        if (bHasAppearance && Appearance.CloudDriftDegPerDay != 0.0)
        {
            const double Days = State.GetSimSeconds() / AstroConstants::SecondsPerDay;
            SurfaceMID->SetScalarParameterValue(TEXT("CloudOffsetU"), FMath::Frac(Days * Appearance.CloudDriftDegPerDay / 360.0));
        }
    }
    if (AtmosphereMID)
    {
        AtmosphereMID->SetVectorParameterValue(TEXT("SunDirectionWS"), SunDir);
        AtmosphereMID->SetScalarParameterValue(TEXT("SunLux"), SunLux);
        bool bInside = false;
        if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
        {
            const FAstroVector3d CameraSim = Sim->EngineToSimPosition(PC->PlayerCameraManager->GetCameraLocation());
            bInside = (CameraSim - BodyPos).Length() < AtmosphereTopMeters;
        }
        AtmosphereMID->SetScalarParameterValue(TEXT("CameraInside"), bInside ? 1.0f : 0.0f);
    }
    if (RingMID)
    {
        RingMID->SetVectorParameterValue(TEXT("SunDirectionWS"), SunDir);
        RingMID->SetScalarParameterValue(TEXT("SunLux"), SunLux);
    }
}
