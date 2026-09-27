// See CLAUDE.md Phase 8.
#include "BodyTerrain.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Math/AstroConstants.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include <cmath>

DEFINE_LOG_CATEGORY_STATIC(LogAstroTerrain, Log, All);

namespace
{
    // PCG-style integer hash -> [0, 1).
    inline uint32 Hash3(int64 X, int64 Y, int64 Z, uint32 Seed)
    {
        uint64 H = static_cast<uint64>(X) * 0x9E3779B185EBCA87ull ^ static_cast<uint64>(Y) * 0xC2B2AE3D27D4EB4Full
                 ^ static_cast<uint64>(Z) * 0x165667B19E3779F9ull ^ (static_cast<uint64>(Seed) << 32 | Seed);
        H ^= H >> 33; H *= 0xFF51AFD7ED558CCDull; H ^= H >> 33; H *= 0xC4CEB9FE1A85EC53ull; H ^= H >> 33;
        return static_cast<uint32>(H);
    }

    inline double Unit(uint32 H) { return (H & 0xFFFFFF) / double(0x1000000); }

    inline FAstroVector3d Gradient(int64 X, int64 Y, int64 Z, uint32 Seed)
    {
        const uint32 H = Hash3(X, Y, Z, Seed);
        // One of 12 cube-edge directions (Perlin's improved-noise set).
        static const double G[12][3] = { {1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},{1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},{0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1} };
        const double* V = G[H % 12];
        return FAstroVector3d(V[0], V[1], V[2]);
    }

    inline double Fade(double T) { return T * T * T * (T * (T * 6.0 - 15.0) + 10.0); }

    // 3D gradient noise in double precision, roughly [-1, 1].
    double GradientNoise(const FAstroVector3d& P, uint32 Seed)
    {
        const double FX = std::floor(P.X), FY = std::floor(P.Y), FZ = std::floor(P.Z);
        const int64 X0 = static_cast<int64>(FX), Y0 = static_cast<int64>(FY), Z0 = static_cast<int64>(FZ);
        const double DX = P.X - FX, DY = P.Y - FY, DZ = P.Z - FZ;
        double Corner[8];
        for (int32 i = 0; i < 8; ++i)
        {
            const int32 CX = i & 1, CY = (i >> 1) & 1, CZ = (i >> 2) & 1;
            Corner[i] = Gradient(X0 + CX, Y0 + CY, Z0 + CZ, Seed).Dot(FAstroVector3d(DX - CX, DY - CY, DZ - CZ));
        }
        const double U = Fade(DX), V = Fade(DY), W = Fade(DZ);
        const double X00 = Corner[0] + U * (Corner[1] - Corner[0]);
        const double X10 = Corner[2] + U * (Corner[3] - Corner[2]);
        const double X01 = Corner[4] + U * (Corner[5] - Corner[4]);
        const double X11 = Corner[6] + U * (Corner[7] - Corner[6]);
        const double Y0v = X00 + V * (X10 - X00);
        const double Y1v = X01 + V * (X11 - X01);
        return (Y0v + W * (Y1v - Y0v)) * 1.4;
    }
}

bool FEquirectMap::LoadFromFile(const FString& Path)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Path))
    {
        return false;
    }
    IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    const EImageFormat Format = Module.DetectImageFormat(Bytes.GetData(), Bytes.Num());
    TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(Format);
    if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Bytes.GetData(), Bytes.Num()))
    {
        return false;
    }
    const int32 SrcW = Wrapper->GetWidth();
    const int32 SrcH = Wrapper->GetHeight();
    // Decode in the file's native layout (a rejected conversion request can leave the
    // decoder unusable), then reduce to 16-bit luminance ourselves.
    const ERGBFormat Native = Wrapper->GetFormat();
    const int32 Depth = Wrapper->GetBitDepth();
    TArray<uint8> Raw;
    if (!Wrapper->GetRaw(Native, Depth, Raw))
    {
        return false;
    }
    const bool bGray = Native == ERGBFormat::Gray || Native == ERGBFormat::GrayF;
    const int32 Channels = bGray ? 1 : 4;
    const bool bBGR = Native == ERGBFormat::BGRA || Native == ERGBFormat::BGRE;
    auto Channel = [&](int32 Index) -> double // 0..1
    {
        switch (Depth)
        {
        case 8:  return Raw[Index] / 255.0;
        case 16:
            return (Native == ERGBFormat::RGBAF || Native == ERGBFormat::GrayF)
                ? FMath::Clamp(static_cast<double>(FFloat16(reinterpret_cast<const uint16*>(Raw.GetData())[Index]).GetFloat()), 0.0, 1.0)
                : reinterpret_cast<const uint16*>(Raw.GetData())[Index] / 65535.0;
        case 32: return FMath::Clamp(static_cast<double>(reinterpret_cast<const float*>(Raw.GetData())[Index]), 0.0, 1.0);
        default: return 0.0;
        }
    };
    auto SrcAt = [&](int32 X, int32 Y) -> uint32
    {
        const int32 I = (Y * SrcW + X) * Channels;
        if (bGray)
        {
            return static_cast<uint32>(Channel(I) * 65535.0 + 0.5);
        }
        const double R = Channel(I + (bBGR ? 2 : 0)), G = Channel(I + 1), B = Channel(I + (bBGR ? 0 : 2));
        return static_cast<uint32>((0.299 * R + 0.587 * G + 0.114 * B) * 65535.0 + 0.5);
    };

    // Box-downsample by a power of two until the map fits MaxWidth.
    int32 Factor = 1;
    while (SrcW / Factor > MaxWidth)
    {
        Factor *= 2;
    }
    Width = SrcW / Factor;
    Height = SrcH / Factor;
    Values.SetNumUninitialized(Width * Height);
    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            uint32 Sum = 0;
            for (int32 DY = 0; DY < Factor; ++DY)
            {
                for (int32 DX = 0; DX < Factor; ++DX)
                {
                    Sum += SrcAt(X * Factor + DX, Y * Factor + DY);
                }
            }
            Values[Y * Width + X] = static_cast<uint16>(Sum / (Factor * Factor));
        }
    }
    return true;
}

