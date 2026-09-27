#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TourStopRow.generated.h"
// One stop of the guided tour (Content/UI/DataTables/DT_Tour.csv; rows play in row-name
// order). Teachers can reorder, re-word or add stops without touching code. See CLAUDE.md
// "Guided tour".

UENUM(BlueprintType)
enum class EAstroTourStopMode : uint8
{
    // Warp to Body (cinematic), then orbit it at DistanceRadii.
    Travel,
    // No warp: the camera flies/zooms to orbit Body at DistanceRadii from the given side.
    Orbit,
    // Switch to the Milky Way scale-domain (the Sun marker is shown).
    Galaxy,
};

USTRUCT(BlueprintType)
struct FAstroTourStopRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour")
    EAstroTourStopMode Mode = EAstroTourStopMode::Travel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour")
    FName Body;

    // Camera distance from the body's centre, in equatorial radii.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour")
    double DistanceRadii = 3.5;

    // Orbit mode only: Sun-body-camera angle and height above the body's equator (deg).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour")
    double PhaseDeg = 45.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour")
    double ElevationDeg = 15.0;

    // Real seconds to stay after arriving (N skips ahead).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour")
    double HoldSeconds = 20.0;

    // Slow automatic orbit while holding (deg/s).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour")
    double OrbitDriftDegPerSecond = 3.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour")
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tour")
    FString Narration;
};
