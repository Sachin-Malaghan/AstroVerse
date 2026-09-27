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
    // Local up at the pawn (engine space) and height above the terrain there.
    bool SampleGround(const UAstroSimulationSubsystem* Sim, FVector& OutUp, double& OutHeightAboveGround, double& OutGravity) const;
    double SphereOfInfluence(const UAstroSimulationSubsystem* Sim, int32 Body) const;
    bool bFacedInitialBody = false;
};
