#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TourStopRow.h"
#include "AstroTourSubsystem.generated.h"
// Guided tour: Sun -> planets -> a look back at the whole Solar System -> the Milky Way with
// the Sun marked. Plays DT_Tour stops in order: warps with the cinematic travel style, asks
// the pawn to orbit each stop (OnCameraRequest — the pawn owns the camera), and shows the
// narration on the HUD. Start with F2, astro.Tour.Start, or -AstroTour on the command line
// (classroom / kiosk). See CLAUDE.md "Guided tour".

// Body, distance (radii), phase (deg), elevation (deg), drift (deg/s), bUseSide (else keep direction).
DECLARE_MULTICAST_DELEGATE_SixParams(FOnAstroTourCameraRequest, FName, double, double, double, double, bool);

UCLASS()
class ASTROUI_API UAstroTourSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    static UAstroTourSubsystem* Get(const UObject* WorldContext);

    void Start();
    void Stop();
    void Next();
    void Toggle() { bRunning ? Stop() : Start(); }
    bool IsRunning() const { return bRunning; }
    int32 GetStopCount() const { return Stops.Num(); }

    FOnAstroTourCameraRequest OnCameraRequest;

private:
    enum class EStep : uint8 { Enter, Travelling, Arrived, Holding, Galaxy };

    void LoadStops();
    void EnterStop();
    void ShowNarration();
    void Finish();

    TArray<FAstroTourStopRow> Stops;
    int32 Index = 0;
    EStep Step = EStep::Enter;
    double StepSeconds = 0.0;
    bool bRunning = false;
    bool bAutoStartPending = false;
    double AutoStartDelay = 0.0;
};
