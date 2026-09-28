#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "AstroEarthTiles.generated.h"
// Real-world 3D map close to Earth's surface: Google Photorealistic 3D Tiles (the 3D cities
// and terrain Google Earth shows, licensed through Google Maps Platform) or Cesium ion
// streams, drawn by the Cesium for Unreal plugin. AstroVerse keeps no hard dependency on the
// plugin: the bridge finds its classes by reflection at runtime and stays inactive (our own
// DEM terrain is used) when the plugin or an API key is missing. Below the activation
// altitude over Earth it spawns a Cesium georeference at our render origin - our landed
// frame already uses Cesium's axes (+X east, +Y south, +Z up) - keeps it on our floating
// origin as that rebases, and hides AstroVerse's own Earth surface there. See CLAUDE.md
// "Real-world Earth tiles" and SETUP.md.

UENUM()
enum class EAstroEarthTileSource : uint8
{
    Off,
    // Google Maps Platform Map Tiles API (needs a Google Cloud API key with the Map Tiles API enabled).
    GooglePhotorealistic,
    // Any Cesium ion 3D Tiles asset (e.g. Cesium World Terrain + imagery, or Google tiles via ion).
    CesiumIon,
};

// Per-user settings (Saved/Config/<Platform>/GameUserSettings.ini): API keys never go in git.
UCLASS(Config = GameUserSettings, meta = (DisplayName = "Astro Earth Tiles"))
class ASTRORENDERING_API UAstroEarthTilesSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category = "Earth Tiles")
    EAstroEarthTileSource Source = EAstroEarthTileSource::Off;

    UPROPERTY(Config, EditAnywhere, Category = "Earth Tiles")
    FString GoogleMapsApiKey;

    UPROPERTY(Config, EditAnywhere, Category = "Earth Tiles")
    FString CesiumIonAccessToken;

    // Cesium ion asset ID (2275207 = Google Photorealistic 3D Tiles on ion; 1 = Cesium World Terrain).
    UPROPERTY(Config, EditAnywhere, Category = "Earth Tiles")
    int64 CesiumIonAssetId = 2275207;

    // Switch to the tiles below this height above Earth's surface (km).
    UPROPERTY(Config, EditAnywhere, Category = "Earth Tiles", meta = (ClampMin = "1"))
    double ActivateBelowKm = 50.0;

    static UAstroEarthTilesSettings* Get() { return GetMutableDefault<UAstroEarthTilesSettings>(); }
};

UCLASS()
class ASTRORENDERING_API UAstroEarthTilesSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    static UAstroEarthTilesSubsystem* Get(const UObject* WorldContext);

    // Why the tiles are / aren't showing (for the HUD and logs).
    const FString& GetStatus() const { return Status; }
    bool IsActive() const { return bActive; }
    static bool IsCesiumAvailable();

private:
    void Activate(double LatDeg, double LonDeg, double HeightM);
    void Deactivate();
    void SetGeoreferenceOrigin(double LatDeg, double LonDeg, double HeightM);

    TWeakObjectPtr<AActor> Georeference;
    TWeakObjectPtr<AActor> Tileset;
    FString Status = TEXT("off");
    bool bActive = false;
    double LastLat = 1e9, LastLon = 1e9, LastHeight = 1e9;
};
