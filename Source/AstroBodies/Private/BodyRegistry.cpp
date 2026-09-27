// See CLAUDE.md Phase 2.
#include "BodyRegistry.h"
#include "Engine/DataTable.h"
#include "Math/AstroConstants.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
    constexpr double JulianDateJ2000 = 2451545.0;

    // ICRF-equatorial -> J2000 ecliptic.
    const FAstroMatrix3d& EquatorialToEcliptic()
    {
        static const FAstroMatrix3d M = FAstroMatrix3d::RotationX(-AstroConstants::ObliquityJ2000Rad);
        return M;
    }

    // IAU convention: the body's equator crosses the ICRF equator at RA = alpha0 + 90 deg,
    // tilted by 90 - delta0. W is then measured along the body's equator.
    FAstroMatrix3d EquatorFrameFromPole(double PoleRADeg, double PoleDecDeg)
    {
        using namespace AstroConstants;
        return EquatorialToEcliptic()
             * FAstroMatrix3d::RotationZ((PoleRADeg + 90.0) * DegToRad)
             * FAstroMatrix3d::RotationX((90.0 - PoleDecDeg) * DegToRad);
    }
}

FAstroMatrix3d FBodyDefinition::GetOrientationAt(double SimSeconds) const
{
    const double W = PrimeMeridianAtEpochRad + RotationRateRadPerSec * SimSeconds;
    if (PoleRARateDegPerCentury == 0.0 && PoleDecRateDegPerCentury == 0.0)
    {
        return EquatorFrameToEcliptic * FAstroMatrix3d::RotationZ(W);
    }
    const double T = SimSeconds / (AstroConstants::SecondsPerDay * 36525.0);
    return EquatorFrameFromPole(PoleRADeg + PoleRARateDegPerCentury * T, PoleDecDeg + PoleDecRateDegPerCentury * T)
         * FAstroMatrix3d::RotationZ(W);
}

bool FBodyRegistry::LoadFromDataTables(const UDataTable* PlanetTable, const UDataTable* MoonTable, FString& OutError)
{
    Bodies.Reset();
    Rows.Reset();
    IndexByID.Reset();
    StarIndex = INDEX_NONE;

    if (!PlanetTable)
    {
        OutError = TEXT("Planet table is missing.");
        return false;
    }
    if (!AddRows(PlanetTable, OutError) || (MoonTable && !AddRows(MoonTable, OutError)))
    {
        return false;
    }
    return Resolve(OutError);
}

bool FBodyRegistry::LoadFromCSVDirectory(const FString& Directory, FString& OutError)
{
    auto LoadTable = [&](const TCHAR* FileName, bool bRequired) -> UDataTable*
    {
        const FString Path = FPaths::Combine(Directory, FileName);
        FString CSV;
        if (!FFileHelper::LoadFileToString(CSV, *Path))
        {
            if (bRequired)
            {
                OutError = FString::Printf(TEXT("Could not read %s"), *Path);
            }
            return nullptr;
        }
        UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
        Table->RowStruct = FAstroBodyDataRow::StaticStruct();
        const TArray<FString> Problems = Table->CreateTableFromCSVString(CSV);
        if (Problems.Num() > 0)
        {
            OutError = FString::Printf(TEXT("%s: %s"), FileName, *FString::Join(Problems, TEXT("; ")));
            return nullptr;
        }
        return Table;
    };

    UDataTable* Planets = LoadTable(TEXT("DT_Planets.csv"), true);
    if (!Planets)
    {
        return false;
    }
    UDataTable* Moons = LoadTable(TEXT("DT_Moons.csv"), false);
    if (!OutError.IsEmpty())
    {
        return false;
    }
    if (!LoadFromDataTables(Planets, Moons, OutError))
    {
        return false;
    }

    FString TerrainCSV;
    if (FFileHelper::LoadFileToString(TerrainCSV, *FPaths::Combine(Directory, TEXT("DT_Terrain.csv"))))
    {
        UDataTable* Terrain = NewObject<UDataTable>(GetTransientPackage());
        Terrain->RowStruct = FAstroTerrainRow::StaticStruct();
        Terrain->CreateTableFromCSVString(TerrainCSV);
        LoadTerrain(Terrain, FPaths::ProjectContentDir());
    }
    return true;
}

