// See CLAUDE.md Phase 11.
#include "AstroUIFormat.h"
#include "Math/AstroConstants.h"

namespace AstroUIFormat
{
    FString Number(double Value, int32 Decimals)
    {
        FNumberFormattingOptions Options;
        Options.UseGrouping = true;
        Options.MinimumFractionalDigits = Decimals;
        Options.MaximumFractionalDigits = Decimals;
        return FText::AsNumber(Value, &Options).ToString();
    }

    FString Distance(double Meters)
    {
        constexpr double LightYear = 9.4607304725808e15;
        const double AbsM = FMath::Abs(Meters);
        if (AbsM < 1000.0)
        {
            return FString::Printf(TEXT("%.0f m"), Meters);
        }
        if (AbsM < 1.0e5)
        {
            return FString::Printf(TEXT("%.2f km"), Meters / 1000.0);
        }
        if (AbsM < 0.05 * AstroConstants::AstronomicalUnit)
        {
            return Number(Meters / 1000.0) + TEXT(" km");
        }
        if (AbsM < 0.5 * LightYear)
        {
            return FString::Printf(TEXT("%.3f AU"), Meters / AstroConstants::AstronomicalUnit);
        }
        return Number(Meters / LightYear, 2) + TEXT(" ly");
    }

    FString Speed(double Mps)
    {
        constexpr double C = 299792458.0;
        const double Abs = FMath::Abs(Mps);
        if (Abs < 1000.0)
        {
            return FString::Printf(TEXT("%.1f m/s"), Mps);
        }
        if (Abs < 0.1 * C)
        {
            return Number(Mps / 1000.0, 1) + TEXT(" km/s");
        }
        return FString::Printf(TEXT("%.2f c"), Mps / C);
    }

    FString Duration(double Seconds)
    {
        const double S = FMath::Abs(Seconds);
        if (S < 120.0) return FString::Printf(TEXT("%.1f s"), Seconds);
        if (S < 7200.0) return FString::Printf(TEXT("%.1f min"), Seconds / 60.0);
        if (S < 2.0 * AstroConstants::SecondsPerDay) return FString::Printf(TEXT("%.2f h"), Seconds / 3600.0);
        if (S < 2.0 * AstroConstants::SecondsPerJulianYear) return FString::Printf(TEXT("%.2f days"), Seconds / AstroConstants::SecondsPerDay);
        return FString::Printf(TEXT("%.2f years"), Seconds / AstroConstants::SecondsPerJulianYear);
    }

    FString TimeScale(double Scale, bool bPaused)
    {
        if (bPaused)
        {
            return TEXT("Paused");
        }
        const double A = FMath::Abs(Scale);
        struct FUnit { double Seconds; const TCHAR* Name; };
        static const FUnit Units[] = {
            { AstroConstants::SecondsPerJulianYear, TEXT("year") }, { 30.0 * AstroConstants::SecondsPerDay, TEXT("month") },
            { 7.0 * AstroConstants::SecondsPerDay, TEXT("week") }, { AstroConstants::SecondsPerDay, TEXT("day") },
            { 3600.0, TEXT("hour") }, { 60.0, TEXT("min") }, { 1.0, TEXT("s") } };
        FString Text = TEXT("Real time");
        if (!FMath::IsNearlyEqual(A, 1.0, 1e-6))
        {
            for (const FUnit& U : Units)
            {
                if (A >= U.Seconds * 0.999)
                {
                    const double N = A / U.Seconds;
                    Text = FMath::IsNearlyEqual(N, FMath::RoundToDouble(N), 1e-3)
                        ? FString::Printf(TEXT("%.0f %s/s"), N, U.Name)
                        : FString::Printf(TEXT("%.2g %s/s"), N, U.Name);
                    break;
                }
            }
            if (A < 1.0)
            {
                Text = FString::Printf(TEXT("%.2gx"), A);
            }
        }
        return Scale < 0.0 ? TEXT("Rewind  ") + Text : Text;
    }

    FString Mass(double Kg)
    {
        constexpr double EarthMass = 5.9722e24;
        const double Earths = Kg / EarthMass;
        if (Earths >= 0.01)
        {
            return FString::Printf(TEXT("%.3g Earth masses"), Earths);
        }
        return FString::Printf(TEXT("%.3g kg"), Kg);
    }
}
