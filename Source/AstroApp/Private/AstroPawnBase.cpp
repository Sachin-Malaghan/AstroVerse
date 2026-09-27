// See CLAUDE.md Phase 8.
#include "AstroPawnBase.h"
#include "AstroFloatingOriginComponent.h"
#include "AstroInputActions.h"
#include "AstroPlayerController.h"
#include "AstroSimulationSubsystem.h"
#include "AstroTravelSubsystem.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Math/AstroConstants.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroPawn, Log, All);

AAstroPawnBase::AAstroPawnBase()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    FloatingOrigin = CreateDefaultSubobject<UAstroFloatingOriginComponent>(TEXT("FloatingOrigin"));
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

void AAstroPawnBase::BeginPlay()
{
    Super::BeginPlay();
    SetActorLocation(FVector::ZeroVector);
}

FName AAstroPawnBase::GetReferenceBody() const
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    return Sim && Sim->GetRegistry().GetAll().IsValidIndex(ReferenceBody) ? Sim->GetRegistry().Get(ReferenceBody).BodyID : NAME_None;
}

void AAstroPawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    const AAstroPlayerController* PC = Cast<AAstroPlayerController>(GetController());
    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    const UAstroInputActions* A = PC ? PC->GetInputActions() : nullptr;
    if (!EIC || !A)
    {
        UE_LOG(LogAstroPawn, Warning, TEXT("No Enhanced Input / AstroPlayerController: pawn input unbound."));
        return;
    }
    EIC->BindAction(A->Move, ETriggerEvent::Triggered, this, &AAstroPawnBase::OnMove);
    EIC->BindAction(A->Move, ETriggerEvent::Completed, this, &AAstroPawnBase::OnMoveCompleted);
    EIC->BindAction(A->Look, ETriggerEvent::Triggered, this, &AAstroPawnBase::OnLook);
    EIC->BindAction(A->Roll, ETriggerEvent::Triggered, this, &AAstroPawnBase::OnRoll);
    EIC->BindAction(A->Roll, ETriggerEvent::Completed, this, &AAstroPawnBase::OnRollCompleted);
    EIC->BindAction(A->SpeedStep, ETriggerEvent::Triggered, this, &AAstroPawnBase::OnSpeedStep);
    EIC->BindAction(A->Boost, ETriggerEvent::Started, this, &AAstroPawnBase::OnBoost);
    EIC->BindAction(A->Boost, ETriggerEvent::Completed, this, &AAstroPawnBase::OnBoostCompleted);
    EIC->BindAction(A->Jump, ETriggerEvent::Started, this, &AAstroPawnBase::OnJump);
    EIC->BindAction(A->Land, ETriggerEvent::Started, this, &AAstroPawnBase::OnLand);
}

void AAstroPawnBase::OnMove(const FInputActionValue& Value) { Input.MoveAxis = Value.Get<FVector>(); }
void AAstroPawnBase::OnMoveCompleted(const FInputActionValue& Value) { Input.MoveAxis = FVector::ZeroVector; }
void AAstroPawnBase::OnLook(const FInputActionValue& Value) { Input.LookAccum += Value.Get<FVector2D>(); }
void AAstroPawnBase::OnRoll(const FInputActionValue& Value) { Input.RollAxis = Value.Get<float>(); }
void AAstroPawnBase::OnRollCompleted(const FInputActionValue& Value) { Input.RollAxis = 0.0f; }
void AAstroPawnBase::OnSpeedStep(const FInputActionValue& Value) { Input.SpeedSteps += Value.Get<float>(); }
void AAstroPawnBase::OnBoost(const FInputActionValue& Value) { Input.bBoost = true; }
void AAstroPawnBase::OnBoostCompleted(const FInputActionValue& Value) { Input.bBoost = false; }
void AAstroPawnBase::OnJump(const FInputActionValue& Value) { bJumpRequested = true; }
void AAstroPawnBase::OnLand(const FInputActionValue& Value) { ToggleLanding(); }

void AAstroPawnBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (!Sim || !Sim->IsReady())
    {
        return;
    }
    if (!bFacedInitialBody && Sim->GetRenderOriginAnchorBody() != INDEX_NONE)
    {
        // Start looking at whatever the origin is anchored to.
        const FAstroVector3d Target = Sim->GetSimulation().GetBodyState(Sim->GetRenderOriginAnchorBody()).Position;
        SetActorRotation(Sim->SimToEngineDirection((Target - Sim->GetRenderOrigin()).Normalized()).Rotation());
        bFacedInitialBody = true;
    }

    // In transit the travel system flies the view; piloted ships take throttle and steering from us.
    if (UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this); Travel && Travel->IsTravelling())
    {
        const FVector2D Look = Input.ConsumeLook();
        if (Travel->GetActiveStyle() == EAstroTravelStyle::PilotedShip)
        {
            Travel->SetPilotInput(static_cast<float>(Input.MoveAxis.X), FVector2D(Look.X + Input.MoveAxis.Y * 4.0, Look.Y));
        }
        Velocity = FVector::ZeroVector;
        Locomotion = EAstroLocomotion::Flying;
        return;
    }

    UpdateReferenceFrame(Sim);
    if (Locomotion == EAstroLocomotion::Walking)
    {
        TickWalking(Sim, DeltaSeconds);
    }
    else
    {
        TickFlying(Sim, DeltaSeconds);
    }
}

double AAstroPawnBase::SphereOfInfluence(const UAstroSimulationSubsystem* Sim, int32 Body) const
{
    // Laplace sphere of influence: r = a (m / M)^(2/5).
    const FBodyRegistry& Registry = Sim->GetRegistry();
    const FBodyDefinition& Def = Registry.Get(Body);
    if (Def.ParentIndex == INDEX_NONE)
    {
        return TNumericLimits<double>::Max();
    }
    return Def.Elements.SemiMajorAxis * FMath::Pow(Def.MassKg / Registry.Get(Def.ParentIndex).MassKg, 0.4);
}

bool AAstroPawnBase::SampleGround(const UAstroSimulationSubsystem* Sim, FVector& OutUp, double& OutHeightAboveGround, double& OutGravity) const
{
    if (ReferenceBody == INDEX_NONE)
    {
        return false;
    }
    const FBodyDefinition& Body = Sim->GetRegistry().Get(ReferenceBody);
    const FAstroVector3d Rel = Sim->EngineToSimPosition(GetActorLocation()) - Sim->GetSimulation().GetBodyState(ReferenceBody).Position;
    const double R = Rel.Length();
    const FAstroVector3d DirBF = (Body.GetOrientationAt(Sim->GetSimulation().GetSimSeconds()).Transposed() * Rel) / R;
    double Ground;
    if (Body.Terrain.IsValid())
    {
        Ground = Body.Terrain->EllipsoidRadius(DirBF) + Body.Terrain->HeightAt(DirBF, 0.5);
    }
    else
    {
        const double A = Body.EquatorialRadiusMeters, C = Body.PolarRadiusMeters;
        Ground = 1.0 / FMath::Sqrt((DirBF.X * DirBF.X + DirBF.Y * DirBF.Y) / (A * A) + DirBF.Z * DirBF.Z / (C * C));
    }
    OutUp = Sim->SimToEngineDirection(Rel / R).GetSafeNormal();
    OutHeightAboveGround = R - Ground;
    OutGravity = Body.GM / (R * R);
    return true;
}

