#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BodyDataRow.generated.h"
// Row schema for DT_Planets / DT_Moons (Content/Bodies/DataTables). Every
// number that describes a real body lives in these tables, never in C++.
// See CLAUDE.md Phase 2.

UENUM(BlueprintType)
enum class EAstroBodyType : uint8
{
    Star,
    Planet,
    Moon
};

// Which plane the orbital elements' inclination/node are measured against.
UENUM(BlueprintType)
enum class EAstroElementFrame : uint8
{
    Ecliptic,      // J2000 ecliptic and equinox
    ParentEquator  // the parent body's equator (node measured from the parent's ICRF-equator node)
};

USTRUCT(BlueprintType)
struct ASTROBODIES_API FAstroBodyDataRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EAstroBodyType BodyType = EAstroBodyType::Planet;

    // Row name of the body this one orbits; None for the Sun.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FName ParentID;

    // Standard gravitational parameter G*M (km^3/s^2) — measured far more
    // precisely than G or M separately. Mass is derived as GM / G.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical")
    double GMKm3PerS2 = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical")
    double EquatorialRadiusKm = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical")
    double PolarRadiusKm = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical")
    double LuminosityWatts = 0.0;

    // --- Orbit (mean elements at EpochJD). Use AU or km for the semi-major axis, not both.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit")
    EAstroElementFrame ElementFrame = EAstroElementFrame::Ecliptic;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit")
    double SemiMajorAxisAU = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit")
    double SemiMajorAxisKm = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit")
    double Eccentricity = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit")
    double InclinationDeg = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit")
    double LongAscendingNodeDeg = 0.0;

    // Longitude of periapsis, varpi = node + argument of periapsis.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit")
    double LongPeriapsisDeg = 0.0;

    // Mean longitude, L = varpi + mean anomaly.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit")
    double MeanLongitudeDeg = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orbit")
    double EpochJD = 2451545.0;

    // --- Rotation (IAU WGCCRE model, constant terms at J2000).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotation")
    double PoleRADeg = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotation")
    double PoleDecDeg = 90.0;

    // Secular pole motion (IAU WGCCRE T terms, deg per Julian century). Earth's precession
    // (-0.641, -0.557) moves its pole ~0.15 deg between 2000 and 2026: enough to put the
    // Sun 0.2 deg off in a site's sky if ignored.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotation")
    double PoleRARateDegPerCentury = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotation")
    double PoleDecRateDegPerCentury = 0.0;

    // Prime meridian angle W at J2000.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotation")
    double PrimeMeridianDeg = 0.0;

    // dW/dt; negative is retrograde (Venus, Uranus).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotation")
    double RotationRateDegPerDay = 0.0;

    // --- Rings (0 = none).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rings")
    double RingInnerRadiusKm = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rings")
    double RingOuterRadiusKm = 0.0;

    // Provenance / known approximations for this row.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString Notes;
};
