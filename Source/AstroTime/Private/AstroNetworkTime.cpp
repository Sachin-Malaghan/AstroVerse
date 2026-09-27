// See CLAUDE.md "Live time".
#include "AstroNetworkTime.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstroNetworkTime, Log, All);

void FAstroNetworkTime::RequestSync(const TArray<FString>& Urls)
{
    if (bInFlight || Urls.Num() == 0)
    {
        return;
    }
    bInFlight = true;
    TryUrl(Urls);
}

void FAstroNetworkTime::TryUrl(TArray<FString> Remaining)
{
    if (Remaining.Num() == 0)
    {
        bInFlight = false;
        UE_LOG(LogAstroNetworkTime, Log, TEXT("No time server reachable; using the system clock (offset %.2f s)."), OffsetSeconds);
        return;
    }
    const FString Url = Remaining[0];
    Remaining.RemoveAt(0);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("HEAD"));
    Request->SetTimeout(5.0f);
    const double SentAt = FPlatformTime::Seconds();
    TWeakPtr<bool> WeakAlive = Alive;
    Request->OnProcessRequestComplete().BindLambda(
        [this, WeakAlive, Url, Remaining, SentAt](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
        {
            if (!WeakAlive.IsValid() || !*WeakAlive.Pin())
            {
                return;
            }
            FDateTime ServerTime;
            if (bOk && Response.IsValid() && FDateTime::ParseHttpDate(Response->GetHeader(TEXT("Date")), ServerTime))
            {
                const double RoundTrip = FPlatformTime::Seconds() - SentAt;
                // The Date header has 1 s resolution and is stamped mid-flight: add half the
                // round trip and half a second (the truncated fraction, on average).
                const FDateTime Estimated = ServerTime + FTimespan::FromSeconds(0.5 * RoundTrip + 0.5);
                OffsetSeconds = (Estimated - FDateTime::UtcNow()).GetTotalSeconds();
                LastSyncRealSeconds = FPlatformTime::Seconds();
                SyncedHost = FPlatformHttp::GetUrlDomain(Url);
                bSynced = true;
                bInFlight = false;
                UE_LOG(LogAstroNetworkTime, Log, TEXT("Network time from %s: system clock offset %+.2f s (round trip %.0f ms)."),
                    *SyncedHost, OffsetSeconds, RoundTrip * 1000.0);
                return;
            }
            TryUrl(Remaining);
        });
    Request->ProcessRequest();
}

FString FAstroNetworkTime::GetSourceDescription() const
{
    return bSynced ? FString::Printf(TEXT("network time (%s)"), *SyncedHost) : FString(TEXT("system clock"));
}
