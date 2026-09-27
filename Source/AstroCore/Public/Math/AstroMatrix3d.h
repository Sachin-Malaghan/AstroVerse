#pragma once
#include "Math/AstroVector3d.h"
// Double-precision 3x3 rotation matrix for reference-frame changes
// (equatorial -> ecliptic, parent-equator -> ecliptic, body-fixed rotation).
// Row-major; M * v rotates a column vector. See CLAUDE.md Phase 2.

struct ASTROCORE_API FAstroMatrix3d
{
    double M[3][3] = { { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 } };

    static FAstroMatrix3d Identity() { return FAstroMatrix3d(); }
    static FAstroMatrix3d RotationX(double AngleRad);
    static FAstroMatrix3d RotationY(double AngleRad);
    static FAstroMatrix3d RotationZ(double AngleRad);
    // Maps local frame axes to parent-frame columns (i.e. the columns are the local X/Y/Z axes).
    static FAstroMatrix3d FromColumns(const FAstroVector3d& X, const FAstroVector3d& Y, const FAstroVector3d& Z);

    FAstroVector3d operator*(const FAstroVector3d& V) const;
    FAstroMatrix3d operator*(const FAstroMatrix3d& Other) const;
    // For a pure rotation the transpose is the inverse.
    FAstroMatrix3d Transposed() const;
    FAstroVector3d GetColumn(int Index) const { return FAstroVector3d(M[0][Index], M[1][Index], M[2][Index]); }
};
