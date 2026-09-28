// See CLAUDE.md "Missions".
#include "AstroMissionActors.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    const TCHAR* HullPath = TEXT("/Game/Rendering/Materials/M_ShipHull.M_ShipHull");
    const TCHAR* PlumePath = TEXT("/Game/Rendering/Materials/M_Exhaust.M_Exhaust");

    const FLinearColor White(0.78f, 0.78f, 0.8f);
    const FLinearColor Dark(0.05f, 0.05f, 0.055f);
    const FLinearColor Grey(0.35f, 0.36f, 0.38f);
    const FLinearColor Gold(0.75f, 0.52f, 0.16f);
    const FLinearColor Steel(0.5f, 0.5f, 0.52f);

    struct FShapes
    {
        UStaticMesh* Cylinder = nullptr;
        UStaticMesh* Cone = nullptr;
        UStaticMesh* Cube = nullptr;
        UStaticMesh* Sphere = nullptr;
    };

    FShapes LoadShapes()
    {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
        static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
        static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
        static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        return FShapes{ Cylinder.Object, Cone.Object, Cube.Object, Sphere.Object };
    }

    // Engine basic shapes are 100 units across, centred on their pivot.
    UStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh,
        const FVector& CentreM, const FVector& SizeM, const FRotator& Rotation)
    {
        UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
        C->SetupAttachment(Parent);
        C->SetStaticMesh(Mesh);
        C->SetRelativeLocationAndRotation(CentreM * 100.0, Rotation);
        C->SetRelativeScale3D(SizeM);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCastShadow(true);
        C->bNeverDistanceCull = true;
        return C;
    }

    void ApplyColours(AActor* Owner, TArray<TPair<TObjectPtr<UStaticMeshComponent>, FLinearColor>>& Pending)
    {
        UMaterialInterface* Hull = LoadObject<UMaterialInterface>(nullptr, HullPath);
        if (!Hull)
        {
            return;
        }
        TMap<uint32, UMaterialInstanceDynamic*> ByColour;
        for (const TPair<TObjectPtr<UStaticMeshComponent>, FLinearColor>& P : Pending)
        {
            const uint32 Key = P.Value.ToFColor(true).DWColor();
            UMaterialInstanceDynamic*& MID = ByColour.FindOrAdd(Key);
            if (!MID)
            {
                MID = UMaterialInstanceDynamic::Create(Hull, Owner);
                MID->SetVectorParameterValue(TEXT("Color"), P.Value);
                MID->SetScalarParameterValue(TEXT("Metallic"), P.Value == Gold ? 0.9f : 0.3f);
                MID->SetScalarParameterValue(TEXT("Roughness"), P.Value == Gold ? 0.35f : 0.5f);
            }
            P.Key->SetMaterial(0, MID);
        }
        Pending.Reset();
    }

    UMaterialInstanceDynamic* MakePlumeMID(AActor* Owner, UStaticMeshComponent* C)
    {
        UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, PlumePath);
        if (!Base)
        {
            return nullptr;
        }
        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Owner);
        C->SetMaterial(0, MID);
        return MID;
    }
}

// ---------------------------------------------------------------------------- rocket

