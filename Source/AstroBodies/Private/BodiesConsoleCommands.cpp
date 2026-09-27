// Developer console commands for the floating origin and body inspection. See CLAUDE.md Phase 2 / Phase 7.
#include "HAL/IConsoleManager.h"
#include "AstroGeodesy.h"
#include "BodyTerrain.h"
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

    // astro.Origin.Land Earth 28.5 -80.6 2  -- stand 2 m above Cape Canaveral in Earth's rotating frame.
    FAutoConsoleCommandWithWorldAndArgs GAstroCmdOriginLand(
        TEXT("astro.Origin.Land"), TEXT("astro.Origin.Land <BodyID> <lat_deg> <east_lon_deg> [altitude_m=2] - body-fixed origin, engine +Z = local up"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(World);
            const int32 Body = Sim && Args.Num() >= 3 ? Sim->FindBodyIndex(FName(*Args[0])) : INDEX_NONE;
            if (Body == INDEX_NONE)
            {
                return;
            }
            const double Lat = FCString::Atod(*Args[1]) * AstroConstants::DegToRad;
            const double Lon = FCString::Atod(*Args[2]) * AstroConstants::DegToRad;
            const double Altitude = Args.Num() > 3 ? FCString::Atod(*Args[3]) : 2.0;
            const FBodyDefinition& Def = Sim->GetRegistry().Get(Body);
            // Latitude in the body's map convention (geodetic on Earth, as GPS gives it), then
            // the real ground there, then up along the radial.
            const bool bGeodetic = Def.Terrain.IsValid() && Def.Terrain->UsesGeodeticLatitude();
            const FAstroVector3d Dir = AstroGeodesy::SurfaceDirection(Lat * AstroConstants::RadToDeg, Lon * AstroConstants::RadToDeg,
                Def.EquatorialRadiusMeters, Def.PolarRadiusMeters, bGeodetic);
            const double a = Def.EquatorialRadiusMeters, c = Def.PolarRadiusMeters;
            double R = 1.0 / FMath::Sqrt((Dir.X * Dir.X + Dir.Y * Dir.Y) / (a * a) + Dir.Z * Dir.Z / (c * c));
            if (Def.Terrain.IsValid())
            {
                R += Def.Terrain->HeightAt(Dir, 0.5);
            }
            Sim->SetRenderOriginBodyFixed(Body, Dir * (R + Altitude));
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
