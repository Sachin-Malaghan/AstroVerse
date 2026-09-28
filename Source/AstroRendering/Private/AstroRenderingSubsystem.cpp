// See CLAUDE.md Phase 6.
#include "AstroRenderingSubsystem.h"
#include "AstroBody.h"
#include "AstroRenderingSettings.h"
#include "AstroSimulationSubsystem.h"
#include "AstroSpaceEnvironment.h"
#include "AstroBeltActor.h"
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

static TAutoConsoleVariable<FString> CVarAstroRenderBudget(
    TEXT("astro.Render.Budget"), TEXT("auto"),
    TEXT("Render budget: auto (device), desktop, vr, mobile. Applied at begin play / astro.Render.ApplyBudget."));

static FAutoConsoleCommandWithWorld GAstroCmdApplyBudget(
    TEXT("astro.Render.ApplyBudget"), TEXT("Re-apply the render budget (after changing astro.Render.Budget)"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (UAstroRenderingSubsystem* Rendering = UAstroRenderingSubsystem::Get(World))
        {
            Rendering->ApplyRenderBudget();
        }
    }));

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
    FActorSpawnParameters BeltParams;
    BeltParams.Name = TEXT("AstroBelts");
    GetWorld()->SpawnActor<AAstroBeltActor>(BeltParams);
    FActorSpawnParameters TerrainParams;
    TerrainParams.Name = TEXT("AstroTerrain");
    Terrain = GetWorld()->SpawnActor<AAstroTerrainActor>(TerrainParams);

    ApplyRenderBudget();
    bDecorated = true;
    UE_LOG(LogAstroRendering, Log, TEXT("Decorated %d bodies (%d appearance rows); %s render budget."),
        Sim->GetRegistry().Num(), Appearance.Num(), *GetBudgetName());
    return true;
}

FString UAstroRenderingSubsystem::GetBudgetName() const
{
    return Budget == EAstroRenderBudget::VR ? TEXT("VR") : Budget == EAstroRenderBudget::Mobile ? TEXT("mobile") : TEXT("desktop");
}

double UAstroRenderingSubsystem::GetFrameBudgetMs() const
{
    const UAstroRenderingSettings* Settings = GetDefault<UAstroRenderingSettings>();
    const double Hz = Budget == EAstroRenderBudget::VR ? Settings->VRTargetHz
                    : Budget == EAstroRenderBudget::Mobile ? Settings->MobileTargetHz
                    : Settings->DesktopTargetHz;
    return 1000.0 / FMath::Max(1.0, Hz);
}

void UAstroRenderingSubsystem::ApplyRenderBudget()
{
    const FString Forced = CVarAstroRenderBudget.GetValueOnGameThread();
    if (Forced.Equals(TEXT("desktop"), ESearchCase::IgnoreCase)) { Budget = EAstroRenderBudget::Desktop; }
    else if (Forced.Equals(TEXT("vr"), ESearchCase::IgnoreCase)) { Budget = EAstroRenderBudget::VR; }
    else if (Forced.Equals(TEXT("mobile"), ESearchCase::IgnoreCase)) { Budget = EAstroRenderBudget::Mobile; }
    else
    {
        const bool bStereo = GEngine && GEngine->XRSystem.IsValid() && GEngine->StereoRenderingDevice.IsValid()
                          && GEngine->StereoRenderingDevice->IsStereoEnabled();
        Budget = bStereo ? EAstroRenderBudget::VR
               : (PLATFORM_ANDROID || PLATFORM_IOS) ? EAstroRenderBudget::Mobile
               : EAstroRenderBudget::Desktop;
    }

    const UAstroRenderingSettings* Settings = GetDefault<UAstroRenderingSettings>();
    const TArray<FString>& Entries = Budget == EAstroRenderBudget::VR ? Settings->VRCVars
                                   : Budget == EAstroRenderBudget::Mobile ? Settings->MobileCVars
                                   : Settings->DesktopCVars;
    for (const FString& Entry : Entries)
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
