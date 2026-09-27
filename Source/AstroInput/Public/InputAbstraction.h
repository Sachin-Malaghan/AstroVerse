#pragma once
#include "CoreMinimal.h"
// Abstracts VR motion controllers vs. desktop mouse/keyboard behind one
// interface so gameplay logic never branches on platform. See CLAUDE.md Phase 8.

class ASTROINPUT_API FInputAbstraction
{
public:
    virtual FVector GetMoveAxis() const = 0;
    virtual bool IsSelectPressed() const = 0;
    virtual ~FInputAbstraction() = default;
};
