// See CLAUDE.md Phase 8.
#include "AstroInputActions.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

namespace
{
    UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
    {
        UInputAction* Action = NewObject<UInputAction>(Outer, Name);
        Action->ValueType = Type;
        return Action;
    }

    // Maps a key onto one axis of an action, optionally negated; swizzles route a 1D key to Y or Z.
    void Map(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, int32 Axis = 0, bool bNegate = false, float Scale = 1.0f)
    {
        FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
        // Sticks and triggers drift; mouse deltas and wheel notches don't.
        if (Key.IsAnalog() && Key != EKeys::Mouse2D && Key != EKeys::MouseX && Key != EKeys::MouseY && Key != EKeys::MouseWheelAxis)
        {
            Mapping.Modifiers.Add(NewObject<UInputModifierDeadZone>(Context));
        }
        if (Axis == 1 || Axis == 2)
        {
            UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
            Swizzle->Order = Axis == 1 ? EInputAxisSwizzle::YXZ : EInputAxisSwizzle::ZYX;
            Mapping.Modifiers.Add(Swizzle);
        }
        if (bNegate)
        {
            Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
        }
        if (Scale != 1.0f)
        {
            UInputModifierScalar* Scalar = NewObject<UInputModifierScalar>(Context);
            Scalar->Scalar = FVector(Scale);
            Mapping.Modifiers.Add(Scalar);
        }
    }

}

UAstroInputActions* UAstroInputActions::Create(UObject* Outer)
{
    UAstroInputActions* A = NewObject<UAstroInputActions>(Outer);
    A->Move = MakeAction(A, TEXT("IA_Move"), EInputActionValueType::Axis3D);
    A->Look = MakeAction(A, TEXT("IA_Look"), EInputActionValueType::Axis2D);
    A->Roll = MakeAction(A, TEXT("IA_Roll"), EInputActionValueType::Axis1D);
    A->SpeedStep = MakeAction(A, TEXT("IA_SpeedStep"), EInputActionValueType::Axis1D);
    A->Boost = MakeAction(A, TEXT("IA_Boost"), EInputActionValueType::Boolean);
    A->Jump = MakeAction(A, TEXT("IA_Jump"), EInputActionValueType::Boolean);
    A->Land = MakeAction(A, TEXT("IA_Land"), EInputActionValueType::Boolean);
    A->SnapTurn = MakeAction(A, TEXT("IA_SnapTurn"), EInputActionValueType::Axis1D);
    A->Teleport = MakeAction(A, TEXT("IA_Teleport"), EInputActionValueType::Boolean);
    A->Select = MakeAction(A, TEXT("IA_Select"), EInputActionValueType::Boolean);
    A->TimeFaster = MakeAction(A, TEXT("IA_TimeFaster"), EInputActionValueType::Boolean);
    A->TimeSlower = MakeAction(A, TEXT("IA_TimeSlower"), EInputActionValueType::Boolean);
    A->TimePause = MakeAction(A, TEXT("IA_TimePause"), EInputActionValueType::Boolean);
    A->TimeRewind = MakeAction(A, TEXT("IA_TimeRewind"), EInputActionValueType::Boolean);
    A->Travel = MakeAction(A, TEXT("IA_Travel"), EInputActionValueType::Boolean);
    A->ToggleGalaxy = MakeAction(A, TEXT("IA_ToggleGalaxy"), EInputActionValueType::Boolean);
    A->ToggleHUD = MakeAction(A, TEXT("IA_ToggleHUD"), EInputActionValueType::Boolean);
    A->Menu = MakeAction(A, TEXT("IA_Menu"), EInputActionValueType::Boolean);
    A->Menu->bTriggerWhenPaused = true; // the menu pauses the game and must be able to close
    A->Help = MakeAction(A, TEXT("IA_Help"), EInputActionValueType::Boolean);

    A->DesktopContext = NewObject<UInputMappingContext>(A, TEXT("IMC_Desktop"));
    A->VRContext = NewObject<UInputMappingContext>(A, TEXT("IMC_VR"));
    A->GlobalContext = NewObject<UInputMappingContext>(A, TEXT("IMC_Global"));
    A->BuildDesktop();
    A->BuildVR();
    A->BuildGlobal();
    return A;
}

void UAstroInputActions::BuildDesktop()
{
    UInputMappingContext* C = DesktopContext;
    // Keyboard: WASD strafe, Space / C vertical, Q / E roll.
    Map(C, Move, EKeys::W, 0);
    Map(C, Move, EKeys::S, 0, true);
    Map(C, Move, EKeys::D, 1);
    Map(C, Move, EKeys::A, 1, true);
    Map(C, Move, EKeys::SpaceBar, 2);
    Map(C, Move, EKeys::C, 2, true);
    Map(C, Move, EKeys::LeftControl, 2, true);
    Map(C, Roll, EKeys::E);
    Map(C, Roll, EKeys::Q, 0, true);
    Map(C, Look, EKeys::Mouse2D);
    Map(C, SpeedStep, EKeys::MouseWheelAxis);
    Map(C, SpeedStep, EKeys::Add);
    Map(C, SpeedStep, EKeys::Subtract, 0, true);
    Map(C, Boost, EKeys::LeftShift);
    Map(C, Jump, EKeys::SpaceBar);
    Map(C, Land, EKeys::G);

    // Gamepad.
    Map(C, Move, EKeys::Gamepad_LeftY, 0);
    Map(C, Move, EKeys::Gamepad_LeftX, 1);
    Map(C, Move, EKeys::Gamepad_RightTriggerAxis, 2);
    Map(C, Move, EKeys::Gamepad_LeftTriggerAxis, 2, true);
    Map(C, Look, EKeys::Gamepad_Right2D, 0, false, 4.0f);
    Map(C, Roll, EKeys::Gamepad_RightShoulder);
    Map(C, Roll, EKeys::Gamepad_LeftShoulder, 0, true);
    Map(C, Boost, EKeys::Gamepad_LeftThumbstick);
    Map(C, Land, EKeys::Gamepad_FaceButton_Top);
    Map(C, Jump, EKeys::Gamepad_FaceButton_Bottom);
}

