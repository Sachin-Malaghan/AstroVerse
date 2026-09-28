#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Math/AstroVector3d.h"
#include "Math/AstroMatrix3d.h"
#include "AstroMissionSubsystem.generated.h"
// A crewed mission, start to finish (decided 2026-09-28): countdown on a real launch pad,
// a two-stage ascent along a gravity-turn profile to a 200 km orbit (compressed in time:
// the first seconds run at 1x, then ~9x), rendezvous and docking with a ring ship waiting in
// orbit, crew transfer, then the ship flies the real-time path to the destination with the
// camera chasing it, and parks in orbit there. The director owns the camera until departure,
// then hands it to UAstroTravelSubsystem. All positions are Earth body-fixed during launch,
// so the pad stays put while Earth turns. See CLAUDE.md "Missions".

class AAstroRocketActor;
class AAstroRingShipActor;
class UAstroSimulationSubsystem;

UENUM()
enum class EAstroMissionPhase : uint8
{
    None,
    Countdown,
    Ascent,
    Coast,
    Rendezvous,
    Docked,
    Departure,
    Parked,
    Failed,
};

struct FAstroMissionStatus
{
    bool bActive = false;
    EAstroMissionPhase Phase = EAstroMissionPhase::None;
    FString Title;       // "Mission to Mars - ship Odyssey"
    FString PhaseText;   // "Ascent - second stage burn"
    FString EventText;   // latest event ("Max-Q", "Stage separation"...)
    double MissionSeconds = 0.0; // T+ (negative during the countdown)
    double AltitudeKm = 0.0;
    double SpeedKmS = 0.0;
    double DownrangeKm = 0.0;
    bool bPiloted = false;
    FString PilotText;   // telemetry + controls while you fly it
    bool bAwaitingDestination = false; // docked in Earth orbit, waiting for ChooseDestination
    FName Vehicle;
    FString VehicleName;
    FName Destination;   // None until chosen in orbit
};

UCLASS()
class ASTROTRAVEL_API UAstroMissionSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    static UAstroMissionSubsystem* Get(const UObject* WorldContext);

    // Launch from (geodetic lat, east lon) on Earth to Destination. SiteName is for the HUD.
    // bPiloted: the autopilot flies the ascent, you fly the docking (RCS) and the cruise; astro.Mission.PilotAscent 1 hands you the ascent too.
    // Vehicle: a row of DT_Vehicles (None = astro.Mission.Vehicle, default HLVM3).
    bool Launch(FName Destination, double LatDeg, double LonDeg, const FString& SiteName, bool bPiloted = false, FName Vehicle = NAME_None);

    // Pilot controls, fed by the pawn every frame: Move = (forward/back, right/left, up/down),
    // Look = mouse drag, bBoost = Shift.
    void SetPilotInput(const FVector& Move, const FVector2D& Look, bool bBoost);
    // Hand the docking to the autopilot (N).
    void RequestAutoDock() { bAutoDock = true; }
    // The destination is decided in Earth orbit, aboard the ship (Launch may pass None).
    bool ChooseDestination(FName Body);
    // Scripted tests: a held control input that overrides the pawn's (zero = off).
    void SetDebugPilotInput(const FVector& Move) { DebugMove = Move; }
    FVector DebugMove = FVector::ZeroVector;
    bool IsPiloting() const;
    void Abort();

    // True while the director drives the camera (the pawn stands down).
    bool IsControllingCamera() const;
    const FAstroMissionStatus& GetStatus() const { return Status; }

    static constexpr const TCHAR* ShipName = TEXT("Odyssey");

