// See CLAUDE.md Phase 8.
#include "AstroGameMode.h"
#include "AstroFlycamPawn.h"
#include "AstroPlayerController.h"
#include "AstroVRPawn.h"
#include "Engine/Engine.h"
#include "StereoRendering.h"

AAstroGameMode::AAstroGameMode()
{
    PlayerControllerClass = AAstroPlayerController::StaticClass();
    DefaultPawnClass = AAstroFlycamPawn::StaticClass();
}

UClass* AAstroGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    const bool bVR = GEngine && GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabled();
    return bVR ? AAstroVRPawn::StaticClass() : AAstroFlycamPawn::StaticClass();
}
