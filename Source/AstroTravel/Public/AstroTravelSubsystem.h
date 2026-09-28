#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Math/AstroVector3d.h"
#include "TravelSystem.h"
#include "AstroTravelSubsystem.generated.h"
// Runs a transit to a body. The path is expressed relative to the (moving) destination and
// interpolated in log-distance, so a trip from Neptune to Earth spends its time sensibly
// instead of crawling the last stretch; direction swings onto the sunlit side for arrival.
// While travelling the floating origin rides the path and pawns stand down (IsTravelling).
// See CLAUDE.md Phase 9.

class AAstroWarpEffects;
class UTimeController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAstroTravelEvent, FName, BodyID);
// After the viewer has been placed for this frame of a transit (e.g. a ship riding along).
DECLARE_MULTICAST_DELEGATE(FOnAstroTravelPathApplied);

UCLASS()
class ASTROTRAVEL_API UAstroTravelSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    static UAstroTravelSubsystem* Get(const UObject* WorldContext);

    // Starts a transit to BodyID with the user's chosen style (or always the cinematic warp,
    // e.g. for the guided tour). False if already travelling or unknown.
    UFUNCTION(BlueprintCallable, Category = "Astro|Travel")
    bool BeginTravel(FName BodyID, bool bForceCinematic = false);
    // Same, with an explicit style (missions always fly the real-time path).
    bool BeginTravelWithStyle(FName BodyID, EAstroTravelStyle Style);

    FOnAstroTravelPathApplied OnPathApplied;

    // Arrives immediately (skip).
    UFUNCTION(BlueprintCallable, Category = "Astro|Travel")
    void FinishNow();

    UFUNCTION(BlueprintPure, Category = "Astro|Travel")
    bool IsTravelling() const { return bTravelling; }

    // 0..1 along the path.
    UFUNCTION(BlueprintPure, Category = "Astro|Travel")
    double GetProgress() const { return Progress; }

    UFUNCTION(BlueprintPure, Category = "Astro|Travel")
    FName GetDestination() const { return DestinationID; }

    UFUNCTION(BlueprintPure, Category = "Astro|Travel")
    EAstroTravelStyle GetActiveStyle() const { return ActiveStyle; }

    // Piloted ship: throttle (-1..1 scales the pace) and steering (tunnel offset), fed by the pawn.
    void SetPilotInput(float Throttle, const FVector2D& Steer);

    // Warp strength 0..1 (peaks mid-transit), for effects and comfort vignettes.
    double GetWarpIntensity() const;

    // Current speed along the path (m/s) and distance still to go (m), for the HUD.
    double GetSpeedMetersPerSecond() const { return SpeedMps; }
    double GetRemainingMeters() const { return RemainingMeters; }

    UPROPERTY(BlueprintAssignable, Category = "Astro|Travel")
    FOnAstroTravelEvent OnTravelStarted;

    UPROPERTY(BlueprintAssignable, Category = "Astro|Travel")
    FOnAstroTravelEvent OnTravelArrived;

private:
    void Arrive();
    void ApplyPathPoint(double S);
    void ApplyRealFlight(class UAstroSimulationSubsystem* Sim, AActor* View, double S);

    bool bTravelling = false;
    EAstroTravelStyle ActiveStyle = EAstroTravelStyle::CinematicWarp;
    FName DestinationID;
    int32 Destination = INDEX_NONE;
    FAstroVector3d StartOffset;   // pawn - destination at departure (m)
    FAstroVector3d ArrivalOffset; // pawn - destination at arrival (m)
    FAstroVector3d StartAbsolute; // real flight: departure point (sim frame, fixed)
    FAstroVector3d LastPosition;
    bool bHasLastPosition = false;
    double SpeedMps = 0.0;
    double RemainingMeters = 0.0;
    double Duration = 5.0;
    double Elapsed = 0.0;
    double Progress = 0.0;
    FQuat StartRotation = FQuat::Identity;
    float BaseFOV = 70.0f;

    bool bPausedClockForTravel = false;
    TOptional<EAstroTravelStyle> StyleOverride;
    float PilotThrottle = 0.0f;
    FVector2D PilotSteer = FVector2D::ZeroVector;
    FVector2D TunnelOffset = FVector2D::ZeroVector;

    TWeakObjectPtr<AAstroWarpEffects> Effects;
};