AAstroRocketActor::AAstroRocketActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.Add(TEXT("AstroSolarSystem"));
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
    const FShapes S = LoadShapes();

    // Stage 1: 42 m x 3.7 m, nine engine bells under a dark thrust section.
    Stage1Root = CreateDefaultSubobject<USceneComponent>(TEXT("Stage1"));
    Stage1Root->SetupAttachment(Root);
    Part(Stage1Root, TEXT("S1Body"), S.Cylinder, FVector(0, 0, 21.5), FVector(3.7, 3.7, 39.0), White);
    Part(Stage1Root, TEXT("S1Thrust"), S.Cylinder, FVector(0, 0, 1.5), FVector(3.8, 3.8, 3.0), Dark);
    Part(Stage1Root, TEXT("S1Interstage"), S.Cylinder, FVector(0, 0, 41.2), FVector(3.72, 3.72, 2.4), Dark);
    for (int32 k = 0; k < 9; ++k)
    {
        const double A = k * UE_TWO_PI / 8.0;
        const FVector At = k == 8 ? FVector(0, 0, -0.9) : FVector(FMath::Cos(A) * 1.2, FMath::Sin(A) * 1.2, -0.9);
        Part(Stage1Root, *FString::Printf(TEXT("S1Bell%d"), k), S.Cone, At, FVector(0.9, 0.9, 1.8), Steel, FRotator(180, 0, 0));
    }
    // Grid fins and legs (folded) for a recognisable booster silhouette.
    for (int32 k = 0; k < 4; ++k)
    {
        const double A = k * UE_HALF_PI + UE_PI / 4.0;
        const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.0);
        Part(Stage1Root, *FString::Printf(TEXT("S1Leg%d"), k), S.Cube, Out * 1.95 + FVector(0, 0, 5.0), FVector(0.3, 0.3, 9.0), Dark,
            FRotator(0, FMath::RadiansToDegrees(A), 0));
        Part(Stage1Root, *FString::Printf(TEXT("S1Fin%d"), k), S.Cube, Out * 2.3 + FVector(0, 0, 38.5), FVector(0.9, 0.1, 1.2), Dark,
            FRotator(0, FMath::RadiansToDegrees(A), 0));
    }
    Stage1Plume = Plume(Stage1Root, TEXT("S1Plume"), S.Cone, -1.8, 55.0, 7.0);
    Stage1Core = Plume(Stage1Root, TEXT("S1Core"), S.Cone, -1.8, 14.0, 3.2);

    // Upper stage + crew capsule, above the interstage.
    UpperRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Upper"));
    UpperRoot->SetupAttachment(Root);
    Part(UpperRoot, TEXT("S2Body"), S.Cylinder, FVector(0, 0, 49.0), FVector(3.7, 3.7, 12.0), White);
    Part(UpperRoot, TEXT("S2Bell"), S.Cone, FVector(0, 0, 41.8), FVector(1.8, 1.8, 3.0), Steel, FRotator(180, 0, 0));
    Part(UpperRoot, TEXT("CapsuleTrunk"), S.Cylinder, FVector(0, 0, 56.5), FVector(3.7, 3.7, 3.0), Grey);
    Part(UpperRoot, TEXT("Capsule"), S.Cone, FVector(0, 0, 60.0), FVector(3.7, 3.7, 4.0), White);
    Part(UpperRoot, TEXT("CapsuleNose"), S.Sphere, FVector(0, 0, 62.0), FVector(0.9, 0.9, 0.9), Dark);
    Stage2Plume = Plume(UpperRoot, TEXT("S2Plume"), S.Cone, 40.3, 40.0, 10.0);

    // Payload fairing over the capsule (two halves in reality; one shell here).
    FairingRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Fairing"));
    FairingRoot->SetupAttachment(Root);
    Part(FairingRoot, TEXT("FairingBase"), S.Cylinder, FVector(0, 0, 58.5), FVector(4.6, 4.6, 5.0), White);
    Part(FairingRoot, TEXT("FairingOgive"), S.Cone, FVector(0, 0, 64.5), FVector(4.6, 4.6, 7.0), White);

    // Pad and tower (stay behind at the site).
    PadRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Pad"));
    PadRoot->SetupAttachment(Root);
    PadRoot->SetUsingAbsoluteLocation(true);
    PadRoot->SetUsingAbsoluteRotation(true);
    Part(PadRoot, TEXT("PadDeck"), S.Cube, FVector(0, 0, -1.8), FVector(40.0, 40.0, 3.0), Grey);
    Part(PadRoot, TEXT("Flame trench"), S.Cube, FVector(0, 12, -1.9), FVector(8.0, 18.0, 3.0), Dark);
    Part(PadRoot, TEXT("Tower"), S.Cube, FVector(-9.0, 0, 38.0), FVector(4.0, 4.0, 80.0), FLinearColor(0.35f, 0.12f, 0.08f));
    Part(PadRoot, TEXT("Arm"), S.Cube, FVector(-5.0, 0, 58.0), FVector(8.0, 1.2, 1.2), FLinearColor(0.35f, 0.12f, 0.08f));
    for (int32 k = 0; k < 4; ++k)
    {
        // Launch mount: four posts holding the booster 7 m above the deck (the flame shows).
        const double A = k * UE_HALF_PI + UE_PI / 4.0;
        Part(PadRoot, *FString::Printf(TEXT("Mount%d"), k), S.Cube, FVector(FMath::Cos(A) * 2.6, FMath::Sin(A) * 2.6, 3.5), FVector(0.8, 0.8, 7.0), Dark);
    }
    for (int32 k = 0; k < 4; ++k)
    {
        Part(PadRoot, *FString::Printf(TEXT("Mast%d"), k), S.Cylinder, FVector(k < 2 ? -16 : 16, k % 2 ? -16 : 16, 45.0), FVector(0.8, 0.8, 90.0), Grey);
    }
    SetThrust(0, 0, 0, 1.0);
}

UStaticMeshComponent* AAstroRocketActor::Part(USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh, const FVector& CentreM, const FVector& SizeM, const FLinearColor& Colour, const FRotator& Rotation)
{
    UStaticMeshComponent* C = MakePart(this, Parent, Name, Mesh, CentreM, SizeM, Rotation);
    Parts.Add(C);
    PendingColours.Emplace(C, Colour);
    return C;
}

