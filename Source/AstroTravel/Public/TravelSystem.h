#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TravelSystem.generated.h"
// Point-to-point transit between bodies. Decided 2026-09-27 (see CLAUDE.md "Decided —
// travel"): two user-selectable styles, and whether the god-mode clock runs during
// transit is a user setting too. See CLAUDE.md Phase 9.

UENUM(BlueprintType)
enum class EAstroTravelStyle : uint8
{
    // Fixed camera flight with a warp effect; the player just watches.
    CinematicWarp,
    // The player flies a ship through the warp: throttle sets the pace, the stick steers in the tunnel.
    PilotedShip
};

UENUM(BlueprintType)
enum class EAstroClockDuringTravel : uint8
{
    // Planets are exactly where you left them when you arrive.
    Pause,
    // Time keeps passing at the current time scale.
    KeepRunning
};

// Per-user travel preferences (saved to GameUserSettings.ini).
UCLASS(Config = GameUserSettings)
class ASTROTRAVEL_API UAstroTravelSettings : public UObject
{
    GENERATED_BODY()

public:
    static UAstroTravelSettings* Get() { return GetMutableDefault<UAstroTravelSettings>(); }

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Astro|Travel")
    EAstroTravelStyle Style = EAstroTravelStyle::CinematicWarp;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Astro|Travel")
    EAstroClockDuringTravel Clock = EAstroClockDuringTravel::Pause;

    // Arrival distance from the destination, in its radii (the Sun uses SunArrivalRadii).
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Astro|Travel")
    double ArrivalRadii = 4.0;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Astro|Travel")
    double SunArrivalRadii = 30.0;

    // Duration = Base + PerDecade * log10(start distance / arrival distance), clamped.
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Astro|Travel")
    double BaseSeconds = 3.0;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Astro|Travel")
    double SecondsPerDecade = 2.2;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Astro|Travel")
    double MaxSeconds = 18.0;

    UFUNCTION(BlueprintCallable, Category = "Astro|Travel")
    void Save() { SaveConfig(); }
};
