// See CLAUDE.md "Real-world Earth tiles".
#include "AstroEarthTiles.h"
#include "AstroBody.h"
#include "AstroGeodesy.h"
#include "AstroRenderingSubsystem.h"
#include "AstroSimulationSubsystem.h"
#include "AstroTerrainActor.h"
#include "BodyTerrain.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroEarthTiles, Log, All);

namespace
{
    UClass* FindCesiumClass(const TCHAR* Name)
    {
        return FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/CesiumRuntime.%s"), Name));
    }

    // Reflection helpers: the plugin is optional, so no headers or link dependency.
    bool SetStringProperty(UObject* Object, const TCHAR* Name, const FString& Value)
    {
        FStrProperty* Prop = FindFProperty<FStrProperty>(Object->GetClass(), Name);
        if (!Prop) { return false; }
        Prop->SetPropertyValue_InContainer(Object, Value);
        return true;
    }

    bool SetInt64Property(UObject* Object, const TCHAR* Name, int64 Value)
    {
        FInt64Property* Prop = FindFProperty<FInt64Property>(Object->GetClass(), Name);
        if (!Prop) { return false; }
        Prop->SetPropertyValue_InContainer(Object, Value);
        return true;
    }

    bool SetEnumProperty(UObject* Object, const TCHAR* Name, const TCHAR* EnumeratorName)
    {
        FEnumProperty* Prop = FindFProperty<FEnumProperty>(Object->GetClass(), Name);
        if (!Prop) { return false; }
        const int64 Value = Prop->GetEnum()->GetValueByNameString(EnumeratorName);
        if (Value == INDEX_NONE) { return false; }
        Prop->GetUnderlyingProperty()->SetIntPropertyValue(Prop->ContainerPtrToValuePtr<void>(Object), Value);
        return true;
    }

    bool CallNoArgs(UObject* Object, const TCHAR* FunctionName)
    {
        UFunction* Function = Object->FindFunction(FunctionName);
        if (!Function) { return false; }
        Object->ProcessEvent(Function, nullptr);
        return true;
    }

    FAutoConsoleCommandWithWorld GAstroCmdEarthTilesStatus(TEXT("astro.EarthTiles.Status"), TEXT("Print the real-world Earth tiles bridge status"),
        FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
        {
            if (const UAstroEarthTilesSubsystem* Tiles = UAstroEarthTilesSubsystem::Get(World))
            {
                UE_LOG(LogAstroEarthTiles, Display, TEXT("Earth tiles: %s (Cesium for Unreal %s)"), *Tiles->GetStatus(),
                    UAstroEarthTilesSubsystem::IsCesiumAvailable() ? TEXT("installed") : TEXT("not installed"));
            }
        }));
}

bool UAstroEarthTilesSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

UAstroEarthTilesSubsystem* UAstroEarthTilesSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UAstroEarthTilesSubsystem>() : nullptr;
}

TStatId UAstroEarthTilesSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UAstroEarthTilesSubsystem, STATGROUP_Tickables);
}

bool UAstroEarthTilesSubsystem::IsCesiumAvailable()
{
    return FindCesiumClass(TEXT("CesiumGeoreference")) && FindCesiumClass(TEXT("Cesium3DTileset"));
}

void UAstroEarthTilesSubsystem::Tick(float DeltaTime)
{
    const UAstroEarthTilesSettings* Settings = UAstroEarthTilesSettings::Get();
    const UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this);
    if (Settings->Source == EAstroEarthTileSource::Off || !Sim || !Sim->IsReady())
    {
        Status = TEXT("off (Earth tiles source not set)");
        Deactivate();
        return;
    }
    if (!IsCesiumAvailable())
    {
        Status = TEXT("unavailable: install and enable the Cesium for Unreal plugin");
        return;
    }
    const bool bGoogle = Settings->Source == EAstroEarthTileSource::GooglePhotorealistic;
    if ((bGoogle && Settings->GoogleMapsApiKey.IsEmpty()) || (!bGoogle && Settings->CesiumIonAccessToken.IsEmpty()))
    {
        Status = bGoogle ? TEXT("needs a Google Maps Platform API key") : TEXT("needs a Cesium ion access token");
        return;
    }
    // Only in Earth's co-rotating (landed / low) frame, whose axes match Cesium's local ENU.
    const int32 Earth = Sim->FindBodyIndex(TEXT("Earth"));
    if (Earth == INDEX_NONE || !Sim->IsRotatingFrame() || Sim->GetRenderOriginAnchorBody() != Earth)
    {
        Status = TEXT("waiting: descend close to Earth");
        Deactivate();
        return;
    }
    const FBodyDefinition& Def = Sim->GetRegistry().Get(Earth);
    const FAstroVector3d BodyFixed = Def.GetOrientationAt(Sim->GetSimulation().GetSimSeconds()).Transposed()
                                   * (Sim->GetRenderOrigin() - Sim->GetSimulation().GetBodyState(Earth).Position);
    const FAstroVector3d Dir = BodyFixed.Normalized();
    const double Lat = AstroGeodesy::LatitudeDeg(Dir, Def.EquatorialRadiusMeters, Def.PolarRadiusMeters, true);
    const double Lon = AstroGeodesy::LongitudeDeg(Dir);
    const double Ellipsoid = Def.Terrain.IsValid() ? Def.Terrain->EllipsoidRadius(Dir) : Def.EquatorialRadiusMeters;
    const double Height = BodyFixed.Length() - Ellipsoid;
    if (Height > Settings->ActivateBelowKm * 1000.0 * (bActive ? 1.1 : 1.0))
    {
        Status = FString::Printf(TEXT("waiting: below %.0f km over Earth"), Settings->ActivateBelowKm);
        Deactivate();
        return;
    }
    if (!bActive)
    {
        Activate(Lat, Lon, Height);
    }
    // Follow the floating origin: the georeference origin is always our render origin.
    if (FMath::Abs(Lat - LastLat) > 1e-7 || FMath::Abs(Lon - LastLon) > 1e-7 || FMath::Abs(Height - LastHeight) > 0.01)
    {
        SetGeoreferenceOrigin(Lat, Lon, Height);
    }
    Status = FString::Printf(TEXT("active at %.5f, %.5f (%s)"), Lat, Lon, bGoogle ? TEXT("Google Photorealistic 3D Tiles") : TEXT("Cesium ion"));
}