float FEquirectMap::Sample(const FAstroVector3d& Dir) const
{
    // Matches the surface shader: u = 0.5 + east longitude / 2pi, v = 0.5 - latitude / pi.
    const double Lon = std::atan2(Dir.Y, Dir.X);
    const double Lat = std::asin(FMath::Clamp(Dir.Z, -1.0, 1.0));
    const double U = (0.5 + Lon / AstroConstants::TwoPi) * Width - 0.5;
    const double V = (0.5 - Lat / AstroConstants::Pi) * Height - 0.5;
    const int32 X0 = FMath::FloorToInt(U), Y0 = FMath::FloorToInt(V);
    const float TX = static_cast<float>(U - X0), TY = static_cast<float>(V - Y0);
    auto At = [this](int32 X, int32 Y) -> float
    {
        X = ((X % Width) + Width) % Width;
        Y = FMath::Clamp(Y, 0, Height - 1);
        return Values[Y * Width + X];
    };
    const float Top = FMath::Lerp(At(X0, Y0), At(X0 + 1, Y0), TX);
    const float Bottom = FMath::Lerp(At(X0, Y0 + 1), At(X0 + 1, Y0 + 1), TX);
    return FMath::Lerp(Top, Bottom, TY) / 65535.0f;
}

void FBodyTerrain::Initialize(const FAstroTerrainRow& InRow, FName SeedName, double EquatorialRadius, double PolarRadius, const FString& ContentDir)
{
    Row = InRow;
    Req = EquatorialRadius;
    Rpol = PolarRadius;
    Seed = GetTypeHash(SeedName.ToString());
    if (!Row.OceanMaskFile.IsEmpty() && !OceanMask.LoadFromFile(FPaths::Combine(ContentDir, Row.OceanMaskFile)))
    {
        UE_LOG(LogAstroTerrain, Warning, TEXT("%s: ocean mask '%s' not loaded; no oceans."), *SeedName.ToString(), *Row.OceanMaskFile);
    }
    if (!Row.HeightmapFile.IsEmpty() && !Heightmap.LoadFromFile(FPaths::Combine(ContentDir, Row.HeightmapFile)))
    {
        UE_LOG(LogAstroTerrain, Warning, TEXT("%s: heightmap '%s' not loaded; procedural relief only."), *SeedName.ToString(), *Row.HeightmapFile);
    }
}

double FBodyTerrain::EllipsoidRadius(const FAstroVector3d& D) const
{
    return 1.0 / std::sqrt((D.X * D.X + D.Y * D.Y) / (Req * Req) + D.Z * D.Z / (Rpol * Rpol));
}

bool FBodyTerrain::IsOcean(const FAstroVector3d& Dir) const
{
    return OceanMask.IsValid() && OceanMask.Sample(Dir) > 0.5f;
}

double FBodyTerrain::FractalRelief(const FAstroVector3d& Dir, double MinFeatureMeters) const
{
    if (Row.ReliefAmplitudeM <= 0.0)
    {
        return 0.0;
    }
    // Octaves from the base wavelength down to the requested feature size (max 22 ~ 1 m on Earth).
    double Wavelength = Row.BaseWavelengthKm * 1000.0;
    double Amplitude = 1.0, Sum = 0.0, Norm = 0.0;
    for (int32 Octave = 0; Octave < 22 && Wavelength >= MinFeatureMeters; ++Octave)
    {
        const FAstroVector3d P = Dir * (Req / Wavelength);
        // Ridged octaves for the large scales give mountain chains; smooth ones below.
        const double N = GradientNoise(P, Seed + Octave * 7919u);
        Sum += (Octave < 4 ? (1.0 - 2.0 * std::abs(N)) : N) * Amplitude;
        Norm += Amplitude;
        Amplitude *= Row.Roughness;
        Wavelength *= 0.5;
    }
    return Norm > 0.0 ? Sum / Norm * Row.ReliefAmplitudeM : 0.0;
}

