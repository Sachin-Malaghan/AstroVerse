#pragma once
#include "CoreMinimal.h"
// Point-to-point warp/transit state machine. DESIGN NOT YET FINALIZED —
// see CLAUDE.md "Blocked" section before implementing Phase 9: whether
// transit is player-piloted or a fixed cinematic is still an open question
// left as a comment on the design doc.

class ASTROTRAVEL_API FTravelSystem
{
public:
    void BeginTravel(FName DestinationBodyID);
    // TODO Phase 9: blocked on transit-style decision (see CLAUDE.md)
};
