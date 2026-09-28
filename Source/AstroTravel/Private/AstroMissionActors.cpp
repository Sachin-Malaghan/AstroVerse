// See CLAUDE.md "Missions" and "Vehicles".
#include "AstroMissionActors.h"
#include "AstroVehicleData.h"
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

namespace
{
    // Plume look per propellant: outer colour/brightness, core colour/brightness, sea-level
    // length and width in exit radii.
    struct FPlumeLook { FLinearColor Outer; float OuterGain; FLinearColor Core; float CoreGain; float Length; float Width; };
    FPlumeLook LookOf(EAstroPlumeKind Kind)
    {
        switch (Kind)
        {
        case EAstroPlumeKind::Solid:      return { FLinearColor(1.0f, 0.82f, 0.55f), 1.6f, FLinearColor(1.0f, 0.95f, 0.85f), 3.0f, 34.0f, 3.2f };
        case EAstroPlumeKind::Kerolox:    return { FLinearColor(1.0f, 0.55f, 0.2f), 1.0f, FLinearColor(1.0f, 0.88f, 0.65f), 2.6f, 26.0f, 3.0f };
        case EAstroPlumeKind::Hydrolox:   return { FLinearColor(0.65f, 0.72f, 1.0f), 0.35f, FLinearColor(0.9f, 0.85f, 1.0f), 1.3f, 12.0f, 2.2f };
        default:                          return { FLinearColor(1.0f, 0.45f, 0.2f), 0.6f, FLinearColor(1.0f, 0.72f, 0.45f), 1.5f, 18.0f, 2.6f };
        }
    }
}

AAstroRocketActor::AAstroRocketActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.Add(TEXT("AstroSolarSystem"));
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
    PadRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Pad"));
    PadRoot->SetupAttachment(Root);
    PadRoot->SetUsingAbsoluteLocation(true);
    PadRoot->SetUsingAbsoluteRotation(true);
}

void AAstroRocketActor::BeginPlay()
{
    Super::BeginPlay();
}

UStaticMeshComponent* AAstroRocketActor::NewPart(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& CentreM, const FVector& Scale, const FRotator& Rotation)
{
    UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
    C->SetupAttachment(Parent);
    C->SetStaticMesh(Mesh);
    C->SetRelativeLocationAndRotation(CentreM * 100.0, Rotation);
    C->SetRelativeScale3D(Scale);
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetCastShadow(true);
    C->bNeverDistanceCull = true;
    C->RegisterComponent();
    Keep.Add(C);
    return C;
}

int32 AAstroRocketActor::FindOrAddGroup(FName Name, double SeparateAt, const FVector& Push)
{
    for (int32 i = 0; i < Groups.Num(); ++i)
    {
        // Parts of one group that separate at different times or push different ways (left and
        // right boosters) get their own roots.
        if (Groups[i].Name == Name && Groups[i].SeparateAt == SeparateAt && Groups[i].PushMS.Equals(Push))
        {
            return i;
        }
    }
    FGroup G;
    G.Name = Name;
    G.SeparateAt = SeparateAt;
    G.PushMS = Push;
    G.Root = NewObject<USceneComponent>(this);
    G.Root->SetupAttachment(Root);
    G.Root->RegisterComponent();
    Keep.Add(G.Root);
    return Groups.Add(G);
}

