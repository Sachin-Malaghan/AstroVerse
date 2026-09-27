#pragma once
#include "CoreMinimal.h"
#include "AstroPawnBase.h"
#include "AstroFlycamPawn.generated.h"
// Desktop spectator-style flycam pawn. See CLAUDE.md Phase 8.
// Mouse look / WASD / Space-C / Q-E roll / wheel speed / Shift boost / G land.

class UCameraComponent;

UCLASS()
class ASTROAPP_API AAstroFlycamPawn : public AAstroPawnBase
{
    GENERATED_BODY()

public:
    AAstroFlycamPawn();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astro|Pawn")
    TObjectPtr<UCameraComponent> Camera;
};
