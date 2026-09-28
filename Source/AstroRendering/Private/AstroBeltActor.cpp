// See CLAUDE.md "Small bodies".
#include "AstroBeltActor.h"
#include "AstroRenderingSubsystem.h"
#include "AstroSimulationSubsystem.h"
#include "AstroSpaceEnvironment.h"
#include "Async/ParallelFor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/AstroConstants.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<int32> CVarAstroBelts(
    TEXT("astro.UI.Belts"), 1, TEXT("Show the asteroid belt, Jupiter Trojans and Kuiper belt."));

namespace
{
    constexpr int32 MainBeltCount = 20000;
    constexpr int32 TrojanCount = 3000;
    constexpr int32 KuiperCount = 10000;

    // Display colours (albedo-ish) and dot sizes (px) per group.
    // Warm tan (main belt, Trojans) and cool grey-blue (Kuiper): distinct from white stars.
    const FLinearColor GroupColour[] = { FLinearColor(0.95f, 0.72f, 0.48f), FLinearColor(0.95f, 0.62f, 0.42f), FLinearColor(0.62f, 0.72f, 0.92f) };
    const float GroupPixels[] = { 2.4f, 2.4f, 2.1f };
    const float GroupBrightness[] = { 0.55f, 0.55f, 0.42f };
    // Typical radius (AU), to fade a belt whose dots would pile into a blob when it is small on screen.
    const double GroupRadiusAU[] = { 2.7, 5.2, 43.0 };

    double Rayleigh(FRandomStream& R, double Sigma)
    {
        return Sigma * FMath::Sqrt(-2.0 * FMath::Loge(FMath::Max(1e-9, (double)R.FRand())));
    }
}

AAstroBeltActor::AAstroBeltActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork; // after the camera and floating origin settle
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
    Tags.Add(TEXT("AstroSolarSystem")); // hidden with the solar system in the galaxy view

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static const TCHAR* Names[] = { TEXT("MainBelt"), TEXT("Trojans"), TEXT("Kuiper") };
    for (int32 g = 0; g < 3; ++g)
    {
        UInstancedStaticMeshComponent* ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Names[g]);
        ISM->SetupAttachment(Root);
        ISM->SetStaticMesh(Plane.Object);
        ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ISM->SetCastShadow(false);
        ISM->SetMobility(EComponentMobility::Movable);
        ISM->bNeverDistanceCull = true;
        ISM->NumCustomDataFloats = 0;
        Groups.Add(ISM);
    }
    GroupMembers.SetNum(3);
}

void AAstroBeltActor::BeginPlay()
{
    Super::BeginPlay();
    SetActorLocation(FVector::ZeroVector);
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Rendering/Materials/M_GalaxyMarker.M_GalaxyMarker"));
    for (int32 g = 0; g < Groups.Num(); ++g)
    {
        if (Base)
        {
            UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
            Groups[g]->SetMaterial(0, MID);
            Glows.Add(MID);
        }
    }
}

void AAstroBeltActor::AddObject(EAstroBeltGroup Group, double A_AU, double E, double IncDeg, double NodeDeg, double ArgDeg, double MDeg)
{
    const double A = A_AU * AstroConstants::AstronomicalUnit;
    const double I = FMath::DegreesToRadians(IncDeg), O = FMath::DegreesToRadians(NodeDeg), W = FMath::DegreesToRadians(ArgDeg);
    const double cO = FMath::Cos(O), sO = FMath::Sin(O), cI = FMath::Cos(I), sI = FMath::Sin(I), cW = FMath::Cos(W), sW = FMath::Sin(W);
    // Perifocal -> ecliptic: P toward periapsis, Q 90 deg ahead in the orbit plane.
    const FAstroVector3d Px(cO * cW - sO * sW * cI, sO * cW + cO * sW * cI, sW * sI);
    const FAstroVector3d Qx(-cO * sW - sO * cW * cI, -sO * sW + cO * cW * cI, cW * sI);
    FBeltObject Obj;
    Obj.P = Px * A;
    Obj.Q = Qx * (A * FMath::Sqrt(1.0 - E * E));
    Obj.Eccentricity = E;
    Obj.MeanAnomalyAtEpoch = FMath::DegreesToRadians(MDeg);
    Obj.MeanMotion = FMath::Sqrt(1.32712440018e20 / (A * A * A)); // Sun's GM
    Obj.Group = static_cast<uint8>(Group);
    GroupMembers[Obj.Group].Add(Objects.Num());
    Objects.Add(Obj);
}

