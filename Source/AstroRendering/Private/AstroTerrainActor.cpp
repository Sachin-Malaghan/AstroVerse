// See CLAUDE.md Phase 8.
#include "AstroTerrainActor.h"
#include "Async/Async.h"
#include "AstroBody.h"
#include "AstroRenderingSettings.h"
#include "AstroSimulationSubsystem.h"
#include "BodyTerrain.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroTerrain, Log, All);

namespace
{
    // Tangent-plane (east, north) meters -> body-fixed surface point.
    FAstroVector3d PlaneToSurface(const FBodyTerrain& Terrain, const FAstroVector3d& CenterPoint, const FAstroMatrix3d& Basis,
                                  double X, double Y, double MinFeature, FAstroVector3d& OutDir)
    {
        const FAstroVector3d East(Basis.M[0][0], Basis.M[0][1], Basis.M[0][2]);
        const FAstroVector3d North(Basis.M[1][0], Basis.M[1][1], Basis.M[1][2]);
        OutDir = (CenterPoint + East * X + North * Y).Normalized();
        return Terrain.SurfacePoint(OutDir, MinFeature);
    }
}

AAstroTerrainActor::AAstroTerrainActor()
{
    // Solar-system presentation: hidden while the galaxy scale-domain is shown.
    Tags.Add(TEXT("AstroSolarSystem"));
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;

    Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
    RootComponent = Mesh;
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->bUseAsyncCooking = true;
    Mesh->SetCastShadow(true);
    Mesh->bNeverDistanceCull = true;
    Mesh->SetVisibility(false);
}

void AAstroTerrainActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    const APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!Sim || !Sim->IsReady() || !PC || !PC->PlayerCameraManager)
    {
        return;
    }
    const FBodyRegistry& Registry = Sim->GetRegistry();
    const FAstroVector3d CameraSim = Sim->EngineToSimPosition(PC->PlayerCameraManager->GetCameraLocation());
    const double SimTime = Sim->GetSimulation().GetSimSeconds();

    // Nearest solid body under the activation altitude.
    int32 Best = INDEX_NONE;
    double BestAltitude = ActivationAltitude;
    FAstroVector3d BestCameraBF;
    for (int32 i = 0; i < Registry.Num(); ++i)
    {
        const FBodyDefinition& Body = Registry.Get(i);
        if (!Body.Terrain.IsValid() || !Body.Terrain->HasSolidSurface())
        {
            continue;
        }
        const FAstroVector3d CameraBF = Body.GetOrientationAt(SimTime).Transposed() * (CameraSim - Sim->GetSimulation().GetBodyState(i).Position);
        const double Altitude = CameraBF.Length() - Body.Terrain->EllipsoidRadius(CameraBF.Normalized());
        if (Altitude < BestAltitude)
        {
            Best = i;
            BestAltitude = Altitude;
            BestCameraBF = CameraBF;
        }
    }

    if (Best != ActiveBody)
    {
        Deactivate();
        if (Best != INDEX_NONE)
        {
            Activate(Best, BestCameraBF);
        }
    }
    if (ActiveBody == INDEX_NONE)
    {
        return;
    }
    UpdateRings(BestCameraBF, FMath::Max(BestAltitude, 1.0));
    UpdateTransform();
}

void AAstroTerrainActor::Activate(int32 BodyIndex, const FAstroVector3d& CameraBodyFixed)
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    ActiveBody = BodyIndex;
    Terrain = Sim->GetRegistry().Get(BodyIndex).Terrain;
    PatchCenterDir = FAstroVector3d();
    RingKeys.Init(FRingKey(), MaxRings);
    RingGeneration.Init(0, MaxRings);
    SetupMaterial(BodyIndex);
    UE_LOG(LogAstroTerrain, Display, TEXT("Terrain active on %s"), *Sim->GetRegistry().Get(BodyIndex).BodyID.ToString());
}

void AAstroTerrainActor::Deactivate()
{
    if (ActiveBody != INDEX_NONE)
    {
        SetBodySphereHidden(ActiveBody, false);
    }
    Mesh->ClearAllMeshSections();
    Mesh->SetVisibility(false);
    bHasGeometry = false;
    for (uint32& Generation : RingGeneration)
    {
        ++Generation; // drop in-flight builds
    }
    ActiveBody = INDEX_NONE;
    Terrain.Reset();
}

