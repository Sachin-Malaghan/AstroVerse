// See CLAUDE.md Phase 2.
#include "AstroBody.h"
#include "AstroSimulationSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<float> CVarAstroDebugRadiusScale(
    TEXT("astro.Debug.RadiusScale"), 1.0f,
    TEXT("Validation only: multiplies every body's drawn radius (true scale = 1). Physics is unaffected."));

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
    if (const FBodyDefinition* Definition = GetDefinition())
    {
        BodyID = Definition->BodyID;
        MassKg = Definition->MassKg;
        RadiusMeters = Definition->GetMeanRadiusMeters();
        BodyType = Definition->BodyType;
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

    // Oblate spheroid: equatorial radius on local X/Y, polar on local Z (the body's north pole).
    const double Scale = RenderScaleFactor * CVarAstroDebugRadiusScale.GetValueOnGameThread() * 100.0 / MeshRadiusCm;
    SetActorLocationAndRotation(Location, Rotation);
    SetActorScale3D(FVector(Definition->EquatorialRadiusMeters, Definition->EquatorialRadiusMeters, Definition->PolarRadiusMeters) * Scale);
    OnRenderTransformUpdated(RenderScaleFactor);
}
