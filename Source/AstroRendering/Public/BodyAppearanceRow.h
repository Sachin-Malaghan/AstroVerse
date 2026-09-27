#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BodyAppearanceRow.generated.h"
// Row schema for DT_Appearance (Content/Rendering/DataTables): how each body is drawn.
// Row names match DT_Planets / DT_Moons. Atmosphere scattering values are physically
// motivated but tuned for the look; they live in data so they can be adjusted without
// a recompile. See CLAUDE.md Phase 6.

class UMaterialInterface;

USTRUCT(BlueprintType)
struct ASTRORENDERING_API FAstroBodyAppearanceRow : public FTableRowBase
{
    GENERATED_BODY()

    // Material instance for the body's surface (parent M_PlanetSurface or M_SunSurface).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
    TSoftObjectPtr<UMaterialInterface> SurfaceMaterial;

    // Cloud layer drift relative to the surface, degrees of longitude per day.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
    double CloudDriftDegPerDay = 0.0;

    // --- Atmosphere shell (0 height = none).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atmosphere")
    double AtmosphereHeightKm = 0.0;

    // Rayleigh scattering coefficients at sea level, per megameter (1e-6 / m).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atmosphere")
    FVector RayleighPerMm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atmosphere")
    double RayleighScaleHeightKm = 8.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atmosphere")
    double MiePerMm = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atmosphere")
    double MieScaleHeightKm = 1.2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atmosphere")
    double MieAnisotropy = 0.76;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atmosphere")
    FLinearColor MieTint = FLinearColor::White;

    // --- Rings (radii come from DT_Planets).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rings")
    TSoftObjectPtr<UMaterialInterface> RingMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString Notes;
};
