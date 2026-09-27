#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AstroRenderingSettings.generated.h"
// Project Settings > Game > Astro Rendering. See CLAUDE.md Phase 6.

class UDataTable;
class UMaterialInterface;
class UStaticMesh;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Astro Rendering"))
class ASTRORENDERING_API UAstroRenderingSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UAstroRenderingSettings();

    UPROPERTY(Config, EditAnywhere, Category = "Data")
    TSoftObjectPtr<UDataTable> AppearanceTable;

    // Used when AppearanceTable is unset, relative to the project Content folder.
    UPROPERTY(Config, EditAnywhere, Category = "Data")
    FString AppearanceCSVFallback = TEXT("Rendering/DataTables/DT_Appearance.csv");

    // High-resolution sphere for bodies (Nanite). Unset = the engine's basic sphere.
    UPROPERTY(Config, EditAnywhere, Category = "Meshes")
    TSoftObjectPtr<UStaticMesh> BodySphereMesh;

    UPROPERTY(Config, EditAnywhere, Category = "Materials")
    TSoftObjectPtr<UMaterialInterface> AtmosphereMaterial;

    UPROPERTY(Config, EditAnywhere, Category = "Materials")
    TSoftObjectPtr<UMaterialInterface> CoronaMaterial;

    UPROPERTY(Config, EditAnywhere, Category = "Materials")
    TSoftObjectPtr<UMaterialInterface> StarFieldMaterial;

    // Fallback surface for bodies with no appearance row.
    UPROPERTY(Config, EditAnywhere, Category = "Materials")
    TSoftObjectPtr<UMaterialInterface> DefaultSurfaceMaterial;

    // --- Photometry.
    // Luminous efficacy of sunlight, lm/W: converts the star's radiant flux to lux.
    UPROPERTY(Config, EditAnywhere, Category = "Photometry")
    double LuminousEfficacy = 93.0;

    // The real photosphere is ~1.6e9 cd/m^2; capped to keep FP16 scene color finite.
    UPROPERTY(Config, EditAnywhere, Category = "Photometry")
    double SunDiskLuminanceCap = 5.0e7;

    UPROPERTY(Config, EditAnywhere, Category = "Photometry")
    double CoronaLuminanceFraction = 1.5e-5;

    // Exposure follows an incident-light meter reading of the sunlight at the camera:
    // EV100 = log2(E / 2.5) + compensation. "Sunny 16" at 1 AU is ~15.6.
    UPROPERTY(Config, EditAnywhere, Category = "Exposure")
    double ExposureCompensation = 0.0;

    UPROPERTY(Config, EditAnywhere, Category = "Exposure")
    double ExposureAdaptSpeed = 3.0;

    // Stars as they'd look to a dark-adapted eye regardless of exposure (cinematic),
    // in display-referred units. 0 = physically correct (invisible next to sunlit bodies).
    UPROPERTY(Config, EditAnywhere, Category = "Exposure")
    double StarDisplayBrightness = 0.35;

    // --- Per-platform render budgets, applied at begin play. "cvar=value" entries.
    UPROPERTY(Config, EditAnywhere, Category = "Budgets")
    TArray<FString> DesktopCVars;

    UPROPERTY(Config, EditAnywhere, Category = "Budgets")
    TArray<FString> VRCVars;
};
