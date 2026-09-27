#pragma once
#include "CoreMinimal.h"
#include "Time/SimClock.h"
// The single god-mode clock every other module reads from. See CLAUDE.md
// Phase 3. Nothing outside this class queries wall-clock time directly.

class ASTROTIME_API UTimeController
{
public:
    void Play();
    void Pause();
    void SetTimeScale(double SecondsSimulatedPerRealSecond);
    double GetSimulatedEpoch() const { return Clock.GetSimulatedSeconds(); }

private:
    FSimClock Clock;
};
