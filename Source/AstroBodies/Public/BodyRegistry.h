#pragma once
#include "CoreMinimal.h"
#include "BodyDataRow.h"
#include "Math/AstroMatrix3d.h"
#include "Physics/KeplerOrbit.h"
#include "BodyTerrain.h"
// Data-driven registry: loads real orbital elements/masses from
// Content/Bodies/DataTables into engine-agnostic body definitions.
// Spawning actors is UAstroSimulationSubsystem's job. See CLAUDE.md Phase 2.

class UDataTable;

// One body, fully resolved to SI units and J2000-ecliptic frames.
struct ASTROBODIES_API FBodyDefinition
{
    FName BodyID;
    FText DisplayName;
    EAstroBodyType BodyType = EAstroBodyType::Planet;
    int32 ParentIndex = INDEX_NONE;
    TArray<int32> ChildIndices;

    double GM = 0.0;                 // m^3/s^2
    double MassKg = 0.0;
    double EquatorialRadiusMeters = 0.0;
    double PolarRadiusMeters = 0.0;
    double LuminosityWatts = 0.0;
    double RingInnerRadiusMeters = 0.0;
    double RingOuterRadiusMeters = 0.0;

    // Orbit about the parent, in OrbitFrame (Sun: unused).
    FKeplerElements Elements;
    // Rotates vectors from the elements' frame into the J2000 ecliptic frame.
    FAstroMatrix3d OrbitFrameToEcliptic;

    // Body-fixed frame at W = 0: columns are the body's X (prime meridian at W=0) / Y / Z (north pole) axes in ecliptic.
    FAstroMatrix3d EquatorFrameToEcliptic;
    double PrimeMeridianAtEpochRad = 0.0;
    double RotationRateRadPerSec = 0.0;
    // Pole at J2000 and its secular drift (deg, deg per Julian century); 0 drift = fixed frame.
    double PoleRADeg = 0.0, PoleDecDeg = 90.0;
    double PoleRARateDegPerCentury = 0.0, PoleDecRateDegPerCentury = 0.0;

    FString Notes;

    // Surface model; null until terrain data is loaded (LoadTerrain).
    TSharedPtr<FBodyTerrain> Terrain;

    double GetMeanRadiusMeters() const { return (2.0 * EquatorialRadiusMeters + PolarRadiusMeters) / 3.0; }
    // Body-fixed -> ecliptic orientation at the given sim time.
    FAstroMatrix3d GetOrientationAt(double SimSeconds) const;
};

class ASTROBODIES_API FBodyRegistry
{
public:
    // Loads planets (Sun included) and optional moons. Returns false and fills
    // OutError on missing parents, duplicate IDs, or non-physical values.
    bool LoadFromDataTables(const UDataTable* PlanetTable, const UDataTable* MoonTable, FString& OutError);

    // Dev/test path: builds transient Data Tables from the CSV sources in Directory
    // (DT_Planets, DT_Moons, and DT_Terrain when present).
    bool LoadFromCSVDirectory(const FString& Directory, FString& OutError);

    // Attaches terrain models from a DT_Terrain-style table; bodies without a row get none.
    void LoadTerrain(const UDataTable* TerrainTable, const FString& ContentDir);

    int32 Num() const { return Bodies.Num(); }
    const FBodyDefinition& Get(int32 Index) const { return Bodies[Index]; }
    const TArray<FBodyDefinition>& GetAll() const { return Bodies; }
    int32 FindIndex(FName BodyID) const;
    int32 GetStarIndex() const { return StarIndex; }

    // Parent-relative state from the mean elements (Kepler), in the ecliptic frame.
    FOrbitalState ComputeRelativeStateAt(int32 Index, double SimSeconds) const;
    // Parent-relative GM used for the conic: G * (M_parent + m_body).
    double GetOrbitMu(int32 Index) const;

private:
    bool AddRows(const UDataTable* Table, FString& OutError);
    bool Resolve(FString& OutError);

    TArray<FBodyDefinition> Bodies;
    TArray<FAstroBodyDataRow> Rows; // parallel to Bodies until resolved
    TMap<FName, int32> IndexByID;
    int32 StarIndex = INDEX_NONE;
};