bool AAstroRocketActor::BuildVehicle(FName Vehicle)
{
    const FAstroVehicleRow* Row = FAstroVehicleCatalog::Find(Vehicle);
    const TArray<const FAstroVehiclePartRow*> PartRows = FAstroVehicleCatalog::Parts(Vehicle);
    if (!Row || PartRows.Num() == 0)
    {
        return false;
    }
    FBox Craft(ForceInit);
    for (const FAstroVehiclePartRow* P : PartRows)
    {
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *P->Mesh);
        if (!Mesh)
        {
            UE_LOG(LogTemp, Warning, TEXT("AstroVehicle %s: mesh %s not found"), *Vehicle.ToString(), *P->Mesh);
            continue;
        }
        const int32 G = FindOrAddGroup(P->Group, P->SeparateAtS, P->SeparationPushMS);
        NewPart(Groups[G].Root, Mesh, FVector::ZeroVector, FVector::OneVector);
        if (P->SeparateAtS < 0.0)
        {
            Craft += Mesh->GetBoundingBox();
        }
    }
    CraftCentreM = Craft.IsValid ? Craft.GetCenter() / 100.0 : FVector(0, 0, Row->HeightM * 0.8);

    const FShapes S = LoadShapes();
    for (const FAstroVehicleEngineRow* E : FAstroVehicleCatalog::Engines(Vehicle))
    {
        int32 G = INDEX_NONE;
        for (int32 i = 0; i < Groups.Num() && G == INDEX_NONE; ++i)
        {
            // The engine's group; with split groups (left / right boosters) the one on its side.
            if (Groups[i].Name == E->Group && (Groups[i].PushMS.IsNearlyZero() || FVector::DotProduct(Groups[i].PushMS, E->ExitM) > 0.0))
            {
                G = i;
            }
        }
        if (G == INDEX_NONE)
        {
            continue;
        }
        FPlume P;
        P.Group = G;
        P.Kind = static_cast<uint8>(E->Kind);
        P.Start = E->BurnStartS;
        P.End = E->BurnEndS;
        P.RadiusM = E->ExitRadiusM;
        P.ExitM = E->ExitM;
        P.Outer = NewPart(Groups[G].Root, S.Cone, E->ExitM, FVector::OneVector);
        P.Core = NewPart(Groups[G].Root, S.Cone, E->ExitM, FVector::OneVector);
        for (UStaticMeshComponent* C : { P.Outer.Get(), P.Core.Get() })
        {
            C->SetCastShadow(false);
            C->SetVisibility(false);
        }
        P.OuterMID = MakePlumeMID(this, P.Outer);
        P.CoreMID = MakePlumeMID(this, P.Core);
        Keep.Add(P.OuterMID);
        Keep.Add(P.CoreMID);
        Plumes.Add(P);
    }

    // Pad: the vehicle's platform mesh, or our generic deck, pedestal and tower.
    UStaticMesh* PadMesh = Row->PadMesh.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Row->PadMesh);
    if (PadMesh)
    {
        NewPart(PadRoot, PadMesh, FVector::ZeroVector, FVector::OneVector, FRotator(0, Row->PadYawDeg, 0));
    }
    else
    {
        BuildGenericPad(Row->PadDeckHeightM);
    }
    ApplyColours(this, PendingColours);
    return true;
}

void AAstroRocketActor::BuildGenericPad(double DeckHeightM)
{
    const FShapes S = LoadShapes();
    const FLinearColor Rust(0.35f, 0.12f, 0.08f);
    auto Add = [this](UStaticMesh* Mesh, const FVector& CentreM, const FVector& SizeM, const FLinearColor& Colour)
    {
        PendingColours.Emplace(NewPart(PadRoot, Mesh, CentreM, SizeM), Colour);
    };
    Add(S.Cube, FVector(0, 0, -1.8), FVector(40.0, 40.0, 3.0), Grey);
    Add(S.Cube, FVector(0, 12, -1.9), FVector(8.0, 18.0, 3.0), Dark);
    Add(S.Cube, FVector(-11.0, 0, 38.0), FVector(4.0, 4.0, 80.0), Rust);
    Add(S.Cube, FVector(-7.0, 0, 50.0), FVector(8.0, 1.2, 1.2), Rust);
    for (int32 k = 0; k < 4; ++k)
    {
        // Launch pedestal: posts holding the vehicle DeckHeightM above the deck (the flame shows).
        const double A = k * UE_HALF_PI + UE_PI / 4.0;
        Add(S.Cube, FVector(FMath::Cos(A) * 1.5, FMath::Sin(A) * 1.5, DeckHeightM * 0.5), FVector(0.8, 0.8, DeckHeightM), Dark);
        Add(S.Cylinder, FVector(k < 2 ? -16 : 16, k % 2 ? -16 : 16, 45.0), FVector(0.8, 0.8, 90.0), Grey);
    }
}

double AAstroRocketActor::GetFirstSeparation() const
{
    double First = -1.0;
    for (const FGroup& G : Groups)
    {
        if (G.SeparateAt >= 0.0 && (First < 0.0 || G.SeparateAt < First))
        {
            First = G.SeparateAt;
        }
    }
    return First;
}