void UAstroEarthTilesSubsystem::SetGeoreferenceOrigin(double LatDeg, double LonDeg, double HeightM)
{
    AActor* Geo = Georeference.Get();
    UFunction* Function = Geo ? Geo->FindFunction(TEXT("SetOriginLongitudeLatitudeHeight")) : nullptr;
    if (!Function)
    {
        return;
    }
    // Parameter struct: one FVector (longitude, latitude, height).
    TArray<uint8> Params;
    Params.SetNumZeroed(Function->ParmsSize);
    if (FStructProperty* Arg = CastField<FStructProperty>(Function->PropertyLink))
    {
        *Arg->ContainerPtrToValuePtr<FVector>(Params.GetData()) = FVector(LonDeg, LatDeg, HeightM);
        Geo->ProcessEvent(Function, Params.GetData());
    }
    LastLat = LatDeg;
    LastLon = LonDeg;
    LastHeight = HeightM;
}

void UAstroEarthTilesSubsystem::Activate(double LatDeg, double LonDeg, double HeightM)
{
    UWorld* World = GetWorld();
    const UAstroEarthTilesSettings* Settings = UAstroEarthTilesSettings::Get();
    FActorSpawnParameters Params;
    Params.Name = TEXT("AstroEarthGeoreference");
    Georeference = World->SpawnActor<AActor>(FindCesiumClass(TEXT("CesiumGeoreference")), Params);
    Params.Name = TEXT("AstroEarthTileset");
    Tileset = World->SpawnActor<AActor>(FindCesiumClass(TEXT("Cesium3DTileset")), Params);
    if (!Georeference.IsValid() || !Tileset.IsValid())
    {
        Status = TEXT("failed to spawn Cesium actors");
        Deactivate();
        return;
    }
    Georeference->Tags.Add(TEXT("AstroSolarSystem"));
    Tileset->Tags.Add(TEXT("AstroSolarSystem"));
    SetGeoreferenceOrigin(LatDeg, LonDeg, HeightM);
    UObject* Set = Tileset.Get();
    bool bOk;
    if (Settings->Source == EAstroEarthTileSource::GooglePhotorealistic)
    {
        bOk = SetEnumProperty(Set, TEXT("TilesetSource"), TEXT("FromUrl"))
           && SetStringProperty(Set, TEXT("Url"), TEXT("https://tile.googleapis.com/v1/3dtiles/root.json?key=") + Settings->GoogleMapsApiKey);
    }
    else
    {
        bOk = SetEnumProperty(Set, TEXT("TilesetSource"), TEXT("FromCesiumIon"))
           && SetInt64Property(Set, TEXT("IonAssetID"), Settings->CesiumIonAssetId)
           && SetStringProperty(Set, TEXT("IonAccessToken"), Settings->CesiumIonAccessToken);
    }
    if (!bOk)
    {
        UE_LOG(LogAstroEarthTiles, Warning, TEXT("Cesium3DTileset properties not found - plugin version mismatch?"));
    }
    CallNoArgs(Set, TEXT("RefreshTileset"));

    // Our own close-range Earth surface steps aside while the real one is shown.
    if (UAstroRenderingSubsystem* Rendering = UAstroRenderingSubsystem::Get(this); Rendering && Rendering->GetTerrain())
    {
        Rendering->GetTerrain()->SetActorHiddenInGame(true);
    }
    if (UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this))
    {
        if (AAstroBody* Earth = Sim->GetBodyActor(Sim->FindBodyIndex(TEXT("Earth"))))
        {
            Earth->SetActorHiddenInGame(true);
        }
    }
    bActive = true;
    UE_LOG(LogAstroEarthTiles, Display, TEXT("Earth tiles on at %.5f, %.5f"), LatDeg, LonDeg);
}

void UAstroEarthTilesSubsystem::Deactivate()
{
    if (!bActive && !Georeference.IsValid() && !Tileset.IsValid())
    {
        return;
    }
    if (Tileset.IsValid()) { Tileset->Destroy(); }
    if (Georeference.IsValid()) { Georeference->Destroy(); }
    Tileset.Reset();
    Georeference.Reset();
    if (UAstroRenderingSubsystem* Rendering = UAstroRenderingSubsystem::Get(this); Rendering && Rendering->GetTerrain())
    {
        Rendering->GetTerrain()->SetActorHiddenInGame(false);
    }
    if (UAstroSimulationSubsystem* Sim = UAstroSimulationSubsystem::Get(this))
    {
        if (AAstroBody* Earth = Sim->GetBodyActor(Sim->FindBodyIndex(TEXT("Earth"))))
        {
            Earth->SetActorHiddenInGame(false);
        }
    }
    LastLat = LastLon = LastHeight = 1e9;
    bActive = false;
}
