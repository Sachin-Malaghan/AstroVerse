// See CLAUDE.md "Vehicles".
#include "AstroVehicleData.h"

namespace
{
    template <typename RowType>
    TArray<const RowType*> AllRows(const TCHAR* Path)
    {
        TArray<const RowType*> Out;
        if (const UDataTable* Table = LoadObject<UDataTable>(nullptr, Path))
        {
            for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
            {
                Out.Add(reinterpret_cast<const RowType*>(Pair.Value));
            }
        }
        return Out;
    }

    const UDataTable* Vehicles()
    {
        return LoadObject<UDataTable>(nullptr, TEXT("/Game/Vehicles/DataTables/DT_Vehicles.DT_Vehicles"));
    }
}

TArray<FName> FAstroVehicleCatalog::List()
{
    TArray<FName> Names;
    if (const UDataTable* Table = Vehicles())
    {
        Names = Table->GetRowNames();
        Names.Sort([Table](const FName& A, const FName& B)
        {
            return Table->FindRow<FAstroVehicleRow>(A, TEXT(""))->SortOrder < Table->FindRow<FAstroVehicleRow>(B, TEXT(""))->SortOrder;
        });
    }
    return Names;
}

const FAstroVehicleRow* FAstroVehicleCatalog::Find(FName Vehicle)
{
    const UDataTable* Table = Vehicles();
    return Table ? Table->FindRow<FAstroVehicleRow>(Vehicle, TEXT("AstroVehicleCatalog"), false) : nullptr;
}

TArray<const FAstroVehiclePartRow*> FAstroVehicleCatalog::Parts(FName Vehicle)
{
    TArray<const FAstroVehiclePartRow*> Rows = AllRows<FAstroVehiclePartRow>(TEXT("/Game/Vehicles/DataTables/DT_VehicleParts.DT_VehicleParts"));
    Rows.RemoveAll([Vehicle](const FAstroVehiclePartRow* R) { return R->Vehicle != Vehicle; });
    return Rows;
}

TArray<const FAstroVehicleEngineRow*> FAstroVehicleCatalog::Engines(FName Vehicle)
{
    TArray<const FAstroVehicleEngineRow*> Rows = AllRows<FAstroVehicleEngineRow>(TEXT("/Game/Vehicles/DataTables/DT_VehicleEngines.DT_VehicleEngines"));
    Rows.RemoveAll([Vehicle](const FAstroVehicleEngineRow* R) { return R->Vehicle != Vehicle; });
    return Rows;
}

TArray<const FAstroVehicleEventRow*> FAstroVehicleCatalog::Events(FName Vehicle)
{
    TArray<const FAstroVehicleEventRow*> Rows = AllRows<FAstroVehicleEventRow>(TEXT("/Game/Vehicles/DataTables/DT_VehicleEvents.DT_VehicleEvents"));
    Rows.RemoveAll([Vehicle](const FAstroVehicleEventRow* R) { return R->Vehicle != Vehicle; });
    Rows.Sort([](const FAstroVehicleEventRow& A, const FAstroVehicleEventRow& B) { return A.TimeS < B.TimeS; });
    return Rows;
}
