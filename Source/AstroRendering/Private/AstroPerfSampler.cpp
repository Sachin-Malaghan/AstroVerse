// Frame-time sampling against the platform budget, for scripted performance passes.
// astro.Perf.Sample <seconds> [label] averages frame, game-thread, render-thread and GPU
// time and logs one "AstroPerf:" line (Tools/Validation/perf_pass.ps1 collects them).
// See CLAUDE.md Phase 12.
#include "AstroRenderingSubsystem.h"
#include "Containers/Ticker.h"
#include "DynamicRHI.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "RenderTimer.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroPerf, Log, All);

namespace
{
    struct FAstroPerfSample
    {
        FString Label;
        double Remaining = 0.0;
        int32 Frames = 0;
        double FrameMs = 0.0, GameMs = 0.0, RenderMs = 0.0, GpuMs = 0.0, WorstFrameMs = 0.0;
        double BudgetMs = 1000.0 / 60.0;
    };

    bool TickSample(float DeltaTime, TSharedRef<FAstroPerfSample> S)
    {
        const double Frame = FApp::GetDeltaTime() * 1000.0;
        S->Frames++;
        S->FrameMs += Frame;
        S->WorstFrameMs = FMath::Max(S->WorstFrameMs, Frame);
        S->GameMs += FPlatformTime::ToMilliseconds(GGameThreadTime);
        S->RenderMs += FPlatformTime::ToMilliseconds(GRenderThreadTime);
        S->GpuMs += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
        S->Remaining -= FApp::GetDeltaTime();
        if (S->Remaining > 0.0)
        {
            return true;
        }
        const double N = FMath::Max(1, S->Frames);
        const double Gpu = S->GpuMs / N, Game = S->GameMs / N, Render = S->RenderMs / N;
        const double Bound = FMath::Max3(Gpu, Game, Render);
        UE_LOG(LogAstroPerf, Display, TEXT("AstroPerf: %s | frame %.2f ms (worst %.2f) | game %.2f | render %.2f | gpu %.2f | budget %.2f | %s"),
            *S->Label, S->FrameMs / N, S->WorstFrameMs, Game, Render, Gpu, S->BudgetMs,
            Bound <= S->BudgetMs ? TEXT("WITHIN") : TEXT("OVER"));
        return false;
    }

    FAutoConsoleCommandWithWorldAndArgs GAstroCmdPerfSample(
        TEXT("astro.Perf.Sample"), TEXT("astro.Perf.Sample <seconds> [label] - log average frame/thread/GPU times against the active render budget"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            TSharedRef<FAstroPerfSample> S = MakeShared<FAstroPerfSample>();
            S->Remaining = Args.Num() > 0 ? FCString::Atod(*Args[0]) : 3.0;
            S->Label = Args.Num() > 1 ? Args[1] : TEXT("sample");
            if (const UAstroRenderingSubsystem* Rendering = UAstroRenderingSubsystem::Get(World))
            {
                S->BudgetMs = Rendering->GetFrameBudgetMs();
                S->Label += FString::Printf(TEXT(" [%s]"), *Rendering->GetBudgetName());
            }
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S](float Dt) { return TickSample(Dt, S); }));
        }));
}
