#pragma once
// Foundation-layer simulated time base. AstroTime::UTimeController (Simulation
// layer) wraps this with the user-facing god-mode API (pause/rewind/timescale).
// Every module reads simulated time from here — never wall-clock time.
// See CLAUDE.md Phase 1 and Phase 3.

#ifndef ASTROCORE_API
#define ASTROCORE_API
#endif

class ASTROCORE_API FSimClock
{
public:
    double GetSimulatedSeconds() const { return SimulatedSeconds; }
    double GetTimeScale() const { return TimeScale; }
    void Advance(double RealDeltaSeconds);
    void SetTimeScale(double NewScale) { TimeScale = NewScale; }
    void SetPaused(bool bInPaused) { bPaused = bInPaused; }
    void SetSimulatedSeconds(double NewSimSeconds) { SimulatedSeconds = NewSimSeconds; }
    bool IsPaused() const { return bPaused; }

private:
    double SimulatedSeconds = 0.0;
    double TimeScale = 1.0;
    bool bPaused = false;
};
