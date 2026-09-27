#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Math/AstroVector3d.h"
#include "Math/AstroConstants.h"
#include "BodyTerrain.generated.h"
// Surface height model for landable bodies: the single source of truth for "where is
// the ground", shared by the walker (AstroApp) and the terrain mesh (AstroRendering) so
// what you see is what you stand on. Heights are meters above the reference ellipsoid
// in DT_Planets / DT_Moons. See CLAUDE.md Phase 8.
//
// Relief is procedural (fractal gradient noise + crater fields) unless a heightmap is
// supplied. Real DEMs (LOLA, MOLA, ETOPO; Tools/Data/fetch_dems.py) are raw grids with a
// <file>.json descriptor, read at full resolution; the procedural terms then only add
// detail below the DEM's pixel size (set by the row's amplitudes). A missing DEM file
// falls back to procedural relief with a warning.

USTRUCT(BlueprintType)
struct ASTROBODIES_API FAstroTerrainRow : public FTableRowBase
{
    GENERATED_BODY()

    // Maps (DEM, ocean mask, surface textures) index geodetic latitude (Earth: WGS84) rather
    // than planetocentric (most planetary datasets). See AstroGeodesy.h.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    bool bGeodeticLatitude = false;

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

    // Optional equirect DEM (Content-relative): an image (gray 0..1 mapped to
    // HeightmapMinM..MaxM) or a raw grid with a <file>.json descriptor (heights in metres).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    FString HeightmapFile;

    // Used instead of the procedural relief columns when HeightmapFile is missing (e.g. the
    // DEMs have not been fetched): keeps the body looking right without the data.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    double FallbackReliefAmplitudeM = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    double FallbackBaseWavelengthKm = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    double FallbackCraterMaxRadiusKm = 0.0;

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

    // Raw elevation grid: full resolution, heights (m) = DN * Scale + Offset. See fetch_dems.py.
    bool LoadRawDEM(const FString& Path, const FString& DescriptorPath);
    bool IsRawDEM() const { return DEM.Num() > 0; }
    // OutLocalRange: max - min of the four surrounding samples (m), a local ruggedness measure.
    double SampleMeters(const FAstroVector3d& Dir, double* OutLocalRange = nullptr) const;
    // Grid spacing at the equator (m) for a body of the given radius.
    double PixelSizeMeters(double Radius) const { return Width > 0 ? AstroConstants::TwoPi * Radius / Width : 0.0; }

    TArray<int16> DEM;
    // Converts a body-fixed direction's latitude to the map's: (A/C)^2 for geodetic maps.
    double LatitudeZScale = 1.0;
    double Scale = 1.0, Offset = 0.0;
    double Lon0Deg = 0.0, Lat0Deg = 90.0;
    bool bSouthUp = false;
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

    bool UsesGeodeticLatitude() const { return Row.bGeodeticLatitude; }
    double GetEquatorialRadius() const { return Req; }
    double GetPolarRadius() const { return Rpol; }

    // True when real elevation data is loaded (vs procedural relief only).
    bool HasRealElevation() const { return Heightmap.IsRawDEM(); }
    // Elevation data only (m above the reference surface; 0 without a DEM), e.g. for readouts.
    double DEMHeightAt(const FAstroVector3d& DirBodyFixed) const { return Heightmap.IsRawDEM() ? Heightmap.SampleMeters(DirBodyFixed) : 0.0; }

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
