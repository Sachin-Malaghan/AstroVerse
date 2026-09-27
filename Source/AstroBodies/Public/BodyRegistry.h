#pragma once
#include "CoreMinimal.h"
// Data-driven registry: loads real orbital elements/masses from
// Content/Bodies/DataTables and spawns/owns the corresponding
// ACelestialBody actors. See CLAUDE.md Phase 2.

class ASTROBODIES_API FBodyRegistry
{
public:
    void LoadFromDataTables();
    // TODO Phase 2: DT_Planets.csv, DT_Moons.csv, DT_Asteroids.csv
};
