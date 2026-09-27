// See CLAUDE.md Phase 8.
#include "AstroFlycamPawn.h"
#include "Camera/CameraComponent.h"

AAstroFlycamPawn::AAstroFlycamPawn()
{
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(RootComponent);
    Camera->SetFieldOfView(70.0f);
    Camera->bUsePawnControlRotation = false; // the pawn itself carries full 6DOF orientation
}
