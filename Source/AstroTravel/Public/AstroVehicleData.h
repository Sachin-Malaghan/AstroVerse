#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AstroVehicleData.generated.h"
// Launch vehicles as data (Content/Vehicles/DataTables/*.csv): one row per vehicle, its parts
// (meshes built by Tools/Editor/build_vehicles.py, all in one vehicle frame: metres, +Z = long
// axis, origin on the axis at the engine exits), its engines (plume windows) and its flight
// events. Adding a vehicle is a data change plus its meshes. See CLAUDE.md "Vehicles".

UENUM(BlueprintType)
enum class EAstroPlumeKind : uint8
{
    Solid,       // bright, dense (S200, Shuttle / SLS boosters)
    Kerolox,     // orange, sooty (F-1)
    Hydrolox,    // faint blue-violet (RS-25, J-2, CE-20)
    Hypergolic,  // orange-red, translucent (Vikas)
};

USTRUCT(BlueprintType)
struct FAstroVehicleRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FString DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FString Description;
    // Shown in the credits (model source and licence).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FString Credit;
    // Default pad (used unless the user has an Earth site active).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FString PadName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") double PadLatDeg = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") double PadLonDeg = 0.0;
    // Launch platform mesh ("" = the generic deck and tower); its origin is under the vehicle axis.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FString PadMesh;
    // Vehicle base (engine exits) above the ground, and the pad's yaw relative to the vehicle.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") double PadDeckHeightM = 7.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") double PadYawDeg = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") double HeightM = 60.0;
    // Time of orbit insertion (s): the ascent profile is stretched to it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") double OrbitSeconds = 540.0;
    // Where the crew vehicle meets the ship's port, and which way that port faces (vehicle frame).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FVector DockPointM = FVector(0, 0, 60);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FVector DockDirection = FVector(0, 0, 1);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") int32 SortOrder = 0;
};

USTRUCT(BlueprintType)
struct FAstroVehiclePartRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FName Vehicle;
    // Static mesh object path.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FString Mesh;
    // Parts sharing a group separate together. Conventional names: Booster, Stage1, Stage2, Tank,
    // Fairing, Escape (jettisons forward), Craft (never separates: what docks with the ship).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FName Group;
    // Mission time of separation (s); negative = stays with the craft.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") double SeparateAtS = -1.0;
    // Separation velocity relative to the vehicle, vehicle frame (m/s).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FVector SeparationPushMS = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FAstroVehicleEngineRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FName Vehicle;
    // The part group the engine belongs to (its plume goes out when the group separates).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FName Group;
    // Nozzle exit centre (vehicle frame, m) and exit radius.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FVector ExitM = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") double ExitRadiusM = 1.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") EAstroPlumeKind Kind = EAstroPlumeKind::Kerolox;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") double BurnStartS = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") double BurnEndS = 100.0;
};

USTRUCT(BlueprintType)
struct FAstroVehicleEventRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FName Vehicle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") double TimeS = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FString Text;
};

// Read access to the vehicle tables (loaded on first use).
struct ASTROTRAVEL_API FAstroVehicleCatalog
{
    // Vehicle IDs in SortOrder.
    static TArray<FName> List();
    static const FAstroVehicleRow* Find(FName Vehicle);
    static TArray<const FAstroVehiclePartRow*> Parts(FName Vehicle);
    static TArray<const FAstroVehicleEngineRow*> Engines(FName Vehicle);
    // Sorted by time.
    static TArray<const FAstroVehicleEventRow*> Events(FName Vehicle);
    static FName Default() { return TEXT("HLVM3"); }
};
