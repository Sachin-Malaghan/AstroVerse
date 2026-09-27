#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputAbstraction.h"
#include "Math/AstroVector3d.h"
#include "AstroPawnBase.generated.h"
// Shared locomotion for the desktop and VR pawns. See CLAUDE.md Phase 8.
// - Reference body: the deepest body whose sphere of influence contains the pawn; the
//   floating origin rides with it so a planet doesn't race away at 30 km/s.
// - Frames: inertial near/around bodies, co-rotating (body-fixed, +Z = up) close to a
//   surface. Switching keeps position, and rotates orientation and velocity with the basis.
// - Flying: speed scales with altitude above the reference body (walking pace at the
//   ground, AU/s between planets). Walking: kinematic, on FBodyTerrain, with real gravity.
// - Orbiting: the camera circles a focused body and looks at it; the wheel zooms on a log
//   scale from interplanetary distance down to tens of metres above the real terrain. Close
//   in, the orbit co-rotates with the body so you stay over the same ground.
// - Home: an always-available reset to a known view (Earth, sunlit side), from anywhere.

class UAstroFloatingOriginComponent;
class UAstroSimulationSubsystem;
class UInputAction;
class FGalaxyView;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EAstroLocomotion : uint8
{
    Flying,
    Walking
};

UCLASS(Abstract)
class ASTROAPP_API AAstroPawnBase : public APawn
{
    GENERATED_BODY()

public:
    AAstroPawnBase();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Pawn")
    TObjectPtr<UAstroFloatingOriginComponent> FloatingOrigin;

    UFUNCTION(BlueprintPure, Category = "Astro|Pawn")
    EAstroLocomotion GetLocomotion() const { return Locomotion; }

    UFUNCTION(BlueprintPure, Category = "Astro|Pawn")
    FName GetReferenceBody() const;

    // Height above the reference body's terrain (m).
    UFUNCTION(BlueprintPure, Category = "Astro|Pawn")
    double GetAltitude() const { return Altitude; }

    UFUNCTION(BlueprintPure, Category = "Astro|Pawn")
    double GetSpeedMetersPerSecond() const { return Velocity.Size() / 100.0; }

    // Land if low over a solid surface; take off if walking.
    UFUNCTION(BlueprintCallable, Category = "Astro|Pawn")
    void ToggleLanding();

    // Zeroes velocity and returns to flight (dev camera placement, travel arrival).
    void StopMotion() { Velocity = FVector::ZeroVector; Locomotion = EAstroLocomotion::Flying; }

    // Orbit camera around a body. DistanceRadii <= 0 keeps the current distance when close
    // (otherwise flies in to 3.5 radii). bInstant snaps instead of flying in.
    void FocusOn(FName BodyID, double DistanceRadii = 0.0, bool bInstant = false);
    // Orbit around a body from a direction given as phase (Sun-body-camera) and elevation angles.
    void FocusOnFromSunSide(FName BodyID, double DistanceRadii, double PhaseDeg, double ElevationDeg, bool bInstant);
    void StopOrbiting() { bOrbiting = false; }
    bool IsOrbiting() const { return bOrbiting; }
    FName GetOrbitBody() const;
    // Slow automatic orbit (deg/s about the body's pole), for the guided tour. 0 = off.
    void SetOrbitDrift(double DegPerSecond) { OrbitDriftDegPerSecond = DegPerSecond; }
    // Reset: back to the solar system and the home view. Works from the galaxy and mid-flight.
    void GoHome();
    // Turn smoothly to look at a body (U = the Sun). Leaves orbit; walking keeps you upright.
    // Any mouse look cancels the turn.
    void FaceBody(FName BodyID);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astro|Pawn")
    FName HomeBody = TEXT("Earth");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astro|Pawn")
    double HomeDistanceRadii = 3.5;

    // Moves the pawn to a sim position (and orientation) through the current frame.
    void TeleportToSim(const FAstroVector3d& SimPosition, const FQuat& EngineRotation);

