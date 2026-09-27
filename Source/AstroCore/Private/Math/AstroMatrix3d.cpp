// See CLAUDE.md Phase 2.
#include "Math/AstroMatrix3d.h"
#include <cmath>

FAstroMatrix3d FAstroMatrix3d::RotationX(double AngleRad)
{
    const double C = std::cos(AngleRad), S = std::sin(AngleRad);
    FAstroMatrix3d R;
    R.M[1][1] = C; R.M[1][2] = -S;
    R.M[2][1] = S; R.M[2][2] = C;
    return R;
}

FAstroMatrix3d FAstroMatrix3d::RotationY(double AngleRad)
{
    const double C = std::cos(AngleRad), S = std::sin(AngleRad);
    FAstroMatrix3d R;
    R.M[0][0] = C;  R.M[0][2] = S;
    R.M[2][0] = -S; R.M[2][2] = C;
    return R;
}

FAstroMatrix3d FAstroMatrix3d::RotationZ(double AngleRad)
{
    const double C = std::cos(AngleRad), S = std::sin(AngleRad);
    FAstroMatrix3d R;
    R.M[0][0] = C; R.M[0][1] = -S;
    R.M[1][0] = S; R.M[1][1] = C;
    return R;
}

FAstroMatrix3d FAstroMatrix3d::FromColumns(const FAstroVector3d& X, const FAstroVector3d& Y, const FAstroVector3d& Z)
{
    FAstroMatrix3d R;
    R.M[0][0] = X.X; R.M[0][1] = Y.X; R.M[0][2] = Z.X;
    R.M[1][0] = X.Y; R.M[1][1] = Y.Y; R.M[1][2] = Z.Y;
    R.M[2][0] = X.Z; R.M[2][1] = Y.Z; R.M[2][2] = Z.Z;
    return R;
}

FAstroVector3d FAstroMatrix3d::operator*(const FAstroVector3d& V) const
{
    return FAstroVector3d(
        M[0][0] * V.X + M[0][1] * V.Y + M[0][2] * V.Z,
        M[1][0] * V.X + M[1][1] * V.Y + M[1][2] * V.Z,
        M[2][0] * V.X + M[2][1] * V.Y + M[2][2] * V.Z);
}

FAstroMatrix3d FAstroMatrix3d::operator*(const FAstroMatrix3d& Other) const
{
    FAstroMatrix3d R;
    for (int Row = 0; Row < 3; ++Row)
    {
        for (int Col = 0; Col < 3; ++Col)
        {
            R.M[Row][Col] = M[Row][0] * Other.M[0][Col] + M[Row][1] * Other.M[1][Col] + M[Row][2] * Other.M[2][Col];
        }
    }
    return R;
}

FAstroMatrix3d FAstroMatrix3d::Transposed() const
{
    FAstroMatrix3d R;
    for (int Row = 0; Row < 3; ++Row)
    {
        for (int Col = 0; Col < 3; ++Col)
        {
            R.M[Row][Col] = M[Col][Row];
        }
    }
    return R;
}
