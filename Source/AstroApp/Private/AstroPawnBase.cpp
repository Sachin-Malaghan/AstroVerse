// See CLAUDE.md Phase 8.
#include "AstroPawnBase.h"
#include "AstroFloatingOriginComponent.h"
#include "AstroInputActions.h"
#include "AstroPlayerController.h"
#include "AstroSimulationSubsystem.h"
#include "AstroTravelSubsystem.h"
#include "AstroScaleDomainSubsystem.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Math/AstroConstants.h"
#include "BodyTerrain.h"
#include "AstroTourSubsystem.h"
#include "Camera/CameraComponent.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroPawn, Log, All);

static TAutoConsoleVariable<float> CVarAstroZoomStep(TEXT("astro.Camera.ZoomStep"), 0.45f,
    TEXT("Wheel zoom per notch, as ln(distance): 0.45 = ~57% closer per notch (x3 with Shift)."));
static TAutoConsoleVariable<float> CVarAstroTelescopeMin(TEXT("astro.Camera.TelescopeMinFOV"), 0.05f,
    TEXT("Narrowest telescope field of view (deg): 0.05 = ~1400x."));


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
    if (const UCameraComponent* Camera = FindComponentByClass<UCameraComponent>())
    {
        BaseFOV = TelescopeFOV = Camera->FieldOfView;
    }
    SetActorLocation(FVector::ZeroVector);
    if (UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this))
    {
        Travel->OnTravelStarted.AddDynamic(this, &AAstroPawnBase::HandleTravelStarted);
        Travel->OnTravelArrived.AddDynamic(this, &AAstroPawnBase::HandleTravelArrived);
    }
    if (UAstroTourSubsystem* Tour = UAstroTourSubsystem::Get(this))
    {
        Tour->OnCameraRequest.AddUObject(this, &AAstroPawnBase::HandleTourCamera);
    }
    if (UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this))
    {
        Sim->OnViewerPlaced.AddWeakLambda(this, [this]() { bOrbiting = false; FaceTarget = INDEX_NONE; });
    }
}

void AAstroPawnBase::HandleTourCamera(FName BodyID, double DistanceRadii, double PhaseDeg, double ElevationDeg, double DriftDegPerSecond, bool bUseSide)
{
    SetOrbitDrift(DriftDegPerSecond);
    if (BodyID.IsNone())
    {
        return; // tour ended: just stop the drift
    }
    if (bUseSide)
    {
        FocusOnFromSunSide(BodyID, DistanceRadii, PhaseDeg, ElevationDeg, false);
    }
    else
    {
        FocusOn(BodyID, DistanceRadii, false);
    }
}

void AAstroPawnBase::HandleTravelStarted(FName BodyID)
{
    bOrbiting = false;
}

void AAstroPawnBase::HandleTravelArrived(FName BodyID)
{
    // Arrive in orbit around the destination: zoom and circle it straight away.
    FocusOn(BodyID, 0.0, true);
}

FName AAstroPawnBase::GetOrbitBody() const
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    return bOrbiting && Sim && Sim->GetRegistry().GetAll().IsValidIndex(OrbitBody) ? Sim->GetRegistry().Get(OrbitBody).BodyID : NAME_None;
}

double AAstroPawnBase::SurfaceRadius(const UAstroSimulationSubsystem* Sim, int32 Body, const FAstroVector3d& Dir) const
{
    const FBodyDefinition& Def = Sim->GetRegistry().Get(Body);
    const FAstroVector3d DirBF = Def.GetOrientationAt(Sim->GetSimulation().GetSimSeconds()).Transposed() * Dir;
    if (Def.Terrain.IsValid())
    {
        return Def.Terrain->EllipsoidRadius(DirBF) + FMath::Max(0.0, Def.Terrain->HeightAt(DirBF, 0.5));
    }
    const double A = Def.EquatorialRadiusMeters, C = Def.PolarRadiusMeters;
    return 1.0 / FMath::Sqrt((DirBF.X * DirBF.X + DirBF.Y * DirBF.Y) / (A * A) + DirBF.Z * DirBF.Z / (C * C));
}

