#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AstroBodiesSettings.generated.h"
// Project Settings > Game > Astro Bodies. See CLAUDE.md Phase 2.

class UDataTable;
class AAstroBody;
class UMaterialInterface;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Astro Bodies"))
class ASTROBODIES_API UAstroBodiesSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UAstroBodiesSettings();

    // Imported Data Table assets (row struct FAstroBodyDataRow).
    UPROPERTY(Config, EditAnywhere, Category = "Data")
    TSoftObjectPtr<UDataTable> PlanetTable;

    UPROPERTY(Config, EditAnywhere, Category = "Data")
    TSoftObjectPtr<UDataTable> MoonTable;

    // Used when the table assets above are unset: CSV sources, relative to the project Content folder.
    UPROPERTY(Config, EditAnywhere, Category = "Data")
    FString CSVFallbackDirectory = TEXT("Bodies/DataTables");

    // Physics step at normal speeds (s). Grows in powers of two up to MaxStepSeconds at high time scales.
    UPROPERTY(Config, EditAnywhere, Category = "Simulation", meta = (ClampMin = "1"))
    double BaseStepSeconds = 3600.0;

    // Largest step that still resolves Mercury's orbit (~88 steps/orbit at 1 day).
    UPROPERTY(Config, EditAnywhere, Category = "Simulation", meta = (ClampMin = "1"))
    double MaxStepSeconds = 86400.0;

    // Physics steps budgeted per real second; the step size grows to stay within it.
    UPROPERTY(Config, EditAnywhere, Category = "Simulation", meta = (ClampMin = "60"))
    int32 StepsPerRealSecondBudget = 60000;

    // Beyond this many steps in one frame (or a date jump), planets move analytically for that frame.
    UPROPERTY(Config, EditAnywhere, Category = "Simulation", meta = (ClampMin = "1"))
    int32 MaxStepsPerFrame = 4000;

    // Bodies closer than this to the render origin draw at true offset; farther ones use scaled space.
    UPROPERTY(Config, EditAnywhere, Category = "Rendering", meta = (ClampMin = "1000"))
    double LinearRenderLimitMeters = 1.0e8;

    UPROPERTY(Config, EditAnywhere, Category = "Spawning")
    bool bSpawnBodiesOnBeginPlay = true;

    // Placeholder surface materials until the Phase 6 art pass. Unset = engine default.
    UPROPERTY(Config, EditAnywhere, Category = "Spawning")
    TSoftObjectPtr<UMaterialInterface> StarMaterial;

    UPROPERTY(Config, EditAnywhere, Category = "Spawning")
    TSoftObjectPtr<UMaterialInterface> PlanetMaterial;

    // Override per type with Blueprint subclasses once art exists.
    UPROPERTY(Config, EditAnywhere, Category = "Spawning")
    TSubclassOf<AAstroBody> StarClass;

    UPROPERTY(Config, EditAnywhere, Category = "Spawning")
    TSubclassOf<AAstroBody> PlanetClass;

    UPROPERTY(Config, EditAnywhere, Category = "Spawning")
    TSubclassOf<AAstroBody> MoonClass;
};
