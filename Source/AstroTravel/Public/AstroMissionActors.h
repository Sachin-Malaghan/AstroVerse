#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AstroMissionActors.generated.h"
// Vehicles for a crewed mission (see UAstroMissionSubsystem): a two-stage launcher with a crew
// capsule under a fairing, and a long-range ring ship waiting in orbit. Built from engine basic
// shapes and the hull / plume materials; placed every frame by the mission director (they carry
// no motion of their own). Sizes in metres, loosely a heavy-lift launcher. See CLAUDE.md
// "Missions".

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS(NotPlaceable)
class ASTROTRAVEL_API AAstroRocketActor : public AActor
{
    GENERATED_BODY()

public:
    AAstroRocketActor();
    virtual void BeginPlay() override;

    // Actor origin = base of the first stage. +Z = the rocket's long axis (nose up).
    static constexpr double Stage1Length = 42.0;
    static constexpr double Stage2Length = 13.0;
    static constexpr double TotalLength = 70.0;

    // Place the rocket (engine space). The spent first stage and fairing are placed separately.
    void SetRocketTransform(const FVector& Base, const FQuat& Rotation);
    void SetStage1Transform(const FVector& Base, const FQuat& Rotation);
    void SetFairingTransform(const FVector& Base, const FQuat& Rotation);
    // Detached pieces follow their own transforms from now on.
    void SeparateStage1();
    void SeparateFairing();
    // 0 = off. First-stage and upper-stage plumes; bVacuum widens the plume (thin air).
    void SetThrust(float Stage1Power, float Stage2Power, float VacuumFraction, double ExposureWhite);
    // Pad and tower (visible until liftoff clears them; they stay at the site).
    void SetPadTransform(const FVector& Base, const FQuat& Rotation);
    void SetPadVisible(bool bVisible);
    // Upper stage / capsule only (after docking the capsule is hidden inside the ship).
    void SetUpperVisible(bool bVisible);

private:
    UStaticMeshComponent* Part(USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh, const FVector& CentreM, const FVector& SizeM, const FLinearColor& Colour, const FRotator& Rotation = FRotator::ZeroRotator);
    UStaticMeshComponent* Plume(USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Cone, double TopM, double LengthM, double WidthM);

    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TObjectPtr<USceneComponent> Stage1Root;
    UPROPERTY() TObjectPtr<USceneComponent> UpperRoot;
    UPROPERTY() TObjectPtr<USceneComponent> FairingRoot;
    UPROPERTY() TObjectPtr<USceneComponent> PadRoot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Stage1Plume;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Stage1Core;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Stage2Plume;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
    TArray<TPair<TObjectPtr<UStaticMeshComponent>, FLinearColor>> PendingColours;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> PlumeMIDs;
    bool bStage1Separated = false;
    bool bFairingSeparated = false;
};

UCLASS(NotPlaceable)
class ASTROTRAVEL_API AAstroRingShipActor : public AActor
{
    GENERATED_BODY()

public:
    AAstroRingShipActor();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // Actor origin = ship centre; +X = forward (the docking port is at the front, engines aft).
    static constexpr double Length = 64.0;
    static constexpr double RingRadius = 30.0;
    static constexpr double DockingPortX = 34.0;

    void SetShipTransform(const FVector& Centre, const FQuat& Rotation);
    void SetEngines(float Power, double ExposureWhite);
    // The ring spins about the ship's axis for artificial gravity (rpm).
    void SetRingRpm(float Rpm) { RingRpm = Rpm; }

private:
    UStaticMeshComponent* Part(USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh, const FVector& CentreM, const FVector& SizeM, const FLinearColor& Colour, const FRotator& Rotation = FRotator::ZeroRotator);

    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TObjectPtr<USceneComponent> RingRoot;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> EnginePlumes;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> PlumeMIDs;
    TArray<TPair<TObjectPtr<UStaticMeshComponent>, FLinearColor>> PendingColours;
    float RingRpm = 4.0f;
    float RingAngle = 0.0f;
};