void AAstroPawnBase::FocusOn(FName BodyID, double DistanceRadii, bool bInstant)
{
    UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    const int32 Body = Sim && Sim->IsReady() ? Sim->FindBodyIndex(BodyID) : INDEX_NONE;
    if (Body == INDEX_NONE)
    {
        return;
    }
    const FBodyDefinition& Def = Sim->GetRegistry().Get(Body);
    const FAstroVector3d BodyPos = Sim->GetSimulation().GetBodyState(Body).Position;
    FAstroVector3d Rel = Sim->EngineToSimPosition(GetActorLocation()) - BodyPos;
    double Distance = Rel.Length();
    if (Distance < 1.0)
    {
        Rel = FAstroVector3d(1.0, 0.0, 0.2);
        Distance = 4.0 * Def.EquatorialRadiusMeters;
    }
    OrbitBody = Body;
    OrbitDir = OrbitTargetDir = Rel / Distance;
    const double Surface = SurfaceRadius(Sim, Body, OrbitDir);
    const double CurrentAlt = FMath::Max(Distance - Surface, 30.0);
    // From far away (or when asked) fly in to a framing distance; close by, keep the height.
    double TargetAlt = CurrentAlt;
    if (DistanceRadii > 0.0)
    {
        TargetAlt = FMath::Max((DistanceRadii - 1.0) * Def.EquatorialRadiusMeters, 30.0);
    }
    else if (Distance > 8.0 * Def.EquatorialRadiusMeters)
    {
        TargetAlt = 2.5 * Def.EquatorialRadiusMeters;
    }
    OrbitTargetLogAltitude = FMath::Loge(TargetAlt);
    OrbitLogAltitude = bInstant ? OrbitTargetLogAltitude : FMath::Loge(CurrentAlt);
    OrbitLastSimSeconds = Sim->GetSimulation().GetSimSeconds();
    OrbitView = bInstant ? FQuat::Identity : GetActorQuat();
    Locomotion = EAstroLocomotion::Flying;
    Velocity = FVector::ZeroVector;
    bOrbiting = true;
    if (bInstant)
    {
        // Snap: put the pawn in place now so the frame logic sees the right altitude.
        const double Radius = Surface + TargetAlt;
        const FAstroVector3d Pos = BodyPos + OrbitDir * Radius;
        TeleportToSim(Pos, Sim->SimToEngineDirection(-OrbitDir).ToOrientationQuat());
        UpdateReferenceFrame(Sim);
        OrbitView = GetActorQuat();
    }
    UE_LOG(LogAstroPawn, Display, TEXT("Orbiting %s"), *Def.BodyID.ToString());
}

void AAstroPawnBase::FocusOnFromSunSide(FName BodyID, double DistanceRadii, double PhaseDeg, double ElevationDeg, bool bInstant)
{
    UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    const int32 Body = Sim && Sim->IsReady() ? Sim->FindBodyIndex(BodyID) : INDEX_NONE;
    if (Body == INDEX_NONE)
    {
        return;
    }
    const FBodyRegistry& Registry = Sim->GetRegistry();
    const FAstroVector3d BodyPos = Sim->GetSimulation().GetBodyState(Body).Position;
    FAstroVector3d ToSun = Sim->GetSimulation().GetBodyState(Registry.GetStarIndex()).Position - BodyPos;
    ToSun = ToSun.Length() > 1.0 ? ToSun.Normalized() : FAstroVector3d(1.0, 0.0, 0.0);
    const double Yaw = FMath::Atan2(ToSun.Y, ToSun.X) + FMath::DegreesToRadians(PhaseDeg);
    const double Pitch = FMath::DegreesToRadians(ElevationDeg);
    const FAstroVector3d Dir(FMath::Cos(Pitch) * FMath::Cos(Yaw), FMath::Cos(Pitch) * FMath::Sin(Yaw), FMath::Sin(Pitch));
    const double Radius = DistanceRadii * Registry.Get(Body).EquatorialRadiusMeters;
    if (bInstant)
    {
        // Leave the current frame for one anchored at the body so the jump is exact.
        Sim->SetRenderOriginAnchor(Body, Dir * Radius);
        SetActorLocation(FVector::ZeroVector);
    }
    else
    {
        // Start the fly-in from along the requested direction.
        TeleportToSim(BodyPos + Dir * (Sim->EngineToSimPosition(GetActorLocation()) - BodyPos).Length(), GetActorQuat());
    }
    FocusOn(BodyID, DistanceRadii, bInstant);
}

