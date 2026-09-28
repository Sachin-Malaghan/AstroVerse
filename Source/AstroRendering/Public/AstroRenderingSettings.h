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

    // Lit close-range terrain (AAstroTerrainActor); inherits each body's maps.
    UPROPERTY(Config, EditAnywhere, Category = "Materials")
    TSoftObjectPtr<UMaterialInterface> TerrainMaterial;

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

    // Sun looks (astro.Sun.Look): 0 physical (real brightness: a dazzling white disc),
    // 1 natural (default: white as seen from space - the Sun is ~5800 K and only looks
    // yellow through an atmosphere - but held just under the white point so its surface
    // shows, with a soft white halo), 2 stylized (Solar System Scope: orange with a golden
    // halo). Disc and halo values are relative to the white point at the current exposure.
    UPROPERTY(Config, EditAnywhere, Category = "Photometry")
    double CinematicSunDisc = 1.9;

    UPROPERTY(Config, EditAnywhere, Category = "Photometry")
    double CinematicSunHalo = 0.9;

    UPROPERTY(Config, EditAnywhere, Category = "Photometry")
    double CinematicSunColorMix = 0.7; // stylized; natural keeps 0.15 (white)

    UPROPERTY(Config, EditAnywhere, Category = "Photometry")
    double CinematicSunDetail = 1.5;

    // Natural look: a white disc needs more contrast and less halo to show its surface.
    UPROPERTY(Config, EditAnywhere, Category = "Photometry")
    double NaturalSunDisc = 1.0;

    UPROPERTY(Config, EditAnywhere, Category = "Photometry")
    double NaturalSunDetail = 3.5;

    UPROPERTY(Config, EditAnywhere, Category = "Photometry")
    double NaturalSunHalo = 0.35;

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

    // "Enhanced" sky (astro.Sky.MilkyWay 1, the default): the photographed band is ~1% of
    // white - invisible at the exposure a sunlit planet needs - so a procedural band in
    // galactic coordinates (bulge, Great Rift, star clouds) is drawn behind the map's stars.
    // Only empty sky is affected; bodies, the Sun and exposure are untouched.
    UPROPERTY(Config, EditAnywhere, Category = "Exposure")
    double EnhancedBandStrength = 0.26;

    UPROPERTY(Config, EditAnywhere, Category = "Exposure")
    double EnhancedStarGain = 1.4;

    UPROPERTY(Config, EditAnywhere, Category = "Exposure")
    double EnhancedStarDisplayBrightness = 1.0;

    // --- Per-platform render budgets, applied at begin play. "cvar=value" entries.
    UPROPERTY(Config, EditAnywhere, Category = "Budgets")
    TArray<FString> DesktopCVars;

    UPROPERTY(Config, EditAnywhere, Category = "Budgets")
    TArray<FString> VRCVars;

    // Groundwork for the mobile port (Phase 12): not yet exercised on a device.
    UPROPERTY(Config, EditAnywhere, Category = "Budgets")
    TArray<FString> MobileCVars;

    // Frame-time targets used by astro.Perf.Sample / perf_pass.ps1.
    UPROPERTY(Config, EditAnywhere, Category = "Budgets")
    double DesktopTargetHz = 60.0;

    UPROPERTY(Config, EditAnywhere, Category = "Budgets")
    double VRTargetHz = 90.0;

    UPROPERTY(Config, EditAnywhere, Category = "Budgets")
    double MobileTargetHz = 30.0;
};