void AAstroRocketActor::SeparateGroup(int32 Index)
{
    FGroup& G = Groups[Index];
    G.bSeparated = true;
    G.Root->SetUsingAbsoluteLocation(true);
    G.Root->SetUsingAbsoluteRotation(true);
}

void AAstroRocketActor::SetGroupTransform(int32 Index, const FVector& Base, const FQuat& Rotation)
{
    if (Groups[Index].bSeparated)
    {
        Groups[Index].Root->SetWorldLocationAndRotation(Base, Rotation);
    }
}

void AAstroRocketActor::SetGroupVisible(int32 Index, bool bVisible)
{
    Groups[Index].Root->SetVisibility(bVisible, true);
    RestorePlumes();
}

void AAstroRocketActor::RestorePlumes()
{
    for (FPlume& P : Plumes)
    {
        const bool bShow = P.bOn && Groups[P.Group].Root->IsVisible();
        P.Outer->SetVisibility(bShow);
        P.Core->SetVisibility(bShow);
    }
}

void AAstroRocketActor::SetRocketTransform(const FVector& Base, const FQuat& Rotation)
{
    SetActorLocationAndRotation(Base, Rotation);
}

void AAstroRocketActor::ApplyPlume(FPlume& P, float Power, float VacuumFraction, double ExposureWhite)
{
    const bool bOn = Power > 0.01f && !Groups[P.Group].bSeparated;
    P.bOn = bOn;
    P.Outer->SetVisibility(bOn);
    P.Core->SetVisibility(bOn);
    if (!bOn)
    {
        return;
    }
    const FPlumeLook L = LookOf(static_cast<EAstroPlumeKind>(P.Kind));
    // In thin air the plume balloons outward and dims per area.
    const float Spread = 1.0f + 2.5f * VacuumFraction;
    const double Len = P.RadiusM * L.Length * (1.0 + 0.6 * VacuumFraction) * FMath::Lerp(0.6, 1.0, static_cast<double>(Power));
    const double Wid = P.RadiusM * L.Width * Spread;
    // Cone apex (+Z, local 50) at the nozzle exit, widening away from the vehicle.
    P.Outer->SetRelativeLocation((P.ExitM - FVector(0, 0, Len * 0.5)) * 100.0);
    P.Outer->SetRelativeScale3D(FVector(Wid, Wid, Len));
    const double CoreLen = Len * 0.3, CoreWid = P.RadiusM * 1.6;
    P.Core->SetRelativeLocation((P.ExitM - FVector(0, 0, CoreLen * 0.5)) * 100.0);
    P.Core->SetRelativeScale3D(FVector(CoreWid, CoreWid, CoreLen));
    const float W = static_cast<float>(ExposureWhite) * Power;
    if (P.OuterMID) { P.OuterMID->SetVectorParameterValue(TEXT("Glow"), L.Outer * (L.OuterGain * W / Spread)); }
    if (P.CoreMID) { P.CoreMID->SetVectorParameterValue(TEXT("Glow"), L.Core * (L.CoreGain * W)); }
}

void AAstroRocketActor::SetThrust(double MissionTime, float Throttle, float VacuumFraction, double ExposureWhite)
{
    for (FPlume& P : Plumes)
    {
        const bool bBurning = MissionTime >= P.Start && MissionTime < P.End;
        ApplyPlume(P, bBurning ? Throttle : 0.0f, VacuumFraction, ExposureWhite);
    }
}

void AAstroRocketActor::SetThrustByStage(float Stage1Power, float Stage2Power, float VacuumFraction, double ExposureWhite)
{
    const double First = GetFirstSeparation();
    for (FPlume& P : Plumes)
    {
        ApplyPlume(P, First < 0.0 || P.Start < First ? Stage1Power : Stage2Power, VacuumFraction, ExposureWhite);
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
    for (int32 i = 0; i < Groups.Num(); ++i)
    {
        if (Groups[i].SeparateAt < 0.0)
        {
            Groups[i].Root->SetVisibility(bVisible, true);
        }
    }
    RestorePlumes();
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