void UAstroInputActions::BuildVR()
{
    UInputMappingContext* C = VRContext;
    // UE5 has no generic motion-controller keys; map each control for Meta Touch, Valve Index
    // and Windows Mixed Reality controllers (OpenXR routes other devices onto these profiles).
    auto MapAll = [&](UInputAction* Action, std::initializer_list<FKey> Keys, int32 Axis = 0, bool bNegate = false)
    {
        for (const FKey& Key : Keys)
        {
            Map(C, Action, Key, Axis, bNegate);
        }
    };
    MapAll(Move, { EKeys::OculusTouch_Left_Thumbstick_Y, EKeys::ValveIndex_Left_Thumbstick_Y, EKeys::MixedReality_Left_Thumbstick_Y }, 0);
    MapAll(Move, { EKeys::OculusTouch_Left_Thumbstick_X, EKeys::ValveIndex_Left_Thumbstick_X, EKeys::MixedReality_Left_Thumbstick_X }, 1);
    MapAll(Move, { EKeys::OculusTouch_Left_Trigger_Axis, EKeys::ValveIndex_Left_Trigger_Axis, EKeys::MixedReality_Left_Trigger_Axis }, 2);
    MapAll(Move, { EKeys::OculusTouch_Left_Grip_Axis, EKeys::ValveIndex_Left_Grip_Axis }, 2, true);
    MapAll(SnapTurn, { EKeys::OculusTouch_Right_Thumbstick_X, EKeys::ValveIndex_Right_Thumbstick_X, EKeys::MixedReality_Right_Thumbstick_X });
    MapAll(Teleport, { EKeys::OculusTouch_Right_Thumbstick_Up, EKeys::ValveIndex_Right_Thumbstick_Up, EKeys::MixedReality_Right_Thumbstick_Up });
    MapAll(Boost, { EKeys::OculusTouch_Left_Thumbstick_Click, EKeys::ValveIndex_Left_Thumbstick_Click, EKeys::MixedReality_Left_Thumbstick_Click });
    MapAll(Land, { EKeys::OculusTouch_Left_Y_Click, EKeys::ValveIndex_Left_B_Click });
    MapAll(Jump, { EKeys::OculusTouch_Right_A_Click, EKeys::ValveIndex_Right_A_Click });

    // Global actions on VR controllers live here too, so the desktop context stays keyboard/gamepad only.
    MapAll(Select, { EKeys::OculusTouch_Right_Trigger_Click, EKeys::ValveIndex_Right_Trigger_Click, EKeys::MixedReality_Right_Trigger_Click });
    MapAll(TimePause, { EKeys::OculusTouch_Left_X_Click, EKeys::ValveIndex_Left_A_Click });
    MapAll(Travel, { EKeys::OculusTouch_Right_B_Click, EKeys::ValveIndex_Right_B_Click });
    MapAll(Menu, { EKeys::OculusTouch_Left_Menu_Click, EKeys::MixedReality_Left_Menu_Click });
}

void UAstroInputActions::BuildGlobal()
{
    UInputMappingContext* C = GlobalContext;
    Map(C, Select, EKeys::LeftMouseButton);
    Map(C, Select, EKeys::Gamepad_RightThumbstick);
    // Mobile groundwork (Phase 12): a tap selects what's under the reticle. Touch look/move
    // gestures are left to the port.
    Map(C, Select, EKeys::TouchKeys[ETouchIndex::Touch1]);
    Map(C, TimeFaster, EKeys::RightBracket);
    Map(C, TimeFaster, EKeys::Gamepad_DPad_Right);
    Map(C, TimeSlower, EKeys::LeftBracket);
    Map(C, TimeSlower, EKeys::Gamepad_DPad_Left);
    Map(C, TimePause, EKeys::P);
    Map(C, TimePause, EKeys::Gamepad_DPad_Up);
    Map(C, TimeRewind, EKeys::R);
    Map(C, TimeRewind, EKeys::Gamepad_DPad_Down);
    Map(C, Travel, EKeys::T);
    Map(C, Travel, EKeys::Gamepad_FaceButton_Right);
    Map(C, ToggleGalaxy, EKeys::M);
    Map(C, ToggleGalaxy, EKeys::Gamepad_Special_Left);
    Map(C, ToggleHUD, EKeys::H);
    Map(C, Menu, EKeys::Escape);
    Map(C, Help, EKeys::F1);
    Map(C, Menu, EKeys::Gamepad_Special_Right);
}
