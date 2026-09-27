// Developer commands for scripted validation runs (screenshots, camera aim). See CLAUDE.md Phase 4.
#include "HAL/IConsoleManager.h"
#include "AstroSimulationSubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
    ACameraActor* ViewCamera(UWorld* World)
    {
        APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
        return PC ? Cast<ACameraActor>(PC->GetViewTarget()) : nullptr;
    }

    // e.g. astro.Debug.DelayedExec 5 HighResShot 1920x1080 -- lets -ExecCmds script a timed run.
    FAutoConsoleCommandWithWorldAndArgs GAstroCmdDelayedExec(
        TEXT("astro.Debug.DelayedExec"), TEXT("astro.Debug.DelayedExec <seconds> <command...>"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            if (Args.Num() < 2)
            {
                return;
            }
            const float Delay = FCString::Atof(*Args[0]);
            FString Command;
            for (int32 i = 1; i < Args.Num(); ++i)
            {
                Command += (i > 1 ? TEXT(" ") : TEXT("")) + Args[i];
            }
            TWeakObjectPtr<UWorld> WeakWorld = World;
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld, Command](float)
            {
                // Through the player controller so viewport commands (HighResShot) are handled too.
                UWorld* TargetWorld = WeakWorld.Get();
                APlayerController* PC = TargetWorld ? TargetWorld->GetFirstPlayerController() : nullptr;
                if (PC)
                {
                    PC->ConsoleCommand(Command);
                }
                else if (GEngine)
                {
                    GEngine->Exec(TargetWorld, *Command);
                }
                return false;
            }), Delay);
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdCameraLookAt(
        TEXT("astro.Camera.LookAt"), TEXT("astro.Camera.LookAt <BodyID> - aim the view camera at a body"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(World);
            ACameraActor* Camera = ViewCamera(World);
            const int32 Body = Sim && Args.Num() > 0 ? Sim->FindBodyIndex(FName(*Args[0])) : INDEX_NONE;
            if (!Camera || Body == INDEX_NONE)
            {
                return;
            }
            const FVector Target = Sim->SimToScaledEnginePosition(Sim->GetSimulation().GetBodyState(Body).Position);
            Camera->SetActorRotation((Target - Camera->GetActorLocation()).Rotation());
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdCameraLookDown(
        TEXT("astro.Camera.LookDown"), TEXT("Aim the view camera straight down the ecliptic pole"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            if (ACameraActor* Camera = ViewCamera(World))
            {
                Camera->SetActorRotation(FRotator(-90.0, 0.0, 0.0));
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdCameraFOV(
        TEXT("astro.Camera.FOV"), TEXT("astro.Camera.FOV <degrees>"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            ACameraActor* Camera = ViewCamera(World);
            if (Camera && Args.Num() > 0)
            {
                Camera->GetCameraComponent()->SetFieldOfView(FCString::Atof(*Args[0]));
            }
        }));
}
