#pragma once
// Double-precision 3D vector for astronomical-scale positions and velocities.
// UE's native FVector uses LWC doubles internally in UE5, but this type exists
// so AstroCore's math stays engine-agnostic and unit-testable outside Unreal.
// See CLAUDE.md Phase 1.

struct FAstroVector3d
{
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;

    FAstroVector3d() = default;
    FAstroVector3d(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}

    FAstroVector3d operator+(const FAstroVector3d& Other) const;
    FAstroVector3d operator-(const FAstroVector3d& Other) const;
    FAstroVector3d operator*(double Scalar) const;
    double Length() const;
    FAstroVector3d Normalized() const;
};
