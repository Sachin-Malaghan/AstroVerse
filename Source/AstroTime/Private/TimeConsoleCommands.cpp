// Developer console commands for the god-mode clock. See CLAUDE.md Phase 3.
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "TimeController.h"

namespace
{
    UTimeController* TimeFor(UWorld* World)
    {
        return World ? World->GetSubsystem<UTimeController>() : nullptr;
    }

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdTimeScale(
        TEXT("astro.Time.Scale"), TEXT("astro.Time.Scale <sim seconds per real second> (negative rewinds)"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UTimeController* Time = TimeFor(World);
            if (Time && Args.Num() > 0)
            {
                Time->SetTimeScale(FCString::Atod(*Args[0]));
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdTimePause(
        TEXT("astro.Time.Pause"), TEXT("Pause the simulation clock"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            if (UTimeController* Time = TimeFor(World))
            {
                Time->Pause();
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdTimePlay(
        TEXT("astro.Time.Play"), TEXT("Resume the simulation clock"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            if (UTimeController* Time = TimeFor(World))
            {
                Time->Play();
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdTimeJump(
        TEXT("astro.Time.JumpTo"), TEXT("astro.Time.JumpTo <YYYY-MM-DD[THH:MM:SS]> (UTC)"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            FDateTime Date;
            UTimeController* Time = TimeFor(World);
            if (Time && Args.Num() > 0 && (FDateTime::ParseIso8601(*Args[0], Date) || FDateTime::Parse(Args[0], Date)))
            {
                Time->JumpToDateTime(Date);
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdTimeStatus(
        TEXT("astro.Time.Status"), TEXT("Print the simulated date and time scale"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            if (const UTimeController* Time = TimeFor(World))
            {
                UE_LOG(LogTemp, Display, TEXT("Sim date %s UTC, scale %.6g s/s, %s"),
                    *Time->GetSimulatedDateTime().ToIso8601(), Time->GetTimeScale(), Time->IsPaused() ? TEXT("paused") : TEXT("running"));
            }
        }));
}
