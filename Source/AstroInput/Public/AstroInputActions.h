#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AstroInputActions.generated.h"
// Every AstroVerse input action and its key mappings, built at runtime (no binary
// input assets to keep in sync). Desktop (mouse/keyboard + gamepad) and VR (OpenXR
// motion controllers) contexts share the same actions, so gameplay code binds once
// and never branches on platform. See CLAUDE.md Phase 8.

class UInputAction;
class UInputMappingContext;

UCLASS()
class ASTROINPUT_API UAstroInputActions : public UObject
{
    GENERATED_BODY()

public:
    static UAstroInputActions* Create(UObject* Outer);

    // --- Movement (pawn)
    UPROPERTY() TObjectPtr<UInputAction> Move;        // Axis3D: X forward, Y right, Z up
    UPROPERTY() TObjectPtr<UInputAction> Look;        // Axis2D: X yaw, Y pitch (up = +)
    UPROPERTY() TObjectPtr<UInputAction> Roll;        // Axis1D: + = clockwise
    UPROPERTY() TObjectPtr<UInputAction> SpeedStep;   // Axis1D: wheel notches
    UPROPERTY() TObjectPtr<UInputAction> Boost;       // bool
    UPROPERTY() TObjectPtr<UInputAction> Jump;        // bool (walking)
    UPROPERTY() TObjectPtr<UInputAction> Land;        // bool: land / take off
    UPROPERTY() TObjectPtr<UInputAction> SnapTurn;    // Axis1D (VR)
    UPROPERTY() TObjectPtr<UInputAction> Teleport;    // bool (VR): hold to aim, release to go

    // --- Global (player controller)
    UPROPERTY() TObjectPtr<UInputAction> Select;      // bool: pick / lock the body under the reticle
    UPROPERTY() TObjectPtr<UInputAction> TimeFaster;
    UPROPERTY() TObjectPtr<UInputAction> TimeSlower;
    UPROPERTY() TObjectPtr<UInputAction> TimePause;
    UPROPERTY() TObjectPtr<UInputAction> TimeRewind;
    UPROPERTY() TObjectPtr<UInputAction> Travel;      // travel to the selected body (Phase 9)
    UPROPERTY() TObjectPtr<UInputAction> ToggleGalaxy;
    UPROPERTY() TObjectPtr<UInputAction> ToggleHUD;
    UPROPERTY() TObjectPtr<UInputAction> Menu;
    UPROPERTY() TObjectPtr<UInputAction> Help;
    // Camera: orbit/zoom the selected body (F), return to the home view (Home / Backspace).
    UPROPERTY() TObjectPtr<UInputAction> Focus;
    UPROPERTY() TObjectPtr<UInputAction> Home;
    // Lock the clock to real UTC (L).
    UPROPERTY() TObjectPtr<UInputAction> GoLive;
    // Start / stop the guided tour (F2).
    UPROPERTY() TObjectPtr<UInputAction> Tour;
    UPROPERTY() TObjectPtr<UInputAction> TourNext;
    // Milky Way guide (V).
    UPROPERTY() TObjectPtr<UInputAction> MilkyWayGuide;
    // Turn to face the Sun (U).
    UPROPERTY() TObjectPtr<UInputAction> FaceSun;

    UPROPERTY() TObjectPtr<UInputMappingContext> DesktopContext;
    UPROPERTY() TObjectPtr<UInputMappingContext> VRContext;
    UPROPERTY() TObjectPtr<UInputMappingContext> GlobalContext;

private:
    void BuildDesktop();
    void BuildVR();
    void BuildGlobal();
};
