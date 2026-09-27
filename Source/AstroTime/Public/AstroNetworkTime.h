#pragma once
#include "CoreMinimal.h"
// Real UTC for the Live clock: the system clock, corrected by an offset measured against
// network time servers (the HTTP "Date" header, halved round-trip). Works offline — the
// offset simply stays 0 and the source reports "system clock". See CLAUDE.md "Live time".

class ASTROTIME_API FAstroNetworkTime
{
public:
    ~FAstroNetworkTime() { *Alive = false; }

    // Starts an asynchronous sync against the first responding URL. Safe to call repeatedly.
    void RequestSync(const TArray<FString>& Urls);

    // Best estimate of real UTC now.
    FDateTime UtcNow() const { return FDateTime::UtcNow() + FTimespan::FromSeconds(OffsetSeconds); }

    bool IsSynced() const { return bSynced; }
    double GetOffsetSeconds() const { return OffsetSeconds; }
    double GetLastSyncRealSeconds() const { return LastSyncRealSeconds; }
    FString GetSourceDescription() const;

private:
    void TryUrl(TArray<FString> Remaining);

    TSharedRef<bool> Alive = MakeShared<bool>(true);
    double OffsetSeconds = 0.0;
    double LastSyncRealSeconds = -1.0;
    FString SyncedHost;
    bool bSynced = false;
    bool bInFlight = false;
};
