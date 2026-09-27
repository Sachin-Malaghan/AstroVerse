// See CLAUDE.md Phase 8.
#include "AstroVRPawn.h"
#include "AstroInputActions.h"
#include "AstroPlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "InputActionValue.h"
#include "MotionControllerComponent.h"
#include "UObject/ConstructorHelpers.h"

AAstroVRPawn::AAstroVRPawn()
{
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(RootComponent);
    Camera->bLockToHmd = true;

    LeftController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftController"));
    LeftController->SetupAttachment(RootComponent);
    LeftController->SetTrackingMotionSource(TEXT("Left"));
    RightController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightController"));
    RightController->SetupAttachment(RootComponent);
    RightController->SetTrackingMotionSource(TEXT("Right"));

    TeleportMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TeleportMarker"));
    TeleportMarker->SetupAttachment(RootComponent);
    TeleportMarker->SetUsingAbsoluteLocation(true);
    TeleportMarker->SetUsingAbsoluteRotation(true);
    TeleportMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TeleportMarker->SetWorldScale3D(FVector(0.6, 0.6, 0.02));
    TeleportMarker->SetVisibility(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (Cylinder.Succeeded())
    {
        TeleportMarker->SetStaticMesh(Cylinder.Object);
    }

    // The actor sits on the floor; the HMD supplies eye height.
    EyeHeight = 0.0;
}

void AAstroVRPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    const AAstroPlayerController* PC = Cast<AAstroPlayerController>(GetController());
    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    const UAstroInputActions* A = PC ? PC->GetInputActions() : nullptr;
    if (EIC && A)
    {
        EIC->BindAction(A->SnapTurn, ETriggerEvent::Triggered, this, &AAstroVRPawn::OnSnapTurn);
        EIC->BindAction(A->SnapTurn, ETriggerEvent::Completed, this, &AAstroVRPawn::OnSnapTurn);
        EIC->BindAction(A->Teleport, ETriggerEvent::Started, this, &AAstroVRPawn::OnTeleportStarted);
        EIC->BindAction(A->Teleport, ETriggerEvent::Completed, this, &AAstroVRPawn::OnTeleportReleased);
    }
}

FQuat AAstroVRPawn::GetMovementBasis() const
{
    // Fly where you look; walk relative to the head's heading.
    return Camera->GetComponentQuat();
}

void AAstroVRPawn::ApplyLookInput(float DeltaSeconds)
{
    // Head tracking is the look input in VR; discard mouse deltas.
    Input.ConsumeLook();
}

void AAstroVRPawn::OnSnapTurn(const FInputActionValue& Value)
{
    const float X = Value.Get<float>();
    if (FMath::Abs(X) < 0.3f)
    {
        bSnapArmed = true;
        return;
    }
    if (!bSnapArmed)
    {
        return;
    }
    bSnapArmed = false;
    // Turn about the head, not the tracking origin, so the view pivots in place.
    const FVector Pivot = Camera->GetComponentLocation();
    const FQuat Turn(GetActorUpVector(), FMath::DegreesToRadians(FMath::Sign(X) * SnapTurnDegrees));
    const FVector NewLocation = Pivot + Turn.RotateVector(GetActorLocation() - Pivot);
    SetActorLocationAndRotation(NewLocation, Turn * GetActorQuat());
    WalkYaw += FMath::Sign(X) * SnapTurnDegrees;
}

bool AAstroVRPawn::TraceTeleport(FVector& OutLocation) const
{
    // Ballistic arc from the right controller, stepped until it meets terrain collision.
    const FVector Up = GetActorUpVector();
    FVector P = RightController->GetComponentLocation();
    FVector V = RightController->GetForwardVector() * 900.0; // cm/s
    const FVector G = -Up * 980.0;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(AstroTeleport), false, this);
    for (int32 Step = 0; Step < 60; ++Step)
    {
        const FVector Next = P + V * 0.05 + G * 0.00125;
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, P, Next, ECC_Visibility, Params))
        {
            OutLocation = Hit.ImpactPoint;
            return FVector::DotProduct(Hit.ImpactNormal, Up) > 0.7; // not too steep
        }
        V += G * 0.05;
        P = Next;
    }
    return false;
}

void AAstroVRPawn::OnTeleportStarted(const FInputActionValue& Value)
{
    bAimingTeleport = Locomotion == EAstroLocomotion::Walking;
}

void AAstroVRPawn::OnTeleportReleased(const FInputActionValue& Value)
{
    FVector Target;
    if (bAimingTeleport && TraceTeleport(Target))
    {
        // Put the player's feet (the tracking origin under the head) at the target.
        const FVector HeadOffset = Camera->GetComponentLocation() - GetActorLocation();
        const FVector HorizontalOffset = HeadOffset - GetActorUpVector() * FVector::DotProduct(HeadOffset, GetActorUpVector());
        SetActorLocation(Target - HorizontalOffset);
    }
    bAimingTeleport = false;
    TeleportMarker->SetVisibility(false);
}

void AAstroVRPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bAimingTeleport)
    {
        FVector Target;
        const bool bValid = TraceTeleport(Target);
        TeleportMarker->SetVisibility(bValid);
        if (bValid)
        {
            TeleportMarker->SetWorldLocationAndRotation(Target, FRotationMatrix::MakeFromZ(GetActorUpVector()).ToQuat());
        }
    }

    // Comfort vignette scales with how fast the world moves past you.
    const float Moving = FMath::Clamp(static_cast<float>(Velocity.Size() / 300.0), 0.0f, 1.0f);
    Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
    Camera->PostProcessSettings.VignetteIntensity = 0.2f + ComfortVignette * Moving;
    Camera->PostProcessBlendWeight = 1.0f;
}
