#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AstroMissionActors.generated.h"
// Vehicles for a crewed mission (see UAstroMissionSubsystem): a launch vehicle built from its
// data rows and meshes (AstroVehicleData.h: HLVM3 / Gaganyaan, Saturn V, Space Shuttle, SLS),
// and the long-range ring ship waiting in orbit (engine basic shapes). Placed every frame by the
// mission director (they carry no motion of their own). See CLAUDE.md "Missions" and "Vehicles".

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;

UCLASS(NotPlaceable)
class ASTROTRAVEL_API AAstroRocketActor : public AActor
{
    GENERATED_BODY()

public:
    AAstroRocketActor();
    virtual void BeginPlay() override;

    // Build the vehicle from its data rows (FAstroVehicleCatalog). False if it has no parts.
    // Actor origin = the vehicle frame origin (on the axis at the engine exits); +Z = nose.
    bool BuildVehicle(FName Vehicle);

    // Separable part groups (in data order). A group's root is at the vehicle origin, so moving
    // it moves its parts exactly as if the whole vehicle had moved.
    int32 NumGroups() const { return Groups.Num(); }
    FName GroupName(int32 Index) const { return Groups[Index].Name; }
    double GroupSeparateAt(int32 Index) const { return Groups[Index].SeparateAt; }
    FVector GroupPushMS(int32 Index) const { return Groups[Index].PushMS; }
    bool IsGroupSeparated(int32 Index) const { return Groups[Index].bSeparated; }
    void SeparateGroup(int32 Index);
    void SetGroupTransform(int32 Index, const FVector& Base, const FQuat& Rotation);
    void SetGroupVisible(int32 Index, bool bVisible);
    // Centre of the parts that never separate (the crewed craft), vehicle frame (m).
    FVector GetCraftCentreM() const { return CraftCentreM; }
    // Earliest separation time (end of the first stage, for piloted staging), or -1.
    double GetFirstSeparation() const;

    void SetRocketTransform(const FVector& Base, const FQuat& Rotation);
    // Engine plumes by their burn windows at mission time T; Throttle scales them (0 = off).
    void SetThrust(double MissionTime, float Throttle, float VacuumFraction, double ExposureWhite);
    // Piloted ascent: engines burning before the first separation at Stage1Power, the rest at Stage2Power.
    void SetThrustByStage(float Stage1Power, float Stage2Power, float VacuumFraction, double ExposureWhite);
    // Pad (stays at the site).
    void SetPadTransform(const FVector& Base, const FQuat& Rotation);
    void SetPadVisible(bool bVisible);
    // The crewed craft (groups that never separate).
    void SetUpperVisible(bool bVisible);

private:
    struct FGroup
    {
        FName Name;
        TObjectPtr<USceneComponent> Root;
        double SeparateAt = -1.0;
        FVector PushMS = FVector::ZeroVector;
        bool bSeparated = false;
    };
    struct FPlume
    {
        int32 Group = INDEX_NONE;
        uint8 Kind = 0;
        double Start = 0.0, End = 0.0, RadiusM = 1.0;
        FVector ExitM = FVector::ZeroVector;
        bool bOn = false;
        TObjectPtr<UStaticMeshComponent> Outer;
        TObjectPtr<UStaticMeshComponent> Core;
        TObjectPtr<UMaterialInstanceDynamic> OuterMID;
        TObjectPtr<UMaterialInstanceDynamic> CoreMID;
    };
    int32 FindOrAddGroup(FName Name, double SeparateAt, const FVector& Push);
    UStaticMeshComponent* NewPart(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& CentreM, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator);
    void BuildGenericPad(double DeckHeightM);
    void ApplyPlume(FPlume& P, float Power, float VacuumFraction, double ExposureWhite);
    // Visibility changes propagate to child plumes; put each back to whether it is burning.
    void RestorePlumes();

    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TObjectPtr<USceneComponent> PadRoot;
    UPROPERTY() TArray<TObjectPtr<UObject>> Keep; // components and MIDs created at runtime (GC roots)
    TArray<FGroup> Groups;
    TArray<FPlume> Plumes;
    TArray<TPair<TObjectPtr<UStaticMeshComponent>, FLinearColor>> PendingColours;
    FVector CraftCentreM = FVector::ZeroVector;
};

UCLASS(NotPlaceable)
class ASTROTRAVEL_API AAstroRingShipActor : public AActor
{
    GENERATED_BODY()

public:
    AAstroRingShipActor();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // The ring station-ship Odyssey (Tools/Editor/build_station.py). Actor origin = the ring's
    // centre; +X = forward (docking node and front port), engines aft. Metres.
    static constexpr double Length = 420.0;
    static constexpr double RingRadius = 170.0;     // outer hull
    static constexpr double DockingPortX = 146.0;   // front docking port face

    void SetShipTransform(const FVector& Centre, const FQuat& Rotation);
    // Main engines (0..1) and the current exposure white point, which also drives the window,
    // navigation-light and strobe brightness.
    void SetEngines(float Power, double ExposureWhite);
    // The habitat ring spins about the ship's axis for artificial gravity (rpm; 2.36 = 1 g at 160 m).
    void SetRingRpm(float Rpm) { RingRpm = Rpm; }

private:
    struct FNavLight
    {
        TObjectPtr<UStaticMeshComponent> Halo;
        TObjectPtr<UMaterialInstanceDynamic> MID;
        FLinearColor Colour;
        float Brightness = 1.0f;   // x exposure white
        uint8 Pattern = 0;         // 0 steady, 1 strobe (double flash), 2 slow blink
        float Phase = 0.0f;
    };
    UStaticMeshComponent* MeshPart(USceneComponent* Parent, const TCHAR* Name, const TCHAR* Path);
    void AddNavLight(USceneComponent* Parent, const TCHAR* Name, const FVector& PosM, float SizeM, const FLinearColor& Colour, float Brightness, uint8 Pattern, float Phase);
    void AddFlood(const TCHAR* Name, const FVector& PosM, const FRotator& Aim);

    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TObjectPtr<USceneComponent> RingRoot;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> WindowMeshes;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> WindowMIDs;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> EnginePlumes;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> PlumeMIDs;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> NavHalos;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> NavMIDs;
    TArray<FNavLight> NavLights;
    float RingRpm = 2.36f;
    float RingAngle = 0.0f;
    float Clock = 0.0f;
    double White = 1.0;
};