bool FBodyRegistry::AddRows(const UDataTable* Table, FString& OutError)
{
    if (Table->GetRowStruct() != FAstroBodyDataRow::StaticStruct())
    {
        OutError = FString::Printf(TEXT("%s does not use FAstroBodyDataRow."), *Table->GetName());
        return false;
    }

    bool bOk = true;
    Table->ForeachRow<FAstroBodyDataRow>(TEXT("FBodyRegistry"), [&](const FName& RowName, const FAstroBodyDataRow& Row)
    {
        if (!bOk)
        {
            return;
        }
        if (IndexByID.Contains(RowName))
        {
            OutError = FString::Printf(TEXT("Duplicate body ID '%s'."), *RowName.ToString());
            bOk = false;
            return;
        }
        FBodyDefinition& Body = Bodies.AddDefaulted_GetRef();
        Body.BodyID = RowName;
        IndexByID.Add(RowName, Bodies.Num() - 1);
        Rows.Add(Row);
    });
    return bOk;
}

bool FBodyRegistry::Resolve(FString& OutError)
{
    using namespace AstroConstants;

    // Pass 1: physical properties and parent links.
    for (int32 i = 0; i < Bodies.Num(); ++i)
    {
        FBodyDefinition& Body = Bodies[i];
        const FAstroBodyDataRow& Row = Rows[i];
        const FString ID = Body.BodyID.ToString();

        Body.DisplayName = Row.DisplayName.IsEmpty() ? FText::FromName(Body.BodyID) : Row.DisplayName;
        Body.BodyType = Row.BodyType;
        Body.Notes = Row.Notes;

        if (Row.GMKm3PerS2 <= 0.0 || Row.EquatorialRadiusKm <= 0.0)
        {
            OutError = FString::Printf(TEXT("%s: GM and radius must be positive."), *ID);
            return false;
        }
        Body.GM = Row.GMKm3PerS2 * 1e9;
        Body.MassKg = Body.GM / GravitationalConstant;
        Body.EquatorialRadiusMeters = Row.EquatorialRadiusKm * 1000.0;
        Body.PolarRadiusMeters = (Row.PolarRadiusKm > 0.0 ? Row.PolarRadiusKm : Row.EquatorialRadiusKm) * 1000.0;
        Body.LuminosityWatts = Row.LuminosityWatts;
        Body.RingInnerRadiusMeters = Row.RingInnerRadiusKm * 1000.0;
        Body.RingOuterRadiusMeters = Row.RingOuterRadiusKm * 1000.0;

        Body.EquatorFrameToEcliptic = EquatorFrameFromPole(Row.PoleRADeg, Row.PoleDecDeg);
        Body.PoleRADeg = Row.PoleRADeg;
        Body.PoleDecDeg = Row.PoleDecDeg;
        Body.PoleRARateDegPerCentury = Row.PoleRARateDegPerCentury;
        Body.PoleDecRateDegPerCentury = Row.PoleDecRateDegPerCentury;
        Body.PrimeMeridianAtEpochRad = Row.PrimeMeridianDeg * DegToRad;
        Body.RotationRateRadPerSec = Row.RotationRateDegPerDay * DegToRad / SecondsPerDay;

        if (Body.BodyType == EAstroBodyType::Star)
        {
            if (StarIndex != INDEX_NONE)
            {
                OutError = TEXT("Only one star is supported per solar system.");
                return false;
            }
            StarIndex = i;
            continue;
        }

        const int32* ParentIndex = IndexByID.Find(Row.ParentID);
        if (!ParentIndex)
        {
            OutError = FString::Printf(TEXT("%s: parent '%s' not found."), *ID, *Row.ParentID.ToString());
            return false;
        }
        Body.ParentIndex = *ParentIndex;

        const double SemiMajorAxis = Row.SemiMajorAxisKm > 0.0 ? Row.SemiMajorAxisKm * 1000.0 : Row.SemiMajorAxisAU * AstronomicalUnit;
        if (SemiMajorAxis <= 0.0 || Row.Eccentricity < 0.0 || Row.Eccentricity >= 1.0)
        {
            OutError = FString::Printf(TEXT("%s: needs a positive semi-major axis and 0 <= e < 1."), *ID);
            return false;
        }

        FKeplerElements& E = Body.Elements;
        E.SemiMajorAxis = SemiMajorAxis;
        E.Eccentricity = Row.Eccentricity;
        E.Inclination = Row.InclinationDeg * DegToRad;
        E.LongitudeOfAscendingNode = Row.LongAscendingNodeDeg * DegToRad;
        E.ArgumentOfPeriapsis = (Row.LongPeriapsisDeg - Row.LongAscendingNodeDeg) * DegToRad;
        E.MeanAnomalyAtEpoch = (Row.MeanLongitudeDeg - Row.LongPeriapsisDeg) * DegToRad;
        E.EpochSeconds = (Row.EpochJD - JulianDateJ2000) * SecondsPerDay;
    }

    if (StarIndex == INDEX_NONE)
    {
        OutError = TEXT("No star found in the planet table.");
        return false;
    }

    // Pass 2: frames that depend on the parent, child lists, and hierarchy checks.
    for (int32 i = 0; i < Bodies.Num(); ++i)
    {
        FBodyDefinition& Body = Bodies[i];
        if (Body.ParentIndex == INDEX_NONE)
        {
            continue;
        }
        const FBodyDefinition& Parent = Bodies[Body.ParentIndex];
        const bool bValidParent = (Body.BodyType == EAstroBodyType::Planet && Parent.BodyType == EAstroBodyType::Star)
                               || (Body.BodyType == EAstroBodyType::Moon && Parent.BodyType == EAstroBodyType::Planet);
        if (!bValidParent)
        {
            OutError = FString::Printf(TEXT("%s: planets must orbit the star and moons must orbit a planet."), *Body.BodyID.ToString());
            return false;
        }

        Body.OrbitFrameToEcliptic = Rows[i].ElementFrame == EAstroElementFrame::ParentEquator
            ? Parent.EquatorFrameToEcliptic
            : FAstroMatrix3d::Identity();
        Bodies[Body.ParentIndex].ChildIndices.Add(i);
    }

    Rows.Reset();
    return true;
}

