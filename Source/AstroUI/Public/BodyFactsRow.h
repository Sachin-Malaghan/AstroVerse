#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BodyFactsRow.generated.h"
// Teaching-mode text for each body (Content/UI/DataTables/DT_Facts.csv). Numbers that the
// simulation already knows (radius, mass, gravity, day/year length, distances) are derived
// live instead of duplicated here. See CLAUDE.md Phase 11.

USTRUCT(BlueprintType)
struct ASTROUI_API FAstroBodyFactsRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facts")
    FText Summary;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facts")
    double MeanSurfaceTemperatureK = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facts")
    FText AtmosphereText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facts")
    FText NamedAfter;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facts")
    FText Discovery;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facts")
    FText FunFact;
};