void AAstroBeltActor::Populate()
{
    // Deterministic: the same belt every run.
    FRandomStream R(20260928);
    // Main belt: 2.1-3.3 AU, depleted in the Kirkwood gaps (mean-motion resonances with Jupiter).
    const double Gaps[][2] = { { 2.502, 0.025 }, { 2.825, 0.018 }, { 2.958, 0.015 }, { 3.279, 0.03 } };
    while (GroupMembers[0].Num() < MainBeltCount)
    {
        // Density rises from the inner edge and tapers beyond ~3 AU.
        const double A = 2.1 + 1.2 * FMath::Pow(R.FRand(), 0.85);
        bool bInGap = false;
        for (const auto& Gap : Gaps)
        {
            if (FMath::Abs(A - Gap[0]) < Gap[1] && R.FRand() < 0.92) { bInGap = true; break; }
        }
        if (bInGap || (A > 3.0 && R.FRand() < (A - 3.0) * 1.5))
        {
            continue;
        }
        AddObject(EAstroBeltGroup::MainBelt, A, FMath::Min(Rayleigh(R, 0.08), 0.35), FMath::Min(Rayleigh(R, 7.0), 30.0),
                  R.FRand() * 360.0, R.FRand() * 360.0, R.FRand() * 360.0);
    }
    // Trojans: Jupiter's orbit, clustered 60 deg ahead (L4) and behind (L5), librating +-25 deg.
    const double JupiterL = 34.40, JupiterVarpi = 14.73; // J2000 mean longitude / longitude of perihelion
    for (int32 k = 0; k < TrojanCount; ++k)
    {
        const double Lead = (k % 5 < 3) ? 60.0 : -60.0; // L4 is the richer swarm
        const double L = JupiterL + Lead + R.FRandRange(-1.0f, 1.0f) * 25.0 * R.FRand();
        const double Node = R.FRand() * 360.0;
        const double Varpi = JupiterVarpi + R.FRandRange(-40.0f, 40.0f);
        const double Arg = Varpi - Node;
        const double M = L - Varpi;
        AddObject(EAstroBeltGroup::Trojans, 5.2026 + R.FRandRange(-0.08f, 0.08f), FMath::Min(Rayleigh(R, 0.06), 0.2),
                  FMath::Min(Rayleigh(R, 11.0), 35.0), Node, Arg, M);
    }
    // Kuiper belt: the cold classical belt (42-48 AU, near-circular, flat) and Plutinos
    // (3:2 with Neptune at 39.4 AU, more eccentric and inclined); a sprinkle of hot classicals.
    for (int32 k = 0; k < KuiperCount; ++k)
    {
        const float Pick = R.FRand();
        if (Pick < 0.55f)
        {
            AddObject(EAstroBeltGroup::Kuiper, R.FRandRange(42.0f, 47.5f), FMath::Min(Rayleigh(R, 0.04), 0.12),
                      FMath::Min(Rayleigh(R, 2.5), 8.0), R.FRand() * 360.0, R.FRand() * 360.0, R.FRand() * 360.0);
        }
        else if (Pick < 0.8f)
        {
            AddObject(EAstroBeltGroup::Kuiper, 39.4 + R.FRandRange(-0.3f, 0.3f), R.FRandRange(0.08f, 0.3f),
                      FMath::Min(Rayleigh(R, 10.0), 35.0), R.FRand() * 360.0, R.FRand() * 360.0, R.FRand() * 360.0);
        }
        else
        {
            AddObject(EAstroBeltGroup::Kuiper, R.FRandRange(40.0f, 50.0f), FMath::Min(Rayleigh(R, 0.1), 0.3),
                      FMath::Min(Rayleigh(R, 15.0), 40.0), R.FRand() * 360.0, R.FRand() * 360.0, R.FRand() * 360.0);
        }
    }
    for (int32 g = 0; g < Groups.Num(); ++g)
    {
        TArray<FTransform> Init;
        Init.Init(FTransform::Identity, GroupMembers[g].Num());
        Groups[g]->AddInstances(Init, false);
    }
    bPopulated = true;
}

void AAstroBeltActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    const APlayerController* PC = GetWorld()->GetFirstPlayerController();
    const bool bShow = CVarAstroBelts.GetValueOnGameThread() != 0 && !IsHidden();
    for (UInstancedStaticMeshComponent* ISM : Groups)
    {
        ISM->SetVisibility(bShow);
    }
    if (!bShow || !Sim || !Sim->IsReady() || !PC || !PC->PlayerCameraManager)
    {
        return;
    }
    if (!bPopulated)
    {
        Populate();
    }
    SetActorLocationAndRotation(FVector::ZeroVector, FQuat::Identity);

    const double T = Sim->GetSimulation().GetSimSeconds();
    const FAstroVector3d Sun = Sim->GetSimulation().GetBodyState(Sim->GetRegistry().GetStarIndex()).Position;
    const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
    const FQuat Facing = FRotationMatrix::MakeFromZ(-PC->PlayerCameraManager->GetCameraRotation().Vector()).ToQuat();
    int32 ViewX = 1600, ViewY = 900;
    PC->GetViewportSize(ViewX, ViewY);
    const double RadPerPixel = 2.0 * FMath::Tan(FMath::DegreesToRadians(0.5 * PC->PlayerCameraManager->GetFOVAngle())) / FMath::Max(ViewX, 1);

    // Exposure-relative brightness so the dots read the same everywhere (like the star map).
    double White = 1.2 * FMath::Pow(2.0, 12.0);
    if (const UAstroRenderingSubsystem* Rendering = UAstroRenderingSubsystem::Get(this); Rendering && Rendering->GetEnvironment())
    {
        White = 1.2 * FMath::Pow(2.0, Rendering->GetEnvironment()->GetExposureEV100());
    }

    for (int32 g = 0; g < Groups.Num(); ++g)
    {
        if (Glows.IsValidIndex(g))
        {
            const double CameraSun = FMath::Max((Sim->EngineToSimPosition(Camera) - Sun).Length(), 1.0);
            const double BeltPx = FMath::Atan(GroupRadiusAU[g] * AstroConstants::AstronomicalUnit / CameraSun) / RadPerPixel;
            const double Fade = FMath::Clamp(FMath::Pow(BeltPx / 220.0, 1.5), 0.0, 1.0);
            Glows[g]->SetVectorParameterValue(TEXT("Glow"), GroupColour[g] * static_cast<float>(GroupBrightness[g] * White * Fade));
        }
        const TArray<int32>& Members = GroupMembers[g];
        Scratch.SetNumUninitialized(Members.Num());
        const double Pixels = GroupPixels[g];
        ParallelFor(Members.Num(), [&](int32 k)
        {
            const FBeltObject& O = Objects[Members[k]];
            // Kepler's equation by Newton iteration (e < 0.4: a few steps suffice).
            const double M = O.MeanAnomalyAtEpoch + O.MeanMotion * T;
            double E = M + O.Eccentricity * FMath::Sin(M);
            for (int32 It = 0; It < 4; ++It)
            {
                E -= (E - O.Eccentricity * FMath::Sin(E) - M) / (1.0 - O.Eccentricity * FMath::Cos(E));
            }
            const FAstroVector3d Pos = Sun + O.P * (FMath::Cos(E) - O.Eccentricity) + O.Q * FMath::Sin(E);
            const FVector Location = Sim->SimToScaledEnginePosition(Pos);
            // Constant on-screen size: the engine plane is 100 units across.
            const double Size = FVector::Dist(Location, Camera) * RadPerPixel * Pixels / 100.0;
            Scratch[k] = FTransform(Facing, Location, FVector(Size));
        }, EParallelForFlags::None);
        Groups[g]->BatchUpdateInstancesTransforms(0, Scratch, false, true, true);
    }
}