UStaticMeshComponent* AAstroRocketActor::Plume(USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Cone, double TopM, double LengthM, double WidthM)
{
    // Cone apex (+Z) at the nozzle, widening away from the rocket.
    UStaticMeshComponent* C = MakePart(this, Parent, Name, Cone, FVector(0, 0, TopM - LengthM * 0.5), FVector(WidthM, WidthM, LengthM), FRotator::ZeroRotator);
    C->SetCastShadow(false);
    return C;
}

void AAstroRocketActor::BeginPlay()
{
    Super::BeginPlay();
    ApplyColours(this, PendingColours);
    for (UStaticMeshComponent* P : { Stage1Plume.Get(), Stage1Core.Get(), Stage2Plume.Get() })
    {
        PlumeMIDs.Add(MakePlumeMID(this, P));
    }
    SetThrust(0, 0, 0, 1.0);
}

void AAstroRocketActor::SetRocketTransform(const FVector& Base, const FQuat& Rotation)
{
    SetActorLocationAndRotation(Base, Rotation);
}

void AAstroRocketActor::SetStage1Transform(const FVector& Base, const FQuat& Rotation)
{
    if (bStage1Separated)
    {
        Stage1Root->SetWorldLocationAndRotation(Base, Rotation);
    }
}

void AAstroRocketActor::SetFairingTransform(const FVector& Base, const FQuat& Rotation)
{
    if (bFairingSeparated)
    {
        FairingRoot->SetWorldLocationAndRotation(Base, Rotation);
    }
}

void AAstroRocketActor::SeparateStage1()
{
    bStage1Separated = true;
    Stage1Root->SetUsingAbsoluteLocation(true);
    Stage1Root->SetUsingAbsoluteRotation(true);
}

void AAstroRocketActor::SeparateFairing()
{
    bFairingSeparated = true;
    FairingRoot->SetUsingAbsoluteLocation(true);
    FairingRoot->SetUsingAbsoluteRotation(true);
}

void AAstroRocketActor::SetThrust(float Stage1Power, float Stage2Power, float VacuumFraction, double ExposureWhite)
{
    const float W = static_cast<float>(ExposureWhite);
    Stage1Plume->SetVisibility(Stage1Power > 0.01f);
    Stage1Core->SetVisibility(Stage1Power > 0.01f);
    Stage2Plume->SetVisibility(Stage2Power > 0.01f);
    // In thin air the plume balloons outward.
    const float Spread = 1.0f + 2.5f * VacuumFraction;
    Stage1Plume->SetRelativeScale3D(FVector(7.0f * Spread, 7.0f * Spread, 55.0f * (1.0f + 0.6f * VacuumFraction)));
    if (PlumeMIDs.Num() == 3)
    {
        if (PlumeMIDs[0]) { PlumeMIDs[0]->SetVectorParameterValue(TEXT("Glow"), FLinearColor(1.0f, 0.55f, 0.2f) * (Stage1Power * W * 0.9f / Spread)); }
        if (PlumeMIDs[1]) { PlumeMIDs[1]->SetVectorParameterValue(TEXT("Glow"), FLinearColor(1.0f, 0.9f, 0.7f) * (Stage1Power * W * 2.5f)); }
        if (PlumeMIDs[2]) { PlumeMIDs[2]->SetVectorParameterValue(TEXT("Glow"), FLinearColor(0.7f, 0.75f, 1.0f) * (Stage2Power * W * 0.6f)); }
    }
}

void AAstroRocketActor::SetPadTransform(const FVector& Base, const FQuat& Rotation)
{
    PadRoot->SetWorldLocationAndRotation(Base, Rotation);
}

void AAstroRocketActor::SetPadVisible(bool bVisible)
{
    PadRoot->SetVisibility(bVisible, true);
}

void AAstroRocketActor::SetUpperVisible(bool bVisible)
{
    UpperRoot->SetVisibility(bVisible, true);
}

// ---------------------------------------------------------------------------- ring ship

