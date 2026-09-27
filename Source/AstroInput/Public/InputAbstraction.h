#pragma once
#include "CoreMinimal.h"
// Abstracts VR motion controllers vs. desktop mouse/keyboard behind one
// interface so gameplay logic never branches on platform. See CLAUDE.md Phase 8.

class FInputAbstraction
{
public:
    virtual FVector GetMoveAxis() const = 0;
    virtual bool IsSelectPressed() const = 0;
    virtual ~FInputAbstraction() = default;
};

// Header-only (no export macro): every user compiles its own copy.
// Latest values of the movement actions, written by Enhanced Input bindings and read
// by pawn movement each tick. Look and roll are per-frame deltas, consumed on read.
class FAstroInputState : public FInputAbstraction
{
public:
    virtual FVector GetMoveAxis() const override { return MoveAxis; }
    virtual bool IsSelectPressed() const override { return bSelect; }

    FVector2D ConsumeLook() { const FVector2D V = LookAccum; LookAccum = FVector2D::ZeroVector; return V; }
    float ConsumeSpeedSteps() { const float V = SpeedSteps; SpeedSteps = 0.0f; return V; }

    FVector MoveAxis = FVector::ZeroVector;
    FVector2D LookAccum = FVector2D::ZeroVector;
    float RollAxis = 0.0f;
    float SpeedSteps = 0.0f;
    bool bBoost = false;
    bool bSelect = false;
};