    // Flight speed per meter of altitude (1/s); the wheel scales it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astro|Pawn")
    double SpeedPerAltitude = 0.5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astro|Pawn")
    double WalkSpeed = 1.4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astro|Pawn")
    double RunSpeed = 5.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astro|Pawn")
    double EyeHeight = 1.7;

    // Below this altitude the frame co-rotates with the body (m, or 0.5 R for small bodies).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astro|Pawn")
    double BodyFixedFrameAltitude = 100000.0;

protected:
    // Direction the pawn flies / walks relative to (desktop: the pawn; VR: the head).
    virtual FQuat GetMovementBasis() const { return GetActorQuat(); }
    // Applies look/roll input (desktop) — VR pawns ignore mouse look.
    virtual void ApplyLookInput(float DeltaSeconds);
    // VR: the headset supplies pitch/roll, so the actor itself stays upright when walking.
    virtual bool UsesHeadTracking() const { return false; }

    void OnMove(const FInputActionValue& Value);
    void OnMoveCompleted(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);
    void OnRoll(const FInputActionValue& Value);
    void OnRollCompleted(const FInputActionValue& Value);
    void OnSpeedStep(const FInputActionValue& Value);
    void OnBoost(const FInputActionValue& Value);
    void OnBoostCompleted(const FInputActionValue& Value);
    void OnJump(const FInputActionValue& Value);
    void OnLand(const FInputActionValue& Value);

    FAstroInputState Input;
    EAstroLocomotion Locomotion = EAstroLocomotion::Flying;
    FVector Velocity = FVector::ZeroVector; // engine cm/s, current frame axes
    double SpeedMultiplier = 1.0;
    double Altitude = 0.0;
    int32 ReferenceBody = INDEX_NONE;
    bool bJumpRequested = false;

    // Walking: yaw/pitch relative to the local up, so the view stays upright.
    double WalkYaw = 0.0;
    double WalkPitch = 0.0;

private:
    void UpdateReferenceFrame(UAstroSimulationSubsystem* Sim);
    void SwitchFrame(UAstroSimulationSubsystem* Sim, TFunctionRef<void()> Change);
    void TickFlying(UAstroSimulationSubsystem* Sim, float DeltaSeconds);
    void TickWalking(UAstroSimulationSubsystem* Sim, float DeltaSeconds);
    void TickGalaxyFlight(const FGalaxyView& Galaxy, float DeltaSeconds);
    void TickOrbit(UAstroSimulationSubsystem* Sim, float DeltaSeconds);
    // Radius of the body's surface (terrain or cloud tops) under a sim-frame direction.
    double SurfaceRadius(const UAstroSimulationSubsystem* Sim, int32 Body, const FAstroVector3d& Dir) const;
    UFUNCTION() void HandleTravelArrived(FName BodyID);
    UFUNCTION() void HandleTravelStarted(FName BodyID);
    void HandleTourCamera(FName BodyID, double DistanceRadii, double PhaseDeg, double ElevationDeg, double DriftDegPerSecond, bool bUseSide);
    // Local up at the pawn (engine space) and height above the terrain there.
    bool SampleGround(const UAstroSimulationSubsystem* Sim, FVector& OutUp, double& OutHeightAboveGround, double& OutGravity) const;
    double SphereOfInfluence(const UAstroSimulationSubsystem* Sim, int32 Body) const;
    bool bFacedInitialBody = false;

    // Orbit camera state (sim frame). Dir points from the body centre to the camera.
    bool bOrbiting = false;
    bool bPendingHome = false;
    int32 OrbitBody = INDEX_NONE;
    FAstroVector3d OrbitDir, OrbitTargetDir;
    double OrbitLogAltitude = 0.0, OrbitTargetLogAltitude = 0.0; // ln(metres above the surface)
    double OrbitLastSimSeconds = 0.0;
    double OrbitDriftDegPerSecond = 0.0;
    FQuat OrbitView = FQuat::Identity;

    // FaceBody state.
    int32 FaceTarget = INDEX_NONE;
    double FaceSeconds = 0.0;
    bool ApplyFaceTarget(UAstroSimulationSubsystem* Sim, float DeltaSeconds);
};
