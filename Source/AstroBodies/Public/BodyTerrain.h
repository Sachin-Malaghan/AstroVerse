#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Math/AstroVector3d.h"
#include "BodyTerrain.generated.h"
// Surface height model for landable bodies: the single source of truth for "where is
// the ground", shared by the walker (AstroApp) and the terrain mesh (AstroRendering) so
// what you see is what you stand on. Heights are meters above the reference ellipsoid
// in DT_Planets / DT_Moons. See CLAUDE.md Phase 8.
//
// Relief is procedural (fractal gradient noise + crater fields) unless a heightmap is
// supplied; real DEMs (LOLA, MOLA, ETOPO) drop in as HeightmapFile without code changes.

USTRUCT(BlueprintType)
struct ASTROBODIES_API FAstroTerrainRow : public FTableRowBase
{
    GENERATED_BODY()

    // False for gas giants: no solid surface to stand on.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    bool bHasSolidSurface = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    double ReliefAmplitudeM = 0.0;

    // Largest procedural feature size.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    double BaseWavelengthKm = 500.0;

    // Amplitude falloff per octave (0.5 = classic fBm; higher = rougher).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    double Roughness = 0.5;

    // RMS-ish amplitude of metre-to-200 m roughness (boulders, ripples, regolith texture).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    double MicroReliefM = 0.0;

    // Craters per unit area scale (0 = none).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    double CraterDensity = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    double CraterMaxRadiusKm = 0.0;

    // Equirect grayscale image, white = ocean (Content-relative). Ocean is flat at sea level.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    FString OceanMaskFile;

    // Optional equirect DEM (Content-relative), gray 0..1 mapped to HeightmapMinM..MaxM.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    FString HeightmapFile;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    double HeightmapMinM = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    double HeightmapMaxM = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    FString Notes;
};

// Equirectangular grayscale image, bilinearly sampled on the CPU.
struct ASTROBODIES_API FEquirectMap
{
    int32 Width = 0;
    int32 Height = 0;
    TArray<uint16> Values; // 0..65535; maps wider than MaxWidth are box-downsampled on load

    static constexpr int32 MaxWidth = 4096;

    bool LoadFromFile(const FString& Path);
    bool IsValid() const { return Width > 0 && Height > 0; }
    // Unit direction in the body-fixed frame (X = prime meridian, Z = north).
    float Sample(const FAstroVector3d& Dir) const;
};

class ASTROBODIES_API FBodyTerrain
{
public:
    // SeedName makes each body's procedural relief distinct and stable.
    void Initialize(const FAstroTerrainRow& Row, FName SeedName, double EquatorialRadius, double PolarRadius, const FString& ContentDir);

    bool HasSolidSurface() const { return Row.bHasSolidSurface; }

    // Height above the ellipsoid (m) at a body-fixed direction. Features smaller than
    // MinFeatureMeters are skipped (coarse LODs ask for less detail and pay less).
    double HeightAt(const FAstroVector3d& DirBodyFixed, double MinFeatureMeters = 1.0) const;

    // Ellipsoid radius along a body-fixed direction.
    double EllipsoidRadius(const FAstroVector3d& DirBodyFixed) const;

    // Ground point (body-fixed meters) along a direction.
    FAstroVector3d SurfacePoint(const FAstroVector3d& DirBodyFixed, double MinFeatureMeters = 1.0) const
    {
        return DirBodyFixed * (EllipsoidRadius(DirBodyFixed) + HeightAt(DirBodyFixed, MinFeatureMeters));
    }

    bool IsOcean(const FAstroVector3d& DirBodyFixed) const;

private:
    double FractalRelief(const FAstroVector3d& Dir, double MinFeatureMeters) const;
    double MicroRelief(const FAstroVector3d& Dir, double MinFeatureMeters) const;
    double Craters(const FAstroVector3d& Dir, double MinFeatureMeters) const;

    FAstroTerrainRow Row;
    double Req = 1.0;
    double Rpol = 1.0;
    uint32 Seed = 0;
    FEquirectMap OceanMask;
    FEquirectMap Heightmap;
};
