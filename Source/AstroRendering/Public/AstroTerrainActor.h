#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/AstroMatrix3d.h"
#include "AstroTerrainActor.generated.h"
// Close-range surface for the body the camera is near: a geometry clipmap of square
// rings (each twice the spacing of the one inside it) on the body's tangent plane,
// displaced by FBodyTerrain — the same model the walker stands on. Rings rebuild on a
// worker thread only when their snapped center moves. Lit by the star's directional
// light (correct near the camera), so relief casts real shadows. See CLAUDE.md Phase 8.

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class FBodyTerrain;
struct FBodyDefinition;

UCLASS(NotPlaceable)
class ASTRORENDERING_API AAstroTerrainActor : public AActor
{
    GENERATED_BODY()

public:
    AAstroTerrainActor();

    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, Category = "Astro|Terrain")
    TObjectPtr<UProceduralMeshComponent> Mesh;

    // Terrain takes over below this altitude (m); the body's sphere mesh is hidden meanwhile.
    UPROPERTY(EditAnywhere, Category = "Astro|Terrain")
    double ActivationAltitude = 150000.0;

    // Quads per ring side.
    static constexpr int32 GridSize = 64;
    static constexpr int32 MaxRings = 16;

    int32 GetActiveBody() const { return ActiveBody; }
    bool IsBuilt() const { return bHasGeometry; }

private:
    // A ring's grid in units of its spacing. The hole where the finer ring sits depends on
    // that ring's center too, so it is part of the key.
    struct FRingKey
    {
        int64 CenterX = TNumericLimits<int64>::Max();
        int64 CenterY = TNumericLimits<int64>::Max();
        int64 InnerX = 0;
        int64 InnerY = 0;
        double Spacing = 0.0;
        bool operator==(const FRingKey& O) const
        {
            return CenterX == O.CenterX && CenterY == O.CenterY && InnerX == O.InnerX && InnerY == O.InnerY && Spacing == O.Spacing;
        }
    };

    struct FRingMesh
    {
        TArray<FVector> Vertices;
        TArray<int32> Triangles;
        TArray<FVector> Normals;
        TArray<FVector2D> UV0;   // body-fixed unit direction X, Y
        TArray<FVector2D> UV1;   // body-fixed unit direction Z (texture lookup happens per pixel)
    };

    void Activate(int32 BodyIndex, const FAstroVector3d& CameraBodyFixed);
    void Deactivate();
    void UpdateRings(const FAstroVector3d& CameraBodyFixed, double Altitude);
    void BuildRingAsync(int32 Ring, const FRingKey& Key, bool bInner);
    void UpdateTransform();
    void SetBodySphereHidden(int32 BodyIndex, bool bHide);
    void SetupMaterial(int32 BodyIndex);

    int32 ActiveBody = INDEX_NONE;
    TSharedPtr<const FBodyTerrain, ESPMode::ThreadSafe> Terrain;
    // Tangent-plane patch: center direction and local east/north/up (body-fixed).
    FAstroVector3d PatchCenterDir;
    FAstroVector3d PatchOrigin;   // body-fixed surface point at the center (m)
    FAstroMatrix3d PatchBasis;    // rows: east, north, up
    double BaseSpacing = 1.0;
    int32 NumRings = 0;

    TArray<FRingKey> RingKeys;
    TArray<uint32> RingGeneration;
    bool bHasGeometry = false;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> TerrainMID;
};
