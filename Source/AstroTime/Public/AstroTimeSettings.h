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
    // Start in Live mode: simulated time locked to real UTC at 1x (overrides the start date,
    // scale and pause settings above).
    UPROPERTY(Config, EditAnywhere, Category = "Live")
    bool bStartLive = true;

    // Correct the system clock against network time when online (an HTTP HEAD request to
    // each URL in turn; only the response's Date header is used).
    UPROPERTY(Config, EditAnywhere, Category = "Live")
    bool bUseNetworkTime = true;

    UPROPERTY(Config, EditAnywhere, Category = "Live")
    TArray<FString> NetworkTimeUrls = { TEXT("https://www.cloudflare.com"), TEXT("https://www.google.com"), TEXT("https://www.microsoft.com") };

    UPROPERTY(Config, EditAnywhere, Category = "Live", meta = (ClampMin = "1"))
    double NetworkResyncMinutes = 15.0;

    UPROPERTY(Config, EditAnywhere, Category = "Limits", meta = (ClampMin = "1"))
    double MaxAbsTimeScale = 1e10;
};
