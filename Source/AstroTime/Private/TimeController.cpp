// See CLAUDE.md Phase 3.
#include "TimeController.h"
#include "AstroTimeSettings.h"
#include "Engine/World.h"
#include "Math/AstroConstants.h"

namespace
{
    // J2000.0 = 2000-01-01 12:00:00 TT = 11:58:55.816 UTC. Leap seconds since
    // then (5 as of 2026) are ignored: sim time is treated as uniform.
    const FDateTime& J2000Utc()
    {
        static const FDateTime Epoch(2000, 1, 1, 11, 58, 55, 816);
        return Epoch;
    }
}

bool UTimeController::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UTimeController::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    const UAstroTimeSettings* Settings = GetDefault<UAstroTimeSettings>();
    // The one sanctioned wall-clock read: choosing the starting epoch.
    const FDateTime Start = Settings->bStartAtCurrentDate ? FDateTime::UtcNow() : Settings->StartDateUtc;
    Clock.SetSimulatedSeconds(DateTimeToSimSeconds(Start));
    Clock.SetTimeScale(Settings->StartTimeScale);
    Clock.SetPaused(Settings->bStartPaused);

    TickStartHandle = FWorldDelegates::OnWorldTickStart.AddUObject(this, &UTimeController::HandleWorldTickStart);
}

void UTimeController::Deinitialize()
{
    FWorldDelegates::OnWorldTickStart.Remove(TickStartHandle);
    Super::Deinitialize();
}

UTimeController* UTimeController::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UTimeController>() : nullptr;
}

void UTimeController::HandleWorldTickStart(UWorld* World, ELevelTick TickType, float RealDeltaSeconds)
{
    if (World != GetWorld() || !World->HasBegunPlay() || World->IsPaused())
    {
        return;
    }
    const double Before = Clock.GetSimulatedSeconds();
    Clock.Advance(RealDeltaSeconds);
    const double After = Clock.GetSimulatedSeconds();
    OnSimTimeAdvanced.Broadcast(After, After - Before);
}

void UTimeController::Play()
{
    Clock.SetPaused(false);
    OnTimeControlsChanged.Broadcast();
}

void UTimeController::Pause()
{
    Clock.SetPaused(true);
    OnTimeControlsChanged.Broadcast();
}

void UTimeController::TogglePause()
{
    IsPaused() ? Play() : Pause();
}

void UTimeController::SetTimeScale(double SecondsSimulatedPerRealSecond)
{
    const double Limit = GetDefault<UAstroTimeSettings>()->MaxAbsTimeScale;
    Clock.SetTimeScale(FMath::Clamp(SecondsSimulatedPerRealSecond, -Limit, Limit));
    OnTimeControlsChanged.Broadcast();
}

void UTimeController::Rewind()
{
    SetTimeScale(-FMath::Abs(Clock.GetTimeScale()));
}

void UTimeController::PlayForward()
{
    SetTimeScale(FMath::Abs(Clock.GetTimeScale()));
}

const TArray<double>& UTimeController::GetTimeScaleLadder()
{
    using namespace AstroConstants;
    static const TArray<double> Ladder = {
        1.0,                        // real time
        60.0,                       // 1 minute / s
        3600.0,                     // 1 hour / s
        SecondsPerDay,              // 1 day / s
        7.0 * SecondsPerDay,        // 1 week / s
        30.0 * SecondsPerDay,       // ~1 month / s
        SecondsPerJulianYear,       // 1 year / s
        10.0 * SecondsPerJulianYear,
        100.0 * SecondsPerJulianYear,
    };
    return Ladder;
}

void UTimeController::StepTimeScale(int32 Direction)
{
    const TArray<double>& Ladder = GetTimeScaleLadder();
    const double Current = FMath::Abs(Clock.GetTimeScale());
    const double Sign = Clock.GetTimeScale() < 0.0 ? -1.0 : 1.0;

    // Nearest rung at or below the current speed, then step from there.
    int32 Rung = 0;
    for (int32 i = 0; i < Ladder.Num(); ++i)
    {
        if (Ladder[i] <= Current * (1.0 + 1e-9))
        {
            Rung = i;
        }
    }
    Rung = FMath::Clamp(Rung + FMath::Sign(Direction), 0, Ladder.Num() - 1);
    SetTimeScale(Sign * Ladder[Rung]);
}

void UTimeController::JumpToSimSeconds(double SimSecondsSinceJ2000)
{
    const double Before = Clock.GetSimulatedSeconds();
    Clock.SetSimulatedSeconds(SimSecondsSinceJ2000);
    OnSimTimeAdvanced.Broadcast(SimSecondsSinceJ2000, SimSecondsSinceJ2000 - Before);
    OnTimeControlsChanged.Broadcast();
}

void UTimeController::JumpToDateTime(const FDateTime& UtcDateTime)
{
    JumpToSimSeconds(DateTimeToSimSeconds(UtcDateTime));
}

double UTimeController::DateTimeToSimSeconds(const FDateTime& UtcDateTime)
{
    return (UtcDateTime - J2000Utc()).GetTotalSeconds();
}

FDateTime UTimeController::SimSecondsToDateTime(double SimSeconds)
{
    // FDateTime spans years 1-9999; clamp so extreme rewinds still display something sane.
    const double MinSeconds = (FDateTime::MinValue() - J2000Utc()).GetTotalSeconds();
    const double MaxSeconds = (FDateTime::MaxValue() - J2000Utc()).GetTotalSeconds();
    return J2000Utc() + FTimespan::FromSeconds(FMath::Clamp(SimSeconds, MinSeconds, MaxSeconds));
}