void AAstroPawnBase::SwitchFrame(UAstroSimulationSubsystem* Sim, TFunctionRef<void()> Change)
{
    // Keep the pawn's sim position; carry orientation and velocity through the basis change.
    const FAstroRenderFrame Before = Sim->GetRenderFrame();
    const FAstroVector3d PawnSim = Before.ToSimPosition(GetActorLocation());
    Change();
    const FAstroRenderFrame After = Sim->GetRenderFrame();
    FAstroRenderFrame Delta;
    Delta.Basis = After.Basis * Before.Basis.Transposed();
    const FQuat Rotation = Delta.ToEngineRotation(FAstroMatrix3d::Identity());
    SetActorLocationAndRotation(After.ToEnginePosition(PawnSim), Rotation * GetActorQuat());
    Velocity = Rotation.RotateVector(Velocity);
}

void AAstroPawnBase::UpdateReferenceFrame(UAstroSimulationSubsystem* Sim)
{
    const FBodyRegistry& Registry = Sim->GetRegistry();
    const FAstroVector3d PawnSim = Sim->EngineToSimPosition(GetActorLocation());

    // Deepest sphere of influence containing the pawn (hysteresis keeps the current one a little longer).
    int32 Best = Registry.GetStarIndex();
    double BestSOI = TNumericLimits<double>::Max();
    for (int32 i = 0; i < Registry.Num(); ++i)
    {
        if (i == Registry.GetStarIndex())
        {
            continue;
        }
        const double SOI = SphereOfInfluence(Sim, i) * (i == ReferenceBody ? 1.05 : 1.0);
        if ((PawnSim - Sim->GetSimulation().GetBodyState(i).Position).Length() < SOI && SOI < BestSOI)
        {
            Best = i;
            BestSOI = SOI;
        }
    }
    ReferenceBody = Best;

    FVector Up;
    double Height = 0.0, Gravity = 0.0;
    SampleGround(Sim, Up, Height, Gravity);
    Altitude = Height;

    const FBodyDefinition& Def = Registry.Get(Best);
    const double Threshold = FMath::Min(BodyFixedFrameAltitude, Def.EquatorialRadiusMeters * 0.5);
    const bool bInRotatingFrame = Sim->IsRotatingFrame() && Sim->GetRenderOriginAnchorBody() == Best;
    const bool bWantRotating = Locomotion == EAstroLocomotion::Walking
        || (Def.BodyType != EAstroBodyType::Star && Height < (bInRotatingFrame ? Threshold * 1.2 : Threshold));

    if (bWantRotating && !bInRotatingFrame)
    {
        SwitchFrame(Sim, [&]() { Sim->ConvertAnchorToBodyFixed(Best); });
        UE_LOG(LogAstroPawn, Display, TEXT("Frame: %s co-rotating"), *Def.BodyID.ToString());
    }
    else if (!bWantRotating && (bInRotatingFrame || Sim->GetRenderOriginAnchorBody() != Best))
    {
        SwitchFrame(Sim, [&]()
        {
            Sim->SetRenderOriginAnchor(Best, Sim->GetRenderOrigin() - Sim->GetSimulation().GetBodyState(Best).Position);
        });
        UE_LOG(LogAstroPawn, Display, TEXT("Frame: %s inertial"), *Def.BodyID.ToString());
    }
}

void AAstroPawnBase::ApplyLookInput(float DeltaSeconds)
{
    const FVector2D Look = Input.ConsumeLook();
    // 6DOF: yaw about the pawn's up, pitch about its right, roll about its forward.
    const double Yaw = Look.X * 0.1, Pitch = Look.Y * 0.1, Roll = Input.RollAxis * 60.0 * DeltaSeconds;
    const FQuat Q = GetActorQuat();
    const FQuat Delta = FQuat(Q.GetUpVector(), FMath::DegreesToRadians(Yaw))
                      * FQuat(Q.GetRightVector(), FMath::DegreesToRadians(-Pitch))
                      * FQuat(Q.GetForwardVector(), FMath::DegreesToRadians(Roll));
    SetActorRotation(Delta * Q);
}