void FBodyRegistry::LoadTerrain(const UDataTable* TerrainTable, const FString& ContentDir)
{
    if (!TerrainTable || TerrainTable->GetRowStruct() != FAstroTerrainRow::StaticStruct())
    {
        return;
    }
    for (FBodyDefinition& Body : Bodies)
    {
        if (const FAstroTerrainRow* Row = TerrainTable->FindRow<FAstroTerrainRow>(Body.BodyID, TEXT("FBodyRegistry::LoadTerrain"), false))
        {
            Body.Terrain = MakeShared<FBodyTerrain>();
            Body.Terrain->Initialize(*Row, Body.BodyID, Body.EquatorialRadiusMeters, Body.PolarRadiusMeters, ContentDir);
        }
    }
}

int32 FBodyRegistry::FindIndex(FName BodyID) const
{
    const int32* Index = IndexByID.Find(BodyID);
    return Index ? *Index : INDEX_NONE;
}

double FBodyRegistry::GetOrbitMu(int32 Index) const
{
    const FBodyDefinition& Body = Bodies[Index];
    if (Body.ParentIndex == INDEX_NONE)
    {
        return 0.0;
    }
    // A planet's elements describe its whole system's barycenter, so include its moons.
    double SystemGM = Body.GM;
    if (Body.BodyType == EAstroBodyType::Planet)
    {
        for (int32 Child : Body.ChildIndices)
        {
            SystemGM += Bodies[Child].GM;
        }
    }
    return Bodies[Body.ParentIndex].GM + SystemGM;
}

FOrbitalState FBodyRegistry::ComputeRelativeStateAt(int32 Index, double SimSeconds) const
{
    const FBodyDefinition& Body = Bodies[Index];
    if (Body.ParentIndex == INDEX_NONE)
    {
        return FOrbitalState();
    }
    const FOrbitalState Local = KeplerOrbit::StateAtTime(Body.Elements, GetOrbitMu(Index), SimSeconds);
    return FOrbitalState{ Body.OrbitFrameToEcliptic * Local.Position, Body.OrbitFrameToEcliptic * Local.Velocity };
}
