// See CLAUDE.md Phase 6.
#include "AstroRenderingSubsystem.h"
#include "AstroBody.h"
#include "AstroRenderingSettings.h"
#include "AstroSimulationSubsystem.h"
#include "AstroSpaceEnvironment.h"
#include "AstroTerrainActor.h"
#include "BodyShadingComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "IXRTrackingSystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "StereoRendering.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroRendering, Log, All);

bool UAstroRenderingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

UAstroRenderingSubsystem* UAstroRenderingSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UAstroRenderingSubsystem>() : nullptr;
}

TStatId UAstroRenderingSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UAstroRenderingSubsystem, STATGROUP_Tickables);
}

void UAstroRenderingSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bDecorated)
    {
        TryDecorate();
    }
}

void UAstroRenderingSubsystem::LoadAppearance()
{
    const UAstroRenderingSettings* Settings = GetDefault<UAstroRenderingSettings>();
    const UDataTable* Table = Settings->AppearanceTable.LoadSynchronous();
    if (!Table)
    {
        // Dev fallback: build a transient table from the CSV source.
        FString CSV;
        const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), Settings->AppearanceCSVFallback);
        if (FFileHelper::LoadFileToString(CSV, *Path))
        {
            UDataTable* Transient = NewObject<UDataTable>(GetTransientPackage());
            Transient->RowStruct = FAstroBodyAppearanceRow::StaticStruct();
            const TArray<FString> Problems = Transient->CreateTableFromCSVString(CSV);
            for (const FString& Problem : Problems)
            {
                UE_LOG(LogAstroRendering, Warning, TEXT("DT_Appearance: %s"), *Problem);
            }
            Table = Transient;
        }
    }
    if (!Table)
    {
        UE_LOG(LogAstroRendering, Warning, TEXT("No appearance table; bodies use the default surface."));
        return;
    }
    Table->ForeachRow<FAstroBodyAppearanceRow>(TEXT("Appearance"), [this](const FName& Row, const FAstroBodyAppearanceRow& Value)
    {
        Appearance.Add(Row, Value);
    });
}

bool UAstroRenderingSubsystem::TryDecorate()
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    // Wait until the simulation has loaded and spawned its bodies.
    if (!Sim || !Sim->IsReady() || !Sim->GetBodyActor(0))
    {
        return false;
    }

    LoadAppearance();
    for (int32 i = 0; i < Sim->GetRegistry().Num(); ++i)
    {
        AAstroBody* Body = Sim->GetBodyActor(i);
        if (!Body || Body->FindComponentByClass<UBodyShadingComponent>())
        {
            continue;
        }
        UBodyShadingComponent* Shading = NewObject<UBodyShadingComponent>(Body, TEXT("Shading"));
        Shading->Configure(Appearance.Find(Sim->GetRegistry().Get(i).BodyID));
        Shading->RegisterComponent();
    }

    FActorSpawnParameters Params;
    Params.Name = TEXT("AstroSpaceEnvironment");
    Environment = GetWorld()->SpawnActor<AAstroSpaceEnvironment>(Params);
    FActorSpawnParameters TerrainParams;
    TerrainParams.Name = TEXT("AstroTerrain");
    Terrain = GetWorld()->SpawnActor<AAstroTerrainActor>(TerrainParams);

    ApplyRenderBudget();
    bDecorated = true;
    UE_LOG(LogAstroRendering, Log, TEXT("Decorated %d bodies (%d appearance rows); %s render budget."),
        Sim->GetRegistry().Num(), Appearance.Num(), bVRBudget ? TEXT("VR") : TEXT("desktop"));
    return true;
}

void UAstroRenderingSubsystem::ApplyRenderBudget()
{
    bVRBudget = GEngine && GEngine->XRSystem.IsValid() && GEngine->StereoRenderingDevice.IsValid()
             && GEngine->StereoRenderingDevice->IsStereoEnabled();

    const UAstroRenderingSettings* Settings = GetDefault<UAstroRenderingSettings>();
    for (const FString& Entry : bVRBudget ? Settings->VRCVars : Settings->DesktopCVars)
    {
        FString Name, Value;
        if (!Entry.Split(TEXT("="), &Name, &Value))
        {
            continue;
        }
        Name.TrimStartAndEndInline();
        Value.TrimStartAndEndInline();
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name))
        {
            // Code priority: project-setting defaults would otherwise win over the budget.
            CVar->Set(*Value, ECVF_SetByCode);
        }
        else
        {
            UE_LOG(LogAstroRendering, Warning, TEXT("Render budget: unknown cvar '%s'"), *Name);
        }
    }
}