void AAstroPawnBase::TickFlying(UAstroSimulationSubsystem* Sim, float DeltaSeconds)
{
    ApplyLookInput(DeltaSeconds);

    const float Steps = Input.ConsumeSpeedSteps();
    SpeedMultiplier = FMath::Clamp(SpeedMultiplier * FMath::Pow(1.25, Steps), 1e-3, 1e3);

    // Speed ~ altitude: the same stick deflection reads as walking pace at the ground and
    // crossing AU between planets.
    const double SpeedMps = SpeedPerAltitude * SpeedMultiplier * FMath::Max(Altitude, 2.0) * (Input.bBoost ? 10.0 : 1.0);
    const FVector Target = GetMovementBasis().RotateVector(Input.MoveAxis.GetClampedToMaxSize(1.0)) * SpeedMps * 100.0;
    Velocity = FMath::Lerp(Velocity, Target, 1.0 - FMath::Exp(-6.0 * DeltaSeconds));
    AddActorWorldOffset(Velocity * DeltaSeconds);

    // Never below the surface (gas giants: the cloud tops).
    FVector Up;
    double Height = 0.0, Gravity = 0.0;
    if (SampleGround(Sim, Up, Height, Gravity) && Height < 1.0)
    {
        AddActorWorldOffset(Up * (1.0 - Height) * 100.0);
        const double Into = FVector::DotProduct(Velocity, Up);
        if (Into < 0.0)
        {
            Velocity -= Up * Into;
        }
    }

    // Close to a surface, ease the horizon level for comfort.
    if (Sim->IsRotatingFrame() && Altitude < 20000.0 && FMath::IsNearlyZero(Input.RollAxis))
    {
        const FQuat Q = GetActorQuat();
        const FVector Forward = Q.GetForwardVector();
        if (FMath::Abs(FVector::DotProduct(Forward, Up)) < 0.95)
        {
            const FQuat Level = FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();
            SetActorRotation(FQuat::Slerp(Q, Level, 1.0 - FMath::Exp(-1.5 * DeltaSeconds)));
        }
    }
}

void AAstroPawnBase::TickWalking(UAstroSimulationSubsystem* Sim, float DeltaSeconds)
{
    FVector Up;
    double Height = 0.0, Gravity = 0.0;
    if (!SampleGround(Sim, Up, Height, Gravity))
    {
        Locomotion = EAstroLocomotion::Flying;
        return;
    }
    const FBodyDefinition& Body = Sim->GetRegistry().Get(ReferenceBody);

    // Upright view: yaw/pitch relative to local north/up.
    const FVector2D Look = Input.ConsumeLook();
    WalkYaw += Look.X * 0.1;
    WalkPitch = FMath::Clamp(WalkPitch + Look.Y * 0.1, -89.0, 89.0);
    const FVector Pole = Sim->SimToEngineDirection(Body.GetOrientationAt(Sim->GetSimulation().GetSimSeconds()).GetColumn(2));
    FVector North = (Pole - Up * FVector::DotProduct(Pole, Up)).GetSafeNormal();
    if (North.IsNearlyZero())
    {
        North = FVector::CrossProduct(Up, FVector::RightVector).GetSafeNormal();
    }
    const FVector Forward = FQuat(Up, FMath::DegreesToRadians(WalkYaw)).RotateVector(North);
    const FVector Right = FVector::CrossProduct(Up, Forward);
    const FVector View = FQuat(Right, FMath::DegreesToRadians(-WalkPitch)).RotateVector(Forward);
    SetActorRotation(FRotationMatrix::MakeFromXZ(UsesHeadTracking() ? Forward : View, Up).ToQuat());

    // Horizontal walking; vertical is ballistic under the body's real gravity.
    const double Speed = Input.bBoost ? RunSpeed : WalkSpeed;
    const FVector Horizontal = (Forward * Input.MoveAxis.X + Right * Input.MoveAxis.Y).GetClampedToMaxSize(1.0) * Speed * 100.0;
    double Vertical = FVector::DotProduct(Velocity, Up) - Gravity * 100.0 * DeltaSeconds;
    const bool bGrounded = Height <= EyeHeight + 0.05;
    if (bGrounded && bJumpRequested)
    {
        Vertical = 300.0; // 3 m/s: ~0.46 m on Earth, ~2.8 m on the Moon
    }
    bJumpRequested = false;
    Velocity = Horizontal + Up * Vertical;
    AddActorWorldOffset(Velocity * DeltaSeconds);

    // Stand on the terrain.
    if (SampleGround(Sim, Up, Height, Gravity) && Height < EyeHeight)
    {
        AddActorWorldOffset(Up * (EyeHeight - Height) * 100.0);
        if (FVector::DotProduct(Velocity, Up) < 0.0)
        {
            Velocity -= Up * FVector::DotProduct(Velocity, Up);
        }
    }
    Altitude = Height;
}