AAstroRingShipActor::AAstroRingShipActor()
{
    PrimaryActorTick.bCanEverTick = true;
    Tags.Add(TEXT("AstroSolarSystem"));
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
    const FShapes S = LoadShapes();
    const FRotator AlongX(90, 0, 0); // basic cylinders are along Z; lay them along the ship's X

    // Spine, command module and docking port at the front, engine block aft.
    Part(Root, TEXT("Spine"), S.Cylinder, FVector(0, 0, 0), FVector(3.0, 3.0, Length), Grey, AlongX);
    Part(Root, TEXT("Command"), S.Cylinder, FVector(26, 0, 0), FVector(7.0, 7.0, 10.0), White, AlongX);
    Part(Root, TEXT("CommandNose"), S.Cone, FVector(32.5, 0, 0), FVector(7.0, 7.0, 3.0), White, FRotator(-90, 0, 0));
    Part(Root, TEXT("DockingPort"), S.Cylinder, FVector(DockingPortX, 0, 0), FVector(2.2, 2.2, 1.5), Dark, AlongX);
    Part(Root, TEXT("Tank"), S.Cylinder, FVector(-14, 0, 0), FVector(8.0, 8.0, 14.0), Gold, AlongX);
    Part(Root, TEXT("EngineBlock"), S.Cylinder, FVector(-26, 0, 0), FVector(9.0, 9.0, 6.0), Dark, AlongX);
    for (int32 k = 0; k < 4; ++k)
    {
        const double A = k * UE_HALF_PI;
        const FVector At(-31.0, FMath::Cos(A) * 2.6, FMath::Sin(A) * 2.6);
        Part(Root, *FString::Printf(TEXT("Bell%d"), k), S.Cone, At, FVector(2.6, 2.6, 3.5), Steel, FRotator(90, 0, 0));
        UStaticMeshComponent* P = MakePart(this, Root, *FString::Printf(TEXT("ShipPlume%d"), k), S.Cone,
            At + FVector(-12.0, 0, 0), FVector(4.0, 4.0, 22.0), FRotator(-90, 0, 0));
        P->SetCastShadow(false);
        P->SetVisibility(false);
        EnginePlumes.Add(P);
    }
    // Radiators.
    Part(Root, TEXT("RadiatorL"), S.Cube, FVector(-4, 0, 9.0), FVector(18.0, 0.2, 10.0), White);
    Part(Root, TEXT("RadiatorR"), S.Cube, FVector(-4, 0, -9.0), FVector(18.0, 0.2, 10.0), White);

    // The ring: 12 habitat modules on a 30 m radius, joined by spokes to a hub; it spins.
    RingRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Ring"));
    RingRoot->SetupAttachment(Root);
    RingRoot->SetRelativeLocation(FVector(6.0, 0, 0) * 100.0);
    Part(RingRoot, TEXT("Hub"), S.Cylinder, FVector(0, 0, 0), FVector(6.0, 6.0, 4.0), Grey, AlongX);
    for (int32 k = 0; k < 12; ++k)
    {
        const double A = k * UE_TWO_PI / 12.0;
        const FVector Radial(0.0, FMath::Cos(A), FMath::Sin(A));
        const FRotator Face = FRotationMatrix::MakeFromXZ(FVector(1, 0, 0), Radial).Rotator();
        Part(RingRoot, *FString::Printf(TEXT("Module%d"), k), S.Cube, Radial * RingRadius, FVector(5.0, 14.5, 4.0), k % 3 == 0 ? White : Grey, Face);
        if (k % 3 == 0)
        {
            Part(RingRoot, *FString::Printf(TEXT("Spoke%d"), k), S.Cylinder, Radial * (RingRadius * 0.5), FVector(0.9, 0.9, RingRadius - 3.0), Grey,
                FRotationMatrix::MakeFromZ(Radial).Rotator());
        }
    }
}

UStaticMeshComponent* AAstroRingShipActor::Part(USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh, const FVector& CentreM, const FVector& SizeM, const FLinearColor& Colour, const FRotator& Rotation)
{
    UStaticMeshComponent* C = MakePart(this, Parent, Name, Mesh, CentreM, SizeM, Rotation);
    PendingColours.Emplace(C, Colour);
    return C;
}

void AAstroRingShipActor::BeginPlay()
{
    Super::BeginPlay();
    ApplyColours(this, PendingColours);
    for (UStaticMeshComponent* P : EnginePlumes)
    {
        PlumeMIDs.Add(MakePlumeMID(this, P));
    }
}

void AAstroRingShipActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RingAngle = FMath::Fmod(RingAngle + RingRpm * 6.0f * DeltaSeconds, 360.0f);
    RingRoot->SetRelativeRotation(FRotator(0, 0, RingAngle));
}

void AAstroRingShipActor::SetShipTransform(const FVector& Centre, const FQuat& Rotation)
{
    SetActorLocationAndRotation(Centre, Rotation);
}

void AAstroRingShipActor::SetEngines(float Power, double ExposureWhite)
{
    for (int32 k = 0; k < EnginePlumes.Num(); ++k)
    {
        EnginePlumes[k]->SetVisibility(Power > 0.01f);
        if (PlumeMIDs.IsValidIndex(k) && PlumeMIDs[k])
        {
            PlumeMIDs[k]->SetVectorParameterValue(TEXT("Glow"), FLinearColor(0.55f, 0.7f, 1.0f) * static_cast<float>(Power * ExposureWhite * 1.2));
        }
    }
}
