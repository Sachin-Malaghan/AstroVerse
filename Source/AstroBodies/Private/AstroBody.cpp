// See CLAUDE.md Phase 2.
#include "AstroBody.h"
#include "AstroBodiesSettings.h"
#include "AstroSimulationSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// Validation aids for Phase 4. They change only what is drawn, never the physics.
static TAutoConsoleVariable<float> CVarAstroDebugRadiusScale(
    TEXT("astro.Debug.RadiusScale"), 1.0f,
    TEXT("Validation only: multiplies planet and moon drawn radii (true scale = 1)."));
static TAutoConsoleVariable<float> CVarAstroDebugStarRadiusScale(
    TEXT("astro.Debug.StarRadiusScale"), 1.0f,
    TEXT("Validation only: multiplies the star's drawn radius (true scale = 1)."));
static TAutoConsoleVariable<int32> CVarAstroDebugTrails(
    TEXT("astro.Debug.Trails"), 0,
    TEXT("Validation only: 1 draws each body's recent path (moons relative to their planet)."));

namespace
{
    constexpr int32 TrailSamplesPerOrbit = 180;
}

AAstroBody::AAstroBody()
{
    PrimaryActorTick.bCanEverTick = true;
    // After pawns/cameras have moved this frame, so the floating origin is current.
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    RootComponent = BodyMesh;
    BodyMesh->SetMobility(EComponentMobility::Movable);
    BodyMesh->SetCollisionProfileName(TEXT("BlockAll"));
    // Planet-sized bounds must never be distance-culled.
    BodyMesh->bNeverDistanceCull = true;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Sphere.Succeeded())
    {
        BodyMesh->SetStaticMesh(Sphere.Object);
    }
}

void AAstroBody::BindToSimulation(UAstroSimulationSubsystem* InSimulation, int32 InBodyIndex)
{
    Simulation = InSimulation;
    BodyIndex = InBodyIndex;
    TrailPoints.Reset();
    if (const FBodyDefinition* Definition = GetDefinition())
    {
        BodyID = Definition->BodyID;
        MassKg = Definition->MassKg;
        RadiusMeters = Definition->GetMeanRadiusMeters();
        BodyType = Definition->BodyType;

        const UAstroBodiesSettings* Settings = GetDefault<UAstroBodiesSettings>();
        const TSoftObjectPtr<UMaterialInterface>& Material = BodyType == EAstroBodyType::Star ? Settings->StarMaterial : Settings->PlanetMaterial;
        if (UMaterialInterface* Loaded = Material.LoadSynchronous())
        {
            BodyMesh->SetMaterial(0, Loaded);
        }
        SurfaceMaterial = BodyType == EAstroBodyType::Star ? nullptr : BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
        OnBoundToDefinition(*Definition);
    }
}

const FBodyDefinition* AAstroBody::GetDefinition() const
{
    const UAstroSimulationSubsystem* Sim = Simulation.Get();
    return Sim && Sim->GetRegistry().GetAll().IsValidIndex(BodyIndex) ? &Sim->GetRegistry().Get(BodyIndex) : nullptr;
}

void AAstroBody::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const UAstroSimulationSubsystem* Sim = Simulation.Get();
    const FBodyDefinition* Definition = GetDefinition();
    FVector Location;
    FQuat Rotation;
    if (!Definition || !Sim->GetBodyRenderTransform(BodyIndex, Location, Rotation, RenderScaleFactor))
    {
        return;
    }

    const float DebugScale = BodyType == EAstroBodyType::Star
        ? CVarAstroDebugStarRadiusScale.GetValueOnGameThread()
        : CVarAstroDebugRadiusScale.GetValueOnGameThread();

    // Oblate spheroid: equatorial radius on local X/Y, polar on local Z (the body's north pole).
    const double Scale = RenderScaleFactor * DebugScale * 100.0 / MeshRadiusCm;
    SetActorLocationAndRotation(Location, Rotation);
    SetActorScale3D(FVector(Definition->EquatorialRadiusMeters, Definition->EquatorialRadiusMeters, Definition->PolarRadiusMeters) * Scale);
    OnRenderTransformUpdated(RenderScaleFactor);

    UpdateSunDirection();
    UpdateDebugTrail();
}

void AAstroBody::UpdateDebugTrail()
{
    const UAstroSimulationSubsystem* Sim = Simulation.Get();
    const FBodyDefinition* Definition = GetDefinition();
    if (CVarAstroDebugTrails.GetValueOnGameThread() == 0 || !Definition || Definition->ParentIndex == INDEX_NONE)
    {
        TrailPoints.Reset();
        return;
    }

    const FSolarSystemSimulation& State = Sim->GetSimulation();
    const double Now = State.GetSimSeconds();
    const FAstroVector3d ParentPos = State.GetBodyState(Definition->ParentIndex).Position;
    // Planets trail around the barycenter; moons trail around their (moving) planet.
    const FAstroVector3d Frame = Definition->BodyType == EAstroBodyType::Moon ? ParentPos : FAstroVector3d();
    const double Period = KeplerOrbit::OrbitalPeriod(Definition->Elements, Sim->GetRegistry().GetOrbitMu(BodyIndex));
    const double SampleInterval = Period / TrailSamplesPerOrbit;

    if (TrailPoints.Num() == 0 || FMath::Abs(Now - LastTrailSampleTime) >= SampleInterval)
    {
        TrailPoints.Add(State.GetBodyState(BodyIndex).Position - Frame);
        LastTrailSampleTime = Now;
        if (TrailPoints.Num() > TrailSamplesPerOrbit)
        {
            TrailPoints.RemoveAt(0);
        }
    }

    const FColor Color = Definition->BodyType == EAstroBodyType::Moon ? FColor(120, 200, 255) : FColor(255, 200, 90);
    FVector Previous = Sim->SimToScaledEnginePosition(TrailPoints[0] + Frame);
    for (int32 i = 1; i < TrailPoints.Num(); ++i)
    {
        const FVector Next = Sim->SimToScaledEnginePosition(TrailPoints[i] + Frame);
        DrawDebugLine(GetWorld(), Previous, Next, Color, false, 0.0f, SDPG_Foreground, 0.0f);
        Previous = Next;
    }
    DrawDebugLine(GetWorld(), Previous, GetActorLocation(), Color, false, 0.0f, SDPG_Foreground, 0.0f);
}

void AAstroBody::UpdateSunDirection()
{
    const UAstroSimulationSubsystem* Sim = Simulation.Get();
    if (!SurfaceMaterial || !Sim)
    {
        return;
    }
    const int32 Star = Sim->GetRegistry().GetStarIndex();
    const FAstroVector3d ToStar = Sim->GetSimulation().GetBodyState(Star).Position - Sim->GetSimulation().GetBodyState(BodyIndex).Position;
    const FVector Direction = UAstroSimulationSubsystem::SimToEngineDirection(ToStar.Normalized());
    SurfaceMaterial->SetVectorParameterValue(TEXT("SunDirection"), FLinearColor(Direction.X, Direction.Y, Direction.Z, 0.0f));
}

void AAstroBody::SetHighDetail(bool bInHighDetail)
{
    if (bHighDetail != bInHighDetail)
    {
        bHighDetail = bInHighDetail;
        OnDetailChanged(bHighDetail);
    }
}