double FBodyTerrain::MicroRelief(const FAstroVector3d& Dir, double MinFeatureMeters) const
{
    if (Row.MicroReliefM <= 0.0)
    {
        return 0.0;
    }
    // Self-affine roughness (Hurst ~0.8) from 200 m down to 0.5 m: planetary surfaces keep
    // detail at every scale, which is what makes standing on them read as real.
    double Wavelength = 200.0, Amplitude = 1.0, Sum = 0.0;
    for (int32 Octave = 0; Octave < 9 && Wavelength >= MinFeatureMeters; ++Octave)
    {
        Sum += GradientNoise(Dir * (Req / Wavelength), Seed + 104729u + Octave * 613u) * Amplitude;
        Amplitude *= 0.57; // 2^-0.8
        Wavelength *= 0.5;
    }
    return Sum * Row.MicroReliefM;
}

double FBodyTerrain::Craters(const FAstroVector3d& Dir, double MinFeatureMeters) const
{
    if (Row.CraterDensity <= 0.0 || Row.CraterMaxRadiusKm <= 0.0)
    {
        return 0.0;
    }
    // Crater octaves: each halves the radius and quadruples the count (power-law size distribution).
    double Height = 0.0;
    double MaxRadius = Row.CraterMaxRadiusKm * 1000.0;
    for (int32 Level = 0; Level < 16 && MaxRadius >= 3.0 && MaxRadius * 0.3 >= MinFeatureMeters; ++Level)
    {
        // A crater's influence (1.6 radii) must stay under one cell for the 27-cell search.
        const double Cell = FMath::Max(MaxRadius * 4.0 / FMath::Sqrt(Row.CraterDensity), MaxRadius * 2.0);
        const double ShellRadius = Req / Cell;
        const FAstroVector3d P = Dir * ShellRadius;
        const int64 CX = static_cast<int64>(std::floor(P.X)), CY = static_cast<int64>(std::floor(P.Y)), CZ = static_cast<int64>(std::floor(P.Z));
        for (int32 N = 0; N < 27; ++N)
        {
            const int64 X = CX + N % 3 - 1, Y = CY + (N / 3) % 3 - 1, Z = CZ + N / 9 - 1;
            const uint32 Salt = Seed ^ (0xA5A5u + Level * 131u);
            // One candidate per cell. Only seeds within half a cell of the sphere become craters:
            // that makes every crater reachable from every point it touches (no cut-offs).
            const FAstroVector3d Seed3D(X + Unit(Hash3(X, Y, Z, Salt)), Y + Unit(Hash3(X, Y, Z, Salt + 1)), Z + Unit(Hash3(X, Y, Z, Salt + 2)));
            const double SeedRadius = Seed3D.Length();
            if (FMath::Abs(SeedRadius - ShellRadius) > 0.5 || Unit(Hash3(X, Y, Z, Salt + 3)) > 0.6)
            {
                continue;
            }
            const FAstroVector3d Center = Seed3D / SeedRadius;
            const double Radius = MaxRadius * (0.35 + 0.65 * Unit(Hash3(X, Y, Z, Salt + 4)));
            const double Dist = (Dir - Center).Length() * Req / Radius; // in crater radii
            if (Dist > 1.6)
            {
                continue;
            }
            // Depth from the lunar depth-diameter relation (Pike 1974): simple craters d ~ 0.2 D,
            // complex craters (D > 15 km) d = 1.044 D^0.301 km. Rim decays outward.
            const double DiameterKm = Radius * 2.0 / 1000.0;
            const double Depth = DiameterKm < 15.0 ? 0.2 * DiameterKm * 1000.0 : 1044.0 * FMath::Pow(DiameterKm, 0.301);
            const double Bowl = Dist < 1.0 ? (Dist * Dist - 1.0) * Depth : 0.0;
            // Taper to exactly zero by the 1.6-radius cutoff so rims never end in a step.
            const double Taper = 1.0 - FMath::SmoothStep(1.3, 1.6, Dist);
            const double Rim = Depth * 0.3 * std::exp(-std::pow((Dist - 1.0) / 0.3, 2.0)) * Taper;
            Height += Bowl + Rim;
        }
        MaxRadius *= 0.5;
    }
    return Height;
}

double FBodyTerrain::HeightAt(const FAstroVector3d& Dir, double MinFeatureMeters) const
{
    if (!Row.bHasSolidSurface)
    {
        return 0.0;
    }
    double Height = FractalRelief(Dir, MinFeatureMeters) + MicroRelief(Dir, MinFeatureMeters) + Craters(Dir, MinFeatureMeters);
    if (Heightmap.IsValid())
    {
        Height += FMath::Lerp(Row.HeightmapMinM, Row.HeightmapMaxM, static_cast<double>(Heightmap.Sample(Dir)));
    }
    if (OceanMask.IsValid())
    {
        // Oceans are flat at sea level; land rises from the coast.
        const double Land = FMath::Clamp((0.5 - OceanMask.Sample(Dir)) * 4.0, 0.0, 1.0);
        Height = Land > 0.0 ? FMath::Max(Height, 0.0) * Land + 2.0 : 0.0;
    }
    return Height;
}
