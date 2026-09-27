#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CelestialBody.generated.h"
// Base class for every simulated body. See CLAUDE.md Phase 2.
// Real orbital elements and mass live in Data Tables (Content/Bodies/DataTables),
// not hardcoded here — adding a body is a data change, not a recompile.

UCLASS(Abstract)
class ASTROBODIES_API ACelestialBody : public AActor
{
    GENERATED_BODY()

public:
    ACelestialBody();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astro|Body")
    FName BodyID; // matches a row in DT_Planets / DT_Moons / DT_Asteroids

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astro|Body")
    double MassKg = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astro|Body")
    double RadiusMeters = 0.0;

    virtual void Tick(float DeltaSeconds) override;

protected:
    // TODO Phase 2: hook into AstroCore FNBodyIntegrator via BodyRegistry
};
