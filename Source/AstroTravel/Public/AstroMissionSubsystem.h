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
    bool Launch(FName Destination, double LatDeg, double LonDeg, const FString& SiteName);
    void Abort();

    // True while the director drives the camera (the pawn stands down).
    bool IsControllingCamera() const;
    const FAstroMissionStatus& GetStatus() const { return Status; }

    static constexpr const TCHAR* ShipName = TEXT("Odyssey");

private:
    // Earth body-fixed trajectory point (m) at mission time T (s), and the flight direction.
    FAstroVector3d TrajectoryBF(double T, double* OutAltitude = nullptr, double* OutDownrange = nullptr) const;
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
    bool bFairingSeparated = false;
    int32 NextEvent = 0;
    double SeparationTime = 0.0, FairingTime = 0.0;
    FAstroVector3d SeparationBF, SeparationVelBF, FairingBF, FairingVelBF;

    // Parked ship: offset from the destination body (sim frame) and orientation.
    FAstroVector3d ParkedOffset;
    FQuat ParkedSimRotation = FQuat::Identity;
    FAstroMatrix3d ParkedBasis;
};