void AAstroTerrainActor::UpdateRings(const FAstroVector3d& CameraBF, double Altitude)
{
    const FAstroVector3d CameraDir = CameraBF.Normalized();

    // Re-seat the tangent plane when the camera drifts far from its center (gnomonic distortion).
    const double R = Terrain->EllipsoidRadius(CameraDir);
    if (PatchCenterDir.LengthSquared() == 0.0 || (CameraDir - PatchCenterDir).Length() * R > 50000.0)
    {
        PatchCenterDir = CameraDir;
        PatchOrigin = Terrain->SurfacePoint(CameraDir, 1.0);
        FAstroVector3d East = FAstroVector3d(0, 0, 1).Cross(CameraDir);
        East = East.LengthSquared() > 1e-12 ? East.Normalized() : FAstroVector3d(1, 0, 0);
        const FAstroVector3d North = CameraDir.Cross(East);
        PatchBasis = FAstroMatrix3d::FromColumns(East, North, CameraDir).Transposed();
        RingKeys.Init(FRingKey(), MaxRings);
        for (uint32& Generation : RingGeneration)
        {
            ++Generation; // drop builds for the old patch
        }
        Mesh->ClearAllMeshSections();
        bHasGeometry = false;
        SetBodySphereHidden(ActiveBody, false); // until the new patch arrives
    }

    // Finest spacing tracks altitude (power of two, >= 1 m); ring count reaches ~1.5x the horizon.
    double Spacing = 1.0;
    while (Spacing * 32.0 < Altitude)
    {
        Spacing *= 2.0;
    }
    const double Horizon = FMath::Sqrt(2.0 * R * Altitude + Altitude * Altitude);
    const double Reach = FMath::Min(FMath::Max(Horizon * 1.5, 2000.0), R * 1.2);
    int32 Rings = 1;
    while (Rings < MaxRings && GridSize * 0.5 * Spacing * FMath::Pow(2.0, Rings - 1) < Reach)
    {
        ++Rings;
    }
    if (Spacing != BaseSpacing || Rings != NumRings)
    {
        BaseSpacing = Spacing;
        for (int32 i = Rings; i < NumRings; ++i)
        {
            Mesh->ClearMeshSection(i);
            ++RingGeneration[i];
            RingKeys[i] = FRingKey();
        }
        NumRings = Rings;
    }

    // Camera in tangent-plane coordinates.
    const FAstroVector3d Local = PatchBasis * (CameraBF - PatchOrigin);
    int64 InnerX = 0, InnerY = 0;
    for (int32 Ring = 0; Ring < NumRings; ++Ring)
    {
        FRingKey Key;
        Key.Spacing = BaseSpacing * FMath::Pow(2.0, Ring);
        // Snap to 2S so the next ring's grid lines stay aligned.
        Key.CenterX = static_cast<int64>(FMath::RoundToDouble(Local.X / (2.0 * Key.Spacing))) * 2;
        Key.CenterY = static_cast<int64>(FMath::RoundToDouble(Local.Y / (2.0 * Key.Spacing))) * 2;
        Key.InnerX = InnerX;
        Key.InnerY = InnerY;
        if (!(RingKeys[Ring] == Key))
        {
            RingKeys[Ring] = Key;
            BuildRingAsync(Ring, Key, Ring == 0);
        }
        // This ring's center in the next ring's units (exact, since centers snap to 2S).
        InnerX = Key.CenterX / 2;
        InnerY = Key.CenterY / 2;
    }
}

