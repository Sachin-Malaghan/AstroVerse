// Developer commands for scripted validation runs (screenshots, camera aim). See CLAUDE.md Phase 4.
#include "HAL/IConsoleManager.h"
#include "AstroSimulationSubsystem.h"
#include "AstroPawnBase.h"
#include "AstroPlayerController.h"
#include "AstroUISubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
    // The possessed pawn, or a level camera when nothing is possessed.
    AActor* ViewActor(UWorld* World)
    {
        APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
        if (!PC)
        {
            return nullptr;
        }
        return PC->GetPawn() ? static_cast<AActor*>(PC->GetPawn()) : PC->GetViewTarget();
    }

    UCameraComponent* ViewCameraComponent(UWorld* World)
    {
        AActor* Actor = ViewActor(World);
        return Actor ? Actor->FindComponentByClass<UCameraComponent>() : nullptr;
    }

    // Places the view actor at the engine origin with a rotation, stopping any pawn motion.
    void PlaceView(AActor* Actor, const FRotator& Rotation)
    {
        Actor->SetActorLocationAndRotation(FVector::ZeroVector, Rotation);
        if (AAstroPawnBase* Pawn = Cast<AAstroPawnBase>(Actor))
        {
            Pawn->StopMotion();
        }
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
            AActor* View = ViewActor(World);
            const int32 Body = Sim && Args.Num() > 0 ? Sim->FindBodyIndex(FName(*Args[0])) : INDEX_NONE;
            if (!View || Body == INDEX_NONE)
            {
                return;
            }
            const FVector Target = Sim->SimToScaledEnginePosition(Sim->GetSimulation().GetBodyState(Body).Position);
            View->SetActorRotation((Target - View->GetActorLocation()).Rotation());
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
            AActor* View = ViewActor(World);
            const int32 Body = Sim && Args.Num() >= 2 ? Sim->FindBodyIndex(FName(*Args[0])) : INDEX_NONE;
            if (!View || Body == INDEX_NONE)
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
            // Aim along the true direction (direction is preserved by scaled space).
            PlaceView(View, Sim->SimToEngineDirection(-Dir).Rotation());
        }));

    // Engine axes in a landed frame: +X east, -Y north, +Z up. Yaw 0 = east, -90 = north.
    FAutoConsoleCommandWithWorldAndArgs GAstroCmdCameraLook(
        TEXT("astro.Camera.Look"), TEXT("astro.Camera.Look <yaw_deg> <pitch_deg> - aim the view camera in engine axes"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            AActor* View = ViewActor(World);
            if (View && Args.Num() >= 2)
            {
                PlaceView(View, FRotator(FCString::Atod(*Args[1]), FCString::Atod(*Args[0]), 0.0));
            }
        }));

    // astro.Origin.LandSun Moon 8 2 -- stand where the Sun is 8 deg above the horizon, 2 m up,
    // facing across the light so relief and shadows read well.
    FAutoConsoleCommandWithWorldAndArgs GAstroCmdOriginLandSun(
        TEXT("astro.Origin.LandSun"), TEXT("astro.Origin.LandSun <BodyID> <sun_elevation_deg> [altitude_m=2] [view_pitch_deg=-8]"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(World);
            AActor* View = ViewActor(World);
            const int32 Body = Sim && Args.Num() >= 2 ? Sim->FindBodyIndex(FName(*Args[0])) : INDEX_NONE;
            if (!View || Body == INDEX_NONE)
            {
                return;
            }
            const double Elevation = FMath::DegreesToRadians(FCString::Atod(*Args[1]));
            const double Altitude = Args.Num() > 2 ? FCString::Atod(*Args[2]) : 2.0;
            const double Pitch = Args.Num() > 3 ? FCString::Atod(*Args[3]) : -8.0;
            const FBodyDefinition& Def = Sim->GetRegistry().Get(Body);
            const FAstroMatrix3d SimToBody = Def.GetOrientationAt(Sim->GetSimulation().GetSimSeconds()).Transposed();
            const FAstroVector3d SunBF = (SimToBody * (Sim->GetSimulation().GetBodyState(Sim->GetRegistry().GetStarIndex()).Position
                                                      - Sim->GetSimulation().GetBodyState(Body).Position)).Normalized();
            // Walk from the subsolar point toward a pole by (90 - elevation) degrees.
            FAstroVector3d Axis = SunBF.Cross(FAstroVector3d(0, 0, 1)).Normalized();
            if (Axis.LengthSquared() < 0.5)
            {
                Axis = FAstroVector3d(1, 0, 0);
            }
            const double Angle = UE_HALF_PI - Elevation;
            const FAstroVector3d Dir = (SunBF * FMath::Cos(Angle) + Axis.Cross(SunBF) * FMath::Sin(Angle)).Normalized();
            const double Ground = Def.Terrain.IsValid() ? Def.Terrain->EllipsoidRadius(Dir) + Def.Terrain->HeightAt(Dir, 0.5) : Def.EquatorialRadiusMeters;
            Sim->SetRenderOriginBodyFixed(Body, Dir * (Ground + Altitude));
            // Face 90 deg from the Sun's azimuth: side light.
            const FVector SunEngine = Sim->SimToEngineDirection(Def.GetOrientationAt(Sim->GetSimulation().GetSimSeconds()) * SunBF);
            const double SunYaw = FMath::RadiansToDegrees(FMath::Atan2(SunEngine.Y, SunEngine.X));
            PlaceView(View, FRotator(Pitch, SunYaw + 90.0, 0.0));
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdPawnStatus(
        TEXT("astro.Pawn.Status"), TEXT("Print the pawn's locomotion, reference body, frame, altitude and speed"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            const AAstroPawnBase* Pawn = Cast<AAstroPawnBase>(ViewActor(World));
            const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(World);
            if (Pawn && Sim)
            {
                UE_LOG(LogTemp, Display, TEXT("Pawn: %s near %s, %s frame, altitude %.2f m, speed %.2f m/s"),
                    Pawn->GetLocomotion() == EAstroLocomotion::Walking ? TEXT("walking") : TEXT("flying"),
                    *Pawn->GetReferenceBody().ToString(), Sim->IsRotatingFrame() ? TEXT("co-rotating") : TEXT("inertial"),
                    Pawn->GetAltitude(), Pawn->GetSpeedMetersPerSecond());
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdPawnLand(
        TEXT("astro.Pawn.Land"), TEXT("Land the pawn (walk) if low over a solid surface, or take off"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            if (AAstroPawnBase* Pawn = Cast<AAstroPawnBase>(ViewActor(World)))
            {
                Pawn->ToggleLanding();
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdCameraLookDown(
        TEXT("astro.Camera.LookDown"), TEXT("Aim the view camera straight down the ecliptic pole"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            if (AActor* View = ViewActor(World))
            {
                PlaceView(View, FRotator(-90.0, 0.0, 0.0));
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdCameraFOV(
        TEXT("astro.Camera.FOV"), TEXT("astro.Camera.FOV <degrees>"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UCameraComponent* Camera = ViewCameraComponent(World);
            if (Camera && Args.Num() > 0)
            {
                Camera->SetFieldOfView(FCString::Atof(*Args[0]));
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdSelect(
        TEXT("astro.Select"), TEXT("astro.Select <BodyID> - select a body as if clicked (HUD info card)"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            AAstroPlayerController* PC = World ? Cast<AAstroPlayerController>(World->GetFirstPlayerController()) : nullptr;
            if (PC && Args.Num() > 0)
            {
                PC->SelectBody(FName(*Args[0]));
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdUIToggle(
        TEXT("astro.UI.Toggle"), TEXT("astro.UI.Toggle HUD|Help|Menu"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UAstroUISubsystem* UI = World ? UAstroUISubsystem::Get(World->GetFirstPlayerController()) : nullptr;
            if (!UI || Args.Num() == 0)
            {
                return;
            }
            if (Args[0] == TEXT("Help")) { UI->ToggleHelp(); }
            else if (Args[0] == TEXT("Menu")) { UI->ToggleMenu(); }
            else { UI->ToggleHUD(); }
        }));
}
