// Corona / glare billboard around the Sun (engine Plane mesh, always faced to the
// camera by USunCoronaComponent; plane half-size 50 = CoronaExtent solar radii).
// Additive. Returns cd/m^2. See CLAUDE.md Phase 6.
//
// Inputs:
//   LocalPos      float3  mesh-local position
//   CoronaExtent  float   plane half-size in solar radii
//   Luminance     float   corona base luminance
//   Time          float
//   PointSigma    float   point-source glow width (solar radii), 0 = off
//   PointLuminance float  its peak (cd/m^2)
//
// Point source (CLAUDE.md "Performance" / far views): once the photosphere shrinks below a
// couple of pixels it stops rasterizing reliably, so its flux is redistributed into a
// Gaussian ~1 px wide here - the Sun stays the brightest point in the sky from 80 AU.

const float PI = 3.14159265;
float2 p = LocalPos.xy / 50.0 * CoronaExtent; // in solar radii
float r = length(p);
float3 point = float3(0, 0, 0);
if (PointSigma > 0.0)
{
    point = float3(1.0, 0.96, 0.9) * PointLuminance * exp(-r * r / (2.0 * PointSigma * PointSigma));
}
if (r < 0.98)
{
    return point; // the disk itself is drawn by the photosphere
}
float angle = atan2(p.y, p.x);

// K-corona: steep near the limb (~r^-8), a faint r^-3 tail farther out.
float falloff = 0.85 * pow(r, -8.0) + 0.15 * pow(r, -3.0);
// Slowly shifting streamers.
float streak = 0.55 + 0.45 * pow(abs(sin(angle * 3.0 + sin(angle * 7.0 + Time * 0.01) * 0.6)), 3.0);
float edge = saturate((CoronaExtent - r) / (CoronaExtent * 0.25)); // fade out before the quad edge
float3 tint = float3(1.0, 0.93, 0.85);
return point + tint * Luminance * falloff * lerp(1.0, streak, saturate((r - 1.0) * 2.0)) * edge;
