#pragma once
#include "CoreMinimal.h"
// Human-readable astronomy units for the HUD. See CLAUDE.md Phase 11.

namespace AstroUIFormat
{
    ASTROUI_API FString Distance(double Meters);        // m, km, AU, ly
    ASTROUI_API FString Speed(double MetersPerSecond);  // m/s, km/s, multiples of c
    ASTROUI_API FString Duration(double Seconds);       // s .. years
    ASTROUI_API FString TimeScale(double SimSecondsPerSecond, bool bPaused); // "Paused", "1 day/s", "Rewind 1 year/s"
    ASTROUI_API FString Mass(double Kg);                // Earth masses (and kg)
    ASTROUI_API FString Number(double Value, int32 Decimals = 0); // thousands separators
}
