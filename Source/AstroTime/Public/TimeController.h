#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Time/SimClock.h"
#include "AstroNetworkTime.h"
#include "TimeController.generated.h"
// The single god-mode clock every other module reads from. See CLAUDE.md
// Phase 3. Nothing outside this class queries wall-clock time directly.
// Advances once at the start of every world tick, before any actor ticks, so
// every body in a frame sees the same simulated instant.

// (SimSeconds since J2000, SimDelta this frame)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSimTimeAdvanced, double /*SimSeconds*/, double /*SimDeltaSeconds*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTimeControlsChanged);

UCLASS()
class ASTROTIME_API UTimeController : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    static UTimeController* Get(const UObject* WorldContext);

    // Live mode: simulated time follows real UTC at 1x (network-corrected when online).
    // Any manual time change (pause, speed, rewind, jump) leaves Live mode.
    UFUNCTION(BlueprintCallable, Category = "Astro|Time")
    void GoLive();

    UFUNCTION(BlueprintPure, Category = "Astro|Time")
    bool IsLive() const { return bLive; }

    // Real UTC now (system clock + network offset). Only this controller reads the wall clock.
    FDateTime GetRealUtcNow() const { return NetworkTime.UtcNow(); }
    const FAstroNetworkTime& GetNetworkTime() const { return NetworkTime; }
    void RequestNetworkSync();

    UFUNCTION(BlueprintCallable, Category = "Astro|Time")
    void Play();

    UFUNCTION(BlueprintCallable, Category = "Astro|Time")
    void Pause();

    UFUNCTION(BlueprintCallable, Category = "Astro|Time")
    void TogglePause();

    UFUNCTION(BlueprintPure, Category = "Astro|Time")
    bool IsPaused() const { return Clock.IsPaused(); }

    // Simulated seconds per real second. Negative runs time backward.
    UFUNCTION(BlueprintCallable, Category = "Astro|Time")
    void SetTimeScale(double SecondsSimulatedPerRealSecond);

    UFUNCTION(BlueprintPure, Category = "Astro|Time")
    double GetTimeScale() const { return Clock.GetTimeScale(); }

    // Keeps the current speed but runs backward / forward.
    UFUNCTION(BlueprintCallable, Category = "Astro|Time")
    void Rewind();

    UFUNCTION(BlueprintCallable, Category = "Astro|Time")
    void PlayForward();

    UFUNCTION(BlueprintPure, Category = "Astro|Time")
    bool IsRewinding() const { return Clock.GetTimeScale() < 0.0; }

    // Steps |timescale| along the preset ladder (real time, 1 min/s, 1 h/s, 1 day/s, ...), keeping direction.
    UFUNCTION(BlueprintCallable, Category = "Astro|Time")
    void StepTimeScale(int32 Direction);

    UFUNCTION(BlueprintCallable, Category = "Astro|Time")
    void JumpToSimSeconds(double SimSecondsSinceJ2000);

    UFUNCTION(BlueprintCallable, Category = "Astro|Time")
    void JumpToDateTime(const FDateTime& UtcDateTime);

    // Seconds since J2000.0.
    UFUNCTION(BlueprintPure, Category = "Astro|Time")
    double GetSimulatedEpoch() const { return Clock.GetSimulatedSeconds(); }

    UFUNCTION(BlueprintPure, Category = "Astro|Time")
    FDateTime GetSimulatedDateTime() const { return SimSecondsToDateTime(Clock.GetSimulatedSeconds()); }

    static double DateTimeToSimSeconds(const FDateTime& UtcDateTime);
    static FDateTime SimSecondsToDateTime(double SimSeconds);
    static const TArray<double>& GetTimeScaleLadder();

    // Native subscribers (e.g. the solar-system simulation) advance off this.
    FOnSimTimeAdvanced OnSimTimeAdvanced;

    // Fires on play/pause/scale/jump changes, for the HUD.
    UPROPERTY(BlueprintAssignable, Category = "Astro|Time")
    FOnTimeControlsChanged OnTimeControlsChanged;

private:
    void HandleWorldTickStart(UWorld* World, ELevelTick TickType, float RealDeltaSeconds);

    void LeaveLive();

    FSimClock Clock;
    FAstroNetworkTime NetworkTime;
    bool bLive = false;
    double NextNetworkSyncRealSeconds = 0.0;
    FDelegateHandle TickStartHandle;
};