void AAstroTerrainActor::BuildRingAsync(int32 Ring, const FRingKey& Key, bool bInner)
{
    const uint32 Generation = ++RingGeneration[Ring];
    TSharedPtr<const FBodyTerrain, ESPMode::ThreadSafe> TerrainRef = Terrain;
    const FAstroVector3d Origin = PatchOrigin;
    const FAstroMatrix3d Basis = PatchBasis;
    // The gnomonic projection plane touches the ellipsoid under the patch center.
    const FAstroVector3d PlanePoint = PatchCenterDir * TerrainRef->EllipsoidRadius(PatchCenterDir);
    TWeakObjectPtr<AAstroTerrainActor> WeakThis(this);

    Async(EAsyncExecution::ThreadPool, [=]()
    {
        constexpr int32 G = GridSize;
        TSharedPtr<FRingMesh> Out = MakeShared<FRingMesh>();
        const double S = Key.Spacing;
        const double MinFeature = S * 0.75;
        const int32 N = G + 3; // one extra vertex on each side for normals
        TArray<FAstroVector3d> Pos;
        TArray<FAstroVector3d> Dir;
        Pos.SetNumUninitialized(N * N);
        Dir.SetNumUninitialized(N * N);
        for (int32 J = 0; J < N; ++J)
        {
            for (int32 I = 0; I < N; ++I)
            {
                const double X = (Key.CenterX + I - 1 - G / 2) * S;
                const double Y = (Key.CenterY + J - 1 - G / 2) * S;
                FAstroVector3d D;
                const FAstroVector3d Surface = PlaneToSurface(*TerrainRef, PlanePoint, Basis, X, Y, MinFeature, D);
                Pos[J * N + I] = Basis * (Surface - Origin); // east, north, up (m)
                Dir[J * N + I] = D;
            }
        }

        // Patch-local (east, north, up) meters -> engine-local cm (engine Y is mirrored).
        auto ToEngine = [](const FAstroVector3d& V) { return FVector(V.X * 100.0, -V.Y * 100.0, V.Z * 100.0); };
        auto AddVertex = [&](int32 I, int32 J, double Drop)
        {
            const int32 Idx = (J + 1) * N + (I + 1);
            FAstroVector3d P = Pos[Idx];
            P.Z -= Drop;
            Out->Vertices.Add(ToEngine(P));
            const FAstroVector3d Normal = (Pos[Idx + 1] - Pos[Idx - 1]).Cross(Pos[Idx + N] - Pos[Idx - N]).Normalized();
            Out->Normals.Add(FVector(Normal.X, -Normal.Y, Normal.Z));
            // The body-fixed direction drives the texture lookup in the material.
            Out->UV0.Add(FVector2D(Dir[Idx].X, Dir[Idx].Y));
            Out->UV1.Add(FVector2D(Dir[Idx].Z, 0.0));
            return Out->Vertices.Num() - 1;
        };

        // Main grid, (G+1)^2 vertices, skipping quads under the finer ring.
        TArray<int32> Index;
        Index.SetNumUninitialized((G + 1) * (G + 1));
        for (int32 J = 0; J <= G; ++J)
        {
            for (int32 I = 0; I <= G; ++I)
            {
                Index[J * (G + 1) + I] = AddVertex(I, J, 0.0);
            }
        }
        const int64 HoleMinX = Key.InnerX - Key.CenterX + G / 2 - G / 4;
        const int64 HoleMinY = Key.InnerY - Key.CenterY + G / 2 - G / 4;
        for (int32 J = 0; J < G; ++J)
        {
            for (int32 I = 0; I < G; ++I)
            {
                if (!bInner && I >= HoleMinX && I < HoleMinX + G / 2 && J >= HoleMinY && J < HoleMinY + G / 2)
                {
                    continue;
                }
                const int32 A = Index[J * (G + 1) + I], B = Index[J * (G + 1) + I + 1];
                const int32 C = Index[(J + 1) * (G + 1) + I], D = Index[(J + 1) * (G + 1) + I + 1];
                // Front faces up (verified in-engine: the opposite order is culled from above).
                Out->Triangles.Append({ A, B, D, A, D, C });
            }
        }

        // Skirts around the outer edge hide cracks against the next, coarser ring.
        const double Drop = S * 2.0 + 2.0;
        auto Skirt = [&](int32 I0, int32 J0, int32 I1, int32 J1)
        {
            const int32 Top0 = Index[J0 * (G + 1) + I0], Top1 = Index[J1 * (G + 1) + I1];
            const int32 Low0 = AddVertex(I0, J0, Drop), Low1 = AddVertex(I1, J1, Drop);
            // Both windings: skirts are seen from either side.
            Out->Triangles.Append({ Top0, Low1, Top1, Top0, Low0, Low1, Top0, Top1, Low1, Top0, Low1, Low0 });
        };
        for (int32 K = 0; K < G; ++K)
        {
            Skirt(K, 0, K + 1, 0);
            Skirt(K + 1, G, K, G);
            Skirt(0, K + 1, 0, K);
            Skirt(G, K, G, K + 1);
        }

        AsyncTask(ENamedThreads::GameThread, [WeakThis, Ring, Generation, Out]()
        {
            AAstroTerrainActor* Self = WeakThis.Get();
            if (!Self || !Self->RingGeneration.IsValidIndex(Ring) || Self->RingGeneration[Ring] != Generation)
            {
                return; // superseded by a newer build
            }
            // Only the near rings need collision (walker probes, VR teleport traces).
            const bool bCollision = Ring < 4;
            Self->Mesh->CreateMeshSection_LinearColor(Ring, Out->Vertices, Out->Triangles, Out->Normals, Out->UV0, Out->UV1,
                TArray<FVector2D>(), TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), bCollision);
            if (Self->TerrainMID)
            {
                Self->Mesh->SetMaterial(Ring, Self->TerrainMID);
            }
            if (!Self->bHasGeometry)
            {
                Self->bHasGeometry = true;
                Self->Mesh->SetVisibility(true);
                Self->SetBodySphereHidden(Self->ActiveBody, true);
            }
        });
    });
}

