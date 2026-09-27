#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AstroGameMode.generated.h"
// Thin wiring only — almost no logic should live here. See CLAUDE.md Phase 8.
// Picks the VR pawn when a headset is active, the desktop flycam otherwise.

UCLASS()
class ASTROAPP_API AAstroGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AAstroGameMode();
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
};
