#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AstroSolarGeometry.h"
#include "AstroSiteSubsystem.generated.h"
// "Sun at a site": a place on a body (GPS-style latitude / longitude, a UTC offset for local
// time) with its real sky - where the Sun is now, today's sunrise / solar noon / sunset, the
// shadow of a 1 m stick, and the Sun's paths for today, both solstices and the equinox drawn
// over a compass rose with the eight vastu directions. For building orientation (vastu),
// sun hours on a field (crop planning), solar-panel siting and teaching. Driven by the live
// simulation clock, so scrubbing time moves everything. See CLAUDE.md "Sun at a site".

class AAstroSiteActor;

struct FAstroSiteReport
{
    bool bValid = false;
    FName Body;
    FString Name;
    double LatDeg = 0.0, LonDeg = 0.0, UtcOffsetHours = 0.0;
    double ElevationM = 0.0;
    bool bRealElevation = false;
    FAstroSunPosition Sun;
    FAstroSunDay Today;
    double LocalDayStartSim = 0.0;
    double ShadowLengthPerMeter = 0.0; // of a vertical 1 m stick (0 when the Sun is down)
    double ShadowAzimuthDeg = 0.0;
};

// A label the HUD draws at a world position (compass points, hours along the paths).
struct FAstroSiteLabel
{
    FVector ENUCm; // offset (cm, east / north / up) from the ground point, or the viewer if bSky
    FString Text;
    FLinearColor Color;
    bool bSky = false;

    FAstroSiteLabel(const FVector& InENU, const FString& InText, const FLinearColor& InColor, bool bInSky)
        : ENUCm(InENU), Text(InText), Color(InColor), bSky(bInSky) {}
};

UCLASS()
class ASTRORENDERING_API UAstroSiteSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    static UAstroSiteSubsystem* Get(const UObject* WorldContext);

    void SetSite(FName Body, double LatDeg, double LonDeg, double UtcOffsetHours, const FString& Name);
    void ClearSite();
    bool HasSite() const { return Report.bValid; }
    const FAstroSiteReport& GetReport() const { return Report; }
    // Labels to draw now (none while the viewer is away from the site).
    const TArray<FAstroSiteLabel>& GetLabels() const { static const TArray<FAstroSiteLabel> None; return bNearSite ? Labels : None; }
    // Current engine position of a label (follows the frame every tick).
    FVector LabelWorldPosition(const FAstroSiteLabel& Label) const;

    // Site's ground point and local axes in engine space (for placing the viewer).
    bool GetSiteFrame(FVector& OutGround, FVector& OutEast, FVector& OutNorth, FVector& OutUp) const;

    void SetPathsVisible(bool bVisible);
    bool ArePathsVisible() const { return bPathsVisible; }

    // 8-point compass name with the vastu direction (e.g. "NE - Ishanya").
    static FString CompassName(double AzimuthDeg, bool bVastu);

private:
    void Recompute();
    TUniquePtr<FAstroSolarGeometry> MakeGeometry() const;

    FAstroSiteReport Report;
    TArray<FAstroSiteLabel> Labels;
    TWeakObjectPtr<AAstroSiteActor> Actor;
    int32 BodyIndex = INDEX_NONE;
    FAstroVector3d SiteBF, UpBF, EastBF, NorthBF;
    double LastPathDayStart = -1e30;
    double RecomputeTimer = 0.0;
    bool bPathsVisible = true;
    bool bNearSite = false;
    // Sun paths (ENU unit vectors) with their hour marks, rebuilt once per local day.
    TArray<TArray<FAstroVector3d>> Paths;
    TArray<TArray<TPair<FAstroVector3d, FString>>> HourMarks;
};