void AAstroPawnBase::FaceBody(FName BodyID)
{
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    FaceTarget = Sim && Sim->IsReady() ? Sim->FindBodyIndex(BodyID) : INDEX_NONE;
    FaceSeconds = 0.0;
    if (FaceTarget != INDEX_NONE)
    {
        bOrbiting = false;
    }
}

bool AAstroPawnBase::ApplyFaceTarget(UAstroSimulationSubsystem* Sim, float DeltaSeconds)
{
    if (FaceTarget == INDEX_NONE)
    {
        return false;
    }
    FaceSeconds += DeltaSeconds;
    if (!Input.LookAccum.IsNearlyZero() || FaceSeconds > 3.0)
    {
        FaceTarget = INDEX_NONE; // the user took over, or we're there
        return false;
    }
    const FVector Dir = Sim->SimToEngineDirection((Sim->GetSimulation().GetBodyState(FaceTarget).Position - Sim->EngineToSimPosition(GetActorLocation())).Normalized()).GetSafeNormal();
    const double Ease = 1.0 - FMath::Exp(-4.0 * DeltaSeconds);
    if (Locomotion == EAstroLocomotion::Walking)
    {
        FVector Up;
        double Height, Gravity;
        if (!SampleGround(Sim, Up, Height, Gravity))
        {
            return false;
        }
        const FBodyDefinition& Body = Sim->GetRegistry().Get(ReferenceBody);
        const FVector Pole = Sim->SimToEngineDirection(Body.GetOrientationAt(Sim->GetSimulation().GetSimSeconds()).GetColumn(2));
        const FVector North = (Pole - Up * FVector::DotProduct(Pole, Up)).GetSafeNormal();
        const FVector Flat = (Dir - Up * FVector::DotProduct(Dir, Up)).GetSafeNormal();
        if (!North.IsNearlyZero() && !Flat.IsNearlyZero())
        {
            // Same convention as ToggleLanding / TickWalking.
            const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Up, FVector::CrossProduct(North, Flat)), FVector::DotProduct(North, Flat)));
            WalkYaw += FMath::FindDeltaAngleDegrees(WalkYaw, Yaw) * Ease;
        }
        const double Pitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(FVector::DotProduct(Dir, Up), -1.0, 1.0))), -89.0, 89.0);
        WalkPitch += (Pitch - WalkPitch) * Ease;
        return false; // TickWalking applies yaw/pitch
    }
    const FVector CurrentUp = GetActorUpVector();
    const FQuat Target = FRotationMatrix::MakeFromXZ(Dir, CurrentUp).ToQuat();
    SetActorRotation(FQuat::Slerp(GetActorQuat(), Target, Ease));
    return true;
}

void AAstroPawnBase::GoHome()
{
    if (UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this);
        Domains && (Domains->GetDomain() == EAstroScaleDomain::Galaxy || Domains->IsTransitioning()))
    {
        if (!Domains->IsTransitioning())
        {
            Domains->RequestDomain(EAstroScaleDomain::SolarSystem);
        }
        bPendingHome = true; // finish once back in the solar system
        return;
    }
    if (UAstroTravelSubsystem* Travel = UAstroTravelSubsystem::Get(this); Travel && Travel->IsTravelling())
    {
        Travel->FinishNow();
    }
    bPendingHome = false;
    SpeedMultiplier = 1.0;
    StopMotion();
    FocusOnFromSunSide(HomeBody, HomeDistanceRadii, 45.0, 15.0, true);
}