void AAstroTerrainActor::UpdateTransform()
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    const FBodyDefinition& Body = Sim->GetRegistry().Get(ActiveBody);
    const FAstroMatrix3d BodyToSim = Body.GetOrientationAt(Sim->GetSimulation().GetSimSeconds());
    const FAstroVector3d OriginSim = Sim->GetSimulation().GetBodyState(ActiveBody).Position + BodyToSim * PatchOrigin;
    // Patch-local (east, north, up) -> body-fixed is PatchBasis^T; then body-fixed -> sim.
    SetActorLocationAndRotation(Sim->SimToEnginePosition(OriginSim), Sim->SimToEngineRotation(BodyToSim * PatchBasis.Transposed()));
}

void AAstroTerrainActor::SetBodySphereHidden(int32 BodyIndex, bool bHide)
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (AAstroBody* Body = Sim ? Sim->GetBodyActor(BodyIndex) : nullptr)
    {
        Body->BodyMesh->SetVisibility(!bHide);
    }
}

void AAstroTerrainActor::SetupMaterial(int32 BodyIndex)
{
    TerrainMID = nullptr;
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    UMaterialInterface* Base = GetDefault<UAstroRenderingSettings>()->TerrainMaterial.LoadSynchronous();
    const AAstroBody* Body = Sim ? Sim->GetBodyActor(BodyIndex) : nullptr;
    if (!Base || !Body)
    {
        return;
    }
    TerrainMID = UMaterialInstanceDynamic::Create(Base, this);
    // Inherit the body's maps and look from its surface material.
    if (UMaterialInterface* Surface = Body->BodyMesh->GetMaterial(0))
    {
        for (const TCHAR* Name : { TEXT("DayTex"), TEXT("SpecTex"), TEXT("NightTex") })
        {
            UTexture* Texture = nullptr;
            if (Surface->GetTextureParameterValue(FHashedMaterialParameterInfo(Name), Texture) && Texture)
            {
                TerrainMID->SetTextureParameterValue(Name, Texture);
            }
        }
        for (const TCHAR* Name : { TEXT("Procedural"), TEXT("NightNits"), TEXT("SpecAmount") })
        {
            float Value = 0.0f;
            if (Surface->GetScalarParameterValue(FHashedMaterialParameterInfo(Name), Value))
            {
                TerrainMID->SetScalarParameterValue(Name, Value);
            }
        }
        FLinearColor Tint;
        if (Surface->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Tint")), Tint))
        {
            TerrainMID->SetVectorParameterValue(TEXT("Tint"), Tint);
        }
    }
}
