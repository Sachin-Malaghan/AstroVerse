#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AstroTimeSettings.generated.h"
// Project Settings > Game > Astro Time. See CLAUDE.md Phase 3.

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Astro Time"))
class ASTROTIME_API UAstroTimeSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    // Start the simulation at today's real date instead of StartDateUtc.
    UPROPERTY(Config, EditAnywhere, Category = "Start")
    bool bStartAtCurrentDate = true;

    UPROPERTY(Config, EditAnywhere, Category = "Start", meta = (EditCondition = "!bStartAtCurrentDate"))
    FDateTime StartDateUtc = FDateTime(2000, 1, 1, 12, 0, 0);

    UPROPERTY(Config, EditAnywhere, Category = "Start")
    double StartTimeScale = 1.0;

    UPROPERTY(Config, EditAnywhere, Category = "Start")
    bool bStartPaused = false;

    // Hard cap on |time scale| (sim seconds per real second). 1e10 is ~300 years/s.
    UPROPERTY(Config, EditAnywhere, Category = "Limits", meta = (ClampMin = "1"))
    double MaxAbsTimeScale = 1e10;
};