void AAstroPawnBase::ToggleLanding()
{
    UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (!Sim || !Sim->IsReady())
    {
        return;
    }
    // The pawn may have been moved since its last tick (teleport, travel arrival).
    UpdateReferenceFrame(Sim);
    if (ReferenceBody == INDEX_NONE)
    {
        return;
    }
    const FBodyDefinition& Body = Sim->GetRegistry().Get(ReferenceBody);
    if (Locomotion == EAstroLocomotion::Walking)
    {
        Locomotion = EAstroLocomotion::Flying;
        FVector Up;
        double Height, Gravity;
        if (SampleGround(Sim, Up, Height, Gravity))
        {
            Velocity = Up * 500.0;
        }
        UE_LOG(LogAstroPawn, Display, TEXT("Took off from %s"), *Body.BodyID.ToString());
        return;
    }
    const bool bSolid = Body.Terrain.IsValid() && Body.Terrain->HasSolidSurface();
    if (!bSolid || Altitude > 5000.0)
    {
        UE_LOG(LogAstroPawn, Display, TEXT("Can't land: %s"), bSolid ? TEXT("descend below 5 km first") : TEXT("no solid surface"));
        return;
    }
    // Keep facing the same way: derive walk yaw from the current forward.
    Locomotion = EAstroLocomotion::Walking;
    Velocity = FVector::ZeroVector;
    WalkPitch = 0.0;
    WalkYaw = 0.0;
    UpdateReferenceFrame(Sim);
    FVector Up;
    double Height, Gravity;
    if (SampleGround(Sim, Up, Height, Gravity))
    {
        const FVector Pole = Sim->SimToEngineDirection(Body.GetOrientationAt(Sim->GetSimulation().GetSimSeconds()).GetColumn(2));
        const FVector North = (Pole - Up * FVector::DotProduct(Pole, Up)).GetSafeNormal();
        const FVector Forward = (GetActorForwardVector() - Up * FVector::DotProduct(GetActorForwardVector(), Up)).GetSafeNormal();
        if (!North.IsNearlyZero() && !Forward.IsNearlyZero())
        {
            const double Cos = FVector::DotProduct(North, Forward);
            const double Sin = FVector::DotProduct(Up, FVector::CrossProduct(North, Forward));
            WalkYaw = FMath::RadiansToDegrees(FMath::Atan2(Sin, Cos));
        }
    }
    UE_LOG(LogAstroPawn, Display, TEXT("Landed on %s (g = %.2f m/s^2)"), *Body.BodyID.ToString(), Body.GM / FMath::Square(Body.EquatorialRadiusMeters));
}

void AAstroPawnBase::TeleportToSim(const FAstroVector3d& SimPosition, const FQuat& EngineRotation)
{
    if (UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this))
    {
        SetActorLocationAndRotation(Sim->SimToEnginePosition(SimPosition), EngineRotation);
        Velocity = FVector::ZeroVector;
        FloatingOrigin->RebaseNow();
    }
}