private:
    // Earth body-fixed trajectory point (m) at mission time T (s), and the flight direction.
    FAstroVector3d TrajectoryBF(double T, double* OutAltitude = nullptr, double* OutDownrange = nullptr) const;
    FAstroVector3d PointBF(double DownrangeM, double AltitudeM) const;
    void StepPilotAscent(double RealDt);
    void StepPilotDocking(double RealDt);
    void Fail(const FString& Why);
    FAstroVector3d BodyFixedToSim(const UAstroSimulationSubsystem* Sim, const FAstroVector3d& BF) const;
    FAstroVector3d BodyFixedDirToSim(const UAstroSimulationSubsystem* Sim, const FAstroVector3d& Dir) const;
    FVector ToEngine(const UAstroSimulationSubsystem* Sim, const FAstroVector3d& BF) const;
    FQuat EngineRotation(const UAstroSimulationSubsystem* Sim, const FAstroVector3d& AxisBF, const FAstroVector3d& UpHintBF, bool bAxisIsZ) const;
    void PlaceCamera(UAstroSimulationSubsystem* Sim, const FAstroVector3d& CamBF, const FAstroVector3d& LookBF);
    double ExposureWhite(const UAstroSimulationSubsystem* Sim) const;
    void SetEvent(const FString& Text);
    void BeginDeparture();
    static double AstroRingShipDockOffset();
    void UpdateShipRidingAlong();
    UFUNCTION() void HandleArrived(FName BodyID);

    TWeakObjectPtr<AAstroRocketActor> Rocket;
    TWeakObjectPtr<AAstroRingShipActor> Ship;
    // Earthshine: sunlight reflected by the Earth below (albedo 0.3), lighting the vehicles'
    // Earth-facing sides in low orbit. Only affects lit meshes (bodies use their own shading).
    TWeakObjectPtr<class ADirectionalLight> Earthshine;
    void UpdateEarthshine(const UAstroSimulationSubsystem* Sim, const FAstroVector3d& UpBF, double Altitude, bool bOn);
    FAstroMissionStatus Status;
    FName Destination;
    int32 Earth = INDEX_NONE;

    // Site (Earth body-fixed): radial up, east, north; ground radius at the pad.
    FAstroVector3d SiteUp, SiteEast, SiteNorth;
    double GroundRadius = 6378137.0;

    double PhaseSeconds = 0.0;  // real seconds in the current phase
    double MissionTime = 0.0;   // T+ (s), advanced at the compressed rate during ascent
    double EventTimer = 0.0;
    double RealSinceLiftoff = 0.0;
    bool bStageSeparated = false;
    int32 NextEvent = 0;

    // The launch vehicle (AstroVehicleData.h), resolved at launch.
    FName VehicleId;
    double VehicleHeight = 60.0;
    double VehicleSECO = 540.0;   // orbit insertion (s)
    double ProfileScale = 1.0;    // ascent profile time stretch (VehicleSECO / the profile's 540 s)
    double DeckHeight = 7.0;      // vehicle base above the ground on its pad
    FVector DockPointM = FVector(0, 0, 60);
    FVector DockDirection = FVector(0, 0, 1);
    FVector CraftCentreM = FVector(0, 0, 50);
    TArray<TPair<double, FString>> Events;
    struct FGroupSeparation
    {
        double Time = 0.0;
        FAstroVector3d Base, Velocity, Push;
        FQuat Rotation = FQuat::Identity;
    };
    TArray<FGroupSeparation> GroupSep;

    // Orbit reached (autopilot: the profile's SECO; piloted: wherever you made orbit).
    double OrbitX0 = 0.0, OrbitH = 0.0, OrbitGround = 0.0, OrbitT0 = 0.0;

    // Piloted flight state.
    bool bPiloted = false;
    bool bPilotAscent = false;
    FString LaunchSiteName;
    FVector PilotMove = FVector::ZeroVector;
    FVector2D PilotLook = FVector2D::ZeroVector;
    bool bPilotBoost = false;
    double PX = 0.0, PH = 0.0, PVh = 0.0, PVx = 0.0; // downrange (m), altitude (m), vertical / inertial horizontal speed (m/s)
    double PThrottle = 0.0, PPitch = 0.0;            // 0..1, degrees from vertical
    int32 PStage = 1;
    double PUsed1 = 0.0, PUsed2 = 0.0, PIgniteTimer = 0.0;
    bool bPIgnited2 = false, bPLiftoff = false, bPMaxQ = false;
    double CosLat = 1.0;
    // Docking: capsule offset from the orbit point (along-track, side, up) and velocity.
    FVector DockR = FVector::ZeroVector, DockV = FVector::ZeroVector;
    bool bAutoDock = false;
    static constexpr double PilotShipAhead = 300.0;

    // Parked ship: offset from the destination body (sim frame) and orientation.
    FAstroVector3d ParkedOffset;
    FQuat ParkedSimRotation = FQuat::Identity;
    FAstroMatrix3d ParkedBasis;
};
