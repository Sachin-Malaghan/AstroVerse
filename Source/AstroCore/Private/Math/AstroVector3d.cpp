// See CLAUDE.md Phase 1.
#include "Math/AstroVector3d.h"
#include <cmath>

FAstroVector3d FAstroVector3d::operator+(const FAstroVector3d& Other) const
{
    return FAstroVector3d(X + Other.X, Y + Other.Y, Z + Other.Z);
}

FAstroVector3d FAstroVector3d::operator-(const FAstroVector3d& Other) const
{
    return FAstroVector3d(X - Other.X, Y - Other.Y, Z - Other.Z);
}

FAstroVector3d FAstroVector3d::operator-() const
{
    return FAstroVector3d(-X, -Y, -Z);
}

FAstroVector3d FAstroVector3d::operator*(double Scalar) const
{
    return FAstroVector3d(X * Scalar, Y * Scalar, Z * Scalar);
}

FAstroVector3d FAstroVector3d::operator/(double Scalar) const
{
    return FAstroVector3d(X / Scalar, Y / Scalar, Z / Scalar);
}

FAstroVector3d& FAstroVector3d::operator+=(const FAstroVector3d& Other)
{
    X += Other.X; Y += Other.Y; Z += Other.Z;
    return *this;
}

FAstroVector3d& FAstroVector3d::operator-=(const FAstroVector3d& Other)
{
    X -= Other.X; Y -= Other.Y; Z -= Other.Z;
    return *this;
}

FAstroVector3d& FAstroVector3d::operator*=(double Scalar)
{
    X *= Scalar; Y *= Scalar; Z *= Scalar;
    return *this;
}

double FAstroVector3d::Dot(const FAstroVector3d& Other) const
{
    return X * Other.X + Y * Other.Y + Z * Other.Z;
}

FAstroVector3d FAstroVector3d::Cross(const FAstroVector3d& Other) const
{
    return FAstroVector3d(
        Y * Other.Z - Z * Other.Y,
        Z * Other.X - X * Other.Z,
        X * Other.Y - Y * Other.X);
}

double FAstroVector3d::LengthSquared() const
{
    return X * X + Y * Y + Z * Z;
}

double FAstroVector3d::Length() const
{
    return std::sqrt(LengthSquared());
}

FAstroVector3d FAstroVector3d::Normalized() const
{
    const double Len = Length();
    return Len > 0.0 ? *this / Len : FAstroVector3d();
}
