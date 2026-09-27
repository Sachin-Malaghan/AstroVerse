// Developer console commands for the floating origin and body inspection. See CLAUDE.md Phase 2 / Phase 7.
#include "HAL/IConsoleManager.h"
#include "AstroSimulationSubsystem.h"
#include "Engine/World.h"
#include "Math/AstroConstants.h"

namespace
{
    FAutoConsoleCommandWithWorldAndArgs GAstroCmdOriginAnchor(
        TEXT("astro.Origin.AnchorTo"), TEXT("astro.Origin.AnchorTo <BodyID> [x_km y_km z_km] - render origin rides with a body at an ecliptic offset"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(World);
            const int32 Body = Sim && Args.Num() > 0 ? Sim->FindBodyIndex(FName(*Args[0])) : INDEX_NONE;
            if (Body == INDEX_NONE)
            {
                return;
            }
            FAstroVector3d Offset;
            if (Args.Num() >= 4)
            {
                Offset = FAstroVector3d(FCString::Atod(*Args[1]), FCString::Atod(*Args[2]), FCString::Atod(*Args[3])) * 1000.0;
            }
            Sim->SetRenderOriginAnchor(Body, Offset);
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdOriginSet(
        TEXT("astro.Origin.Set"), TEXT("astro.Origin.Set <x_AU> <y_AU> <z_AU> - fixed barycentric render origin"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(World);
            if (Sim && Args.Num() >= 3)
            {
                const FAstroVector3d AU(FCString::Atod(*Args[0]), FCString::Atod(*Args[1]), FCString::Atod(*Args[2]));
                Sim->SetRenderOrigin(AU * AstroConstants::AstronomicalUnit);
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdBodiesList(
        TEXT("astro.Bodies.List"), TEXT("Print every body's barycentric position and distance from the render origin"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(World);
            if (!Sim || !Sim->IsReady())
            {
                return;
            }
            const FAstroVector3d Origin = Sim->GetRenderOrigin();
            const double AU = AstroConstants::AstronomicalUnit;
            for (int32 i = 0; i < Sim->GetRegistry().Num(); ++i)
            {
                const FAstroVector3d& P = Sim->GetSimulation().GetBodyState(i).Position;
                UE_LOG(LogTemp, Display, TEXT("%-10s (%+.6f, %+.6f, %+.6f) AU   %.0f km from origin"),
                    *Sim->GetRegistry().Get(i).BodyID.ToString(), P.X / AU, P.Y / AU, P.Z / AU, (P - Origin).Length() / 1000.0);
            }
        }));
}
