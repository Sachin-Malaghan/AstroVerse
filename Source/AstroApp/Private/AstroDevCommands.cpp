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

    // Rodrigues rotation of V about unit Axis.
    FAstroVector3d Rotate(const FAstroVector3d& V, const FAstroVector3d& Axis, double Angle)
    {
        const double C = FMath::Cos(Angle), S = FMath::Sin(Angle);
        return V * C + Axis.Cross(V) * S + Axis * (Axis.Dot(V) * (1.0 - C));
    }

    // astro.Camera.Frame Saturn 6 40 20 -- camera 6 radii from Saturn, 40 deg phase angle
    // (Sun-body-camera), 20 deg above the ecliptic plane through the body; aimed at the body.
    FAutoConsoleCommandWithWorldAndArgs GAstroCmdCameraFrame(
        TEXT("astro.Camera.Frame"), TEXT("astro.Camera.Frame <BodyID> <distance_in_radii> [phase_deg=45] [elevation_deg=10]"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(World);
            ACameraActor* Camera = ViewCamera(World);
            const int32 Body = Sim && Args.Num() >= 2 ? Sim->FindBodyIndex(FName(*Args[0])) : INDEX_NONE;
            if (!Camera || Body == INDEX_NONE)
            {
                return;
            }
            const double Radii = FCString::Atod(*Args[1]);
            const double Phase = FMath::DegreesToRadians(Args.Num() > 2 ? FCString::Atod(*Args[2]) : 45.0);
            const double Elevation = FMath::DegreesToRadians(Args.Num() > 3 ? FCString::Atod(*Args[3]) : 10.0);

            const FBodyRegistry& Registry = Sim->GetRegistry();
            const FAstroVector3d BodyPos = Sim->GetSimulation().GetBodyState(Body).Position;
            FAstroVector3d ToSun = (Sim->GetSimulation().GetBodyState(Registry.GetStarIndex()).Position - BodyPos).Normalized();
            const FAstroVector3d Up(0.0, 0.0, 1.0);
            if (ToSun.Length() < 0.5)
            {
                ToSun = FAstroVector3d(1.0, 0.0, 0.0); // framing the star itself
            }
            FAstroVector3d Dir = Rotate(ToSun, Up, Phase);
            const FAstroVector3d Side = Dir.Cross(Up).Normalized();
            Dir = Rotate(Dir, Side, -Elevation).Normalized();

            const double Distance = Radii * Registry.Get(Body).EquatorialRadiusMeters;
            Sim->SetRenderOriginAnchor(Body, Dir * Distance);
            Camera->SetActorLocation(FVector::ZeroVector);
            // Aim along the true direction (direction is preserved by scaled space).
            Camera->SetActorRotation(UAstroSimulationSubsystem::SimToEngineDirection(-Dir).Rotation());
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
