// See CLAUDE.md Phase 2.
#include "Physics/KeplerOrbit.h"
#include "Math/AstroConstants.h"
#include "Math/AstroMatrix3d.h"
#include <cmath>

namespace
{
    double WrapTwoPi(double Angle)
    {
        Angle = std::fmod(Angle, AstroConstants::TwoPi);
        return Angle < 0.0 ? Angle + AstroConstants::TwoPi : Angle;
    }

    // Perifocal -> reference frame: Rz(Omega) * Rx(i) * Rz(omega).
    FAstroMatrix3d PerifocalToReference(const FKeplerElements& E)
    {
        return FAstroMatrix3d::RotationZ(E.LongitudeOfAscendingNode)
             * FAstroMatrix3d::RotationX(E.Inclination)
             * FAstroMatrix3d::RotationZ(E.ArgumentOfPeriapsis);
    }
}

namespace KeplerOrbit
{
    double SolveEccentricAnomaly(double MeanAnomaly, double Eccentricity)
    {
        const double M = WrapTwoPi(MeanAnomaly);
        // Starting guess good for all e < 1 (Danby).
        double E = M + 0.85 * Eccentricity * (std::sin(M) >= 0.0 ? 1.0 : -1.0);
        for (int Iteration = 0; Iteration < 50; ++Iteration)
        {
            const double F = E - Eccentricity * std::sin(E) - M;
            const double FPrime = 1.0 - Eccentricity * std::cos(E);
            const double Delta = F / FPrime;
            E -= Delta;
            if (std::abs(Delta) < 1e-15)
            {
                break;
            }
        }
        return E;
    }

    double MeanMotion(const FKeplerElements& Elements, double Mu)
    {
        const double A = Elements.SemiMajorAxis;
        return std::sqrt(Mu / (A * A * A));
    }

    double OrbitalPeriod(const FKeplerElements& Elements, double Mu)
    {
        return AstroConstants::TwoPi / MeanMotion(Elements, Mu);
    }

    FOrbitalState StateAtTime(const FKeplerElements& Elements, double Mu, double SimSeconds)
    {
        const double A = Elements.SemiMajorAxis;
        const double Ecc = Elements.Eccentricity;
        const double N = MeanMotion(Elements, Mu);
        const double MeanAnomaly = Elements.MeanAnomalyAtEpoch + N * (SimSeconds - Elements.EpochSeconds);
        const double E = SolveEccentricAnomaly(MeanAnomaly, Ecc);

        const double CosE = std::cos(E), SinE = std::sin(E);
        const double RootOneMinusE2 = std::sqrt(1.0 - Ecc * Ecc);
        const double R = A * (1.0 - Ecc * CosE);

        const FAstroVector3d PerifocalPos(A * (CosE - Ecc), A * RootOneMinusE2 * SinE, 0.0);
        const double VelScale = std::sqrt(Mu * A) / R;
        const FAstroVector3d PerifocalVel(-VelScale * SinE, VelScale * RootOneMinusE2 * CosE, 0.0);

        const FAstroMatrix3d Rot = PerifocalToReference(Elements);
        return FOrbitalState{ Rot * PerifocalPos, Rot * PerifocalVel };
    }

    FKeplerElements ElementsFromState(const FOrbitalState& State, double Mu, double SimSeconds)
    {
        const FAstroVector3d& R = State.Position;
        const FAstroVector3d& V = State.Velocity;
        const double RLen = R.Length();

        const FAstroVector3d H = R.Cross(V);
        const double HLen = H.Length();
        const FAstroVector3d NodeVec = FAstroVector3d(0.0, 0.0, 1.0).Cross(H);
        const double NodeLen = NodeVec.Length();
        const FAstroVector3d EccVec = V.Cross(H) / Mu - R / RLen;
        const double Ecc = EccVec.Length();

        const double Energy = 0.5 * V.LengthSquared() - Mu / RLen;

        FKeplerElements Out;
        Out.SemiMajorAxis = -Mu / (2.0 * Energy);
        Out.Eccentricity = Ecc;
        Out.Inclination = std::acos(std::fmax(-1.0, std::fmin(1.0, H.Z / HLen)));
        Out.EpochSeconds = SimSeconds;

        constexpr double Tiny = 1e-11;
        const bool bEquatorial = NodeLen < Tiny * HLen;
        const bool bCircular = Ecc < Tiny;

        Out.LongitudeOfAscendingNode = bEquatorial ? 0.0 : WrapTwoPi(std::atan2(NodeVec.Y, NodeVec.X));

        // Reference direction in the orbital plane from which the periapsis is measured.
        const FAstroVector3d NodeDir = bEquatorial ? FAstroVector3d(1.0, 0.0, 0.0) : NodeVec / NodeLen;
        const FAstroVector3d InPlanePerp = (H / HLen).Cross(NodeDir);

        double TrueAnomaly;
        if (bCircular)
        {
            // Periapsis undefined: put it at the node and measure the position from there.
            Out.ArgumentOfPeriapsis = 0.0;
            TrueAnomaly = std::atan2(R.Dot(InPlanePerp), R.Dot(NodeDir));
        }
        else
        {
            Out.ArgumentOfPeriapsis = WrapTwoPi(std::atan2(EccVec.Dot(InPlanePerp), EccVec.Dot(NodeDir)));
            const FAstroVector3d EccDir = EccVec / Ecc;
            TrueAnomaly = std::atan2(R.Dot((H / HLen).Cross(EccDir)), R.Dot(EccDir));
        }

        const double EccAnomaly = 2.0 * std::atan2(std::sqrt(1.0 - Ecc) * std::sin(0.5 * TrueAnomaly),
                                                    std::sqrt(1.0 + Ecc) * std::cos(0.5 * TrueAnomaly));
        Out.MeanAnomalyAtEpoch = WrapTwoPi(EccAnomaly - Ecc * std::sin(EccAnomaly));
        return Out;
    }
}