void AAstroPawnBase::TickOrbit(UAstroSimulationSubsystem* Sim, float DeltaSeconds)
{
    if (!Sim->GetRegistry().GetAll().IsValidIndex(OrbitBody))
    {
        bOrbiting = false;
        return;
    }
    const FBodyDefinition& Def = Sim->GetRegistry().Get(OrbitBody);
    const double Now = Sim->GetSimulation().GetSimSeconds();
    const FAstroMatrix3d Orientation = Def.GetOrientationAt(Now);
    const FAstroVector3d Pole = Orientation.GetColumn(2);

    // Close in, ride with the rotating surface (as the co-rotating frame does below 100 km).
    const double Altitude0 = FMath::Exp(OrbitLogAltitude);
    if (Altitude0 < FMath::Min(BodyFixedFrameAltitude, Def.EquatorialRadiusMeters * 0.5))
    {
        const FAstroMatrix3d Spin = Orientation * Def.GetOrientationAt(OrbitLastSimSeconds).Transposed();
        OrbitDir = (Spin * OrbitDir).Normalized();
        OrbitTargetDir = (Spin * OrbitTargetDir).Normalized();
    }
    OrbitLastSimSeconds = Now;

    // Input: mouse / A-D / Space-C orbit; wheel and W-S zoom on a log scale.
    const FVector2D Look = Input.ConsumeLook();
    const double Boost = Input.bBoost ? 3.0 : 1.0;
    const double YawDeg = Look.X * 0.15 + (Input.MoveAxis.Y * 60.0 * Boost + OrbitDriftDegPerSecond) * DeltaSeconds;
    const double PitchDeg = -Look.Y * 0.15 + Input.MoveAxis.Z * 45.0 * Boost * DeltaSeconds;
    const float Steps = ZoomSteps + Input.ConsumeSpeedSteps() * 0.0f;
    ZoomSteps = 0.0f;
    OrbitTargetLogAltitude -= Steps * CVarAstroZoomStep.GetValueOnGameThread() * Boost + Input.MoveAxis.X * 1.5 * Boost * DeltaSeconds;
    OrbitTargetLogAltitude = FMath::Clamp(OrbitTargetLogAltitude, FMath::Loge(5.0), FMath::Loge(200.0 * AstroConstants::AstronomicalUnit));

    auto RotateAbout = [](const FAstroVector3d& V, const FAstroVector3d& Axis, double Radians)
    {
        const double C = FMath::Cos(Radians), S = FMath::Sin(Radians);
        return V * C + Axis.Cross(V) * S + Axis * (Axis.Dot(V) * (1.0 - C));
    };
    FAstroVector3d Target = RotateAbout(OrbitTargetDir, Pole, FMath::DegreesToRadians(YawDeg));
    const FAstroVector3d Side = Pole.Cross(Target);
    if (Side.Length() > 1e-6)
    {
        const FAstroVector3d Pitched = RotateAbout(Target, Side.Normalized(), FMath::DegreesToRadians(-PitchDeg));
        if (FMath::Abs(Pitched.Dot(Pole)) < 0.995) // stop short of the poles
        {
            Target = Pitched;
        }
    }
    OrbitTargetDir = Target.Normalized();

    // Ease toward the targets: smooth on-screen, and a far-to-near focus flies in over ~2 s.
    const double Ease = 1.0 - FMath::Exp(-8.0 * DeltaSeconds);
    const double ZoomEase = 1.0 - FMath::Exp(-4.0 * DeltaSeconds);
    OrbitDir = (OrbitDir + (OrbitTargetDir - OrbitDir) * Ease).Normalized();
    OrbitLogAltitude += (OrbitTargetLogAltitude - OrbitLogAltitude) * ZoomEase;

    const FAstroVector3d BodyPos = Sim->GetSimulation().GetBodyState(OrbitBody).Position;
    const double Radius = SurfaceRadius(Sim, OrbitBody, OrbitDir) + FMath::Exp(OrbitLogAltitude);
    const FVector Forward = Sim->SimToEngineDirection(-OrbitDir);
    const FVector Up = Sim->SimToEngineDirection(Pole);
    const FQuat Desired = FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();
    OrbitView = FQuat::Slerp(OrbitView, Desired, 1.0 - FMath::Exp(-6.0 * DeltaSeconds));
    SetActorLocationAndRotation(Sim->SimToEnginePosition(BodyPos + OrbitDir * Radius), OrbitView);
    Velocity = FVector::ZeroVector;
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
    EIC->BindAction(A->Zoom, ETriggerEvent::Triggered, this, &AAstroPawnBase::OnZoom);
    EIC->BindAction(A->Telescope, ETriggerEvent::Triggered, this, &AAstroPawnBase::OnTelescope);
    EIC->BindAction(A->Telescope, ETriggerEvent::Completed, this, &AAstroPawnBase::OnTelescopeCompleted);
    EIC->BindAction(A->TelescopeReset, ETriggerEvent::Started, this, &AAstroPawnBase::OnTelescopeReset);
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
void AAstroPawnBase::OnZoom(const FInputActionValue& Value) { ZoomSteps += Value.Get<float>(); }
void AAstroPawnBase::OnTelescope(const FInputActionValue& Value) { TelescopeAxis = Value.Get<float>(); }
void AAstroPawnBase::OnTelescopeCompleted(const FInputActionValue& Value) { TelescopeAxis = 0.0f; }
void AAstroPawnBase::OnTelescopeReset(const FInputActionValue& Value) { TelescopeFOV = BaseFOV; }

void AAstroPawnBase::ApplyTelescope(float DeltaSeconds)
{
    UCameraComponent* Camera = FindComponentByClass<UCameraComponent>();
    if (!Camera || UsesHeadTracking())
    {
        return; // VR: the headset owns the field of view
    }
    if (FMath::Abs(TelescopeAxis) > 0.01f)
    {
        // Exponential: the same hold time halves / doubles the view at any magnification.
        TelescopeFOV = FMath::Clamp(TelescopeFOV * FMath::Exp(-TelescopeAxis * 1.4f * DeltaSeconds), CVarAstroTelescopeMin.GetValueOnGameThread(), BaseFOV);
    }
    if (!FMath::IsNearlyEqual(Camera->FieldOfView, TelescopeFOV, 0.001f))
    {
        Camera->SetFieldOfView(TelescopeFOV);
    }
}
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

    // Galaxy scale-domain: its own units and camera moves; the solar-system frame is untouched.
    if (UAstroScaleDomainSubsystem* Domains = UAstroScaleDomainSubsystem::Get(this))
    {
        if (Domains->IsTransitioning())
        {
            Input.ConsumeLook();
            Velocity = FVector::ZeroVector;
            return;
        }
        if (Domains->GetDomain() == EAstroScaleDomain::Galaxy)
        {
            TickGalaxyFlight(Domains->GetGalaxyView(), DeltaSeconds);
            return;
        }
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

    if (bPendingHome)
    {
        GoHome();
        return;
    }

    ApplyTelescope(DeltaSeconds);
    UpdateReferenceFrame(Sim);
    if (!bOrbiting && Locomotion == EAstroLocomotion::Flying && !FMath::IsNearlyZero(ZoomSteps))
    {
        // Wheel in free flight: zoom toward the selected body (else the nearest), like
        // Solar System Scope. Flight speed stays on + / -.
        FName Target = GetReferenceBody();
        if (const AAstroPlayerController* PC = Cast<AAstroPlayerController>(GetController()); PC && !PC->GetSelectedBody().IsNone())
        {
            Target = PC->GetSelectedBody();
        }
        const float Pending = ZoomSteps;
        FocusOn(Target, 0.0, false);
        // FocusOn from afar flies in to a framing distance; keep the current distance instead.
        if (bOrbiting)
        {
            OrbitTargetLogAltitude = OrbitLogAltitude;
        }
        ZoomSteps = Pending;
    }
    if (bOrbiting && Locomotion == EAstroLocomotion::Flying)
    {
        TickOrbit(Sim, DeltaSeconds);
    }
    else if (Locomotion == EAstroLocomotion::Walking)
    {
        ZoomSteps = 0.0f; // on foot the wheel does nothing (use Z / X to look closer)
        ApplyFaceTarget(Sim, DeltaSeconds);
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
    if (!ApplyFaceTarget(Sim, DeltaSeconds))
    {
        ApplyLookInput(DeltaSeconds);
    }

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

void AAstroPawnBase::TickGalaxyFlight(const FGalaxyView& Galaxy, float DeltaSeconds)
{
    ApplyLookInput(DeltaSeconds);
    const float Steps = Input.ConsumeSpeedSteps();
    SpeedMultiplier = FMath::Clamp(SpeedMultiplier * FMath::Pow(1.25, Steps), 1e-3, 1e3);
    // Same feel as in the solar system: speed grows with distance to the nearest landmark.
    const double SpeedCm = SpeedPerAltitude * SpeedMultiplier * Galaxy.GetFlightScaleCm(GetActorLocation()) * (Input.bBoost ? 10.0 : 1.0);
    const FVector Target = GetMovementBasis().RotateVector(Input.MoveAxis.GetClampedToMaxSize(1.0)) * SpeedCm;
    Velocity = FMath::Lerp(Velocity, Target, 1.0 - FMath::Exp(-6.0 * DeltaSeconds));
    AddActorWorldOffset(Velocity * DeltaSeconds);
    Locomotion = EAstroLocomotion::Flying;
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
    bOrbiting = false;
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
