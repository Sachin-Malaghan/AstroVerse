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
//   HaloLuminance float   halo just outside the limb (0 = physical corona only)
//   HaloWarmth    float   0 = white (natural: the Sun seen from space), 1 = golden (stylized)
//
// Point source (CLAUDE.md "Performance" / far views): once the photosphere shrinks below a
// couple of pixels it stops rasterizing reliably, so its flux is redistributed into a
// Gaussian ~1 px wide here - the Sun stays the brightest point in the sky from 80 AU.

const float PI = 3.14159265;
float2 p = LocalPos.xy / 50.0 * CoronaExtent; // in solar radii
float r = length(p);
float3 glow = float3(0, 0, 0);
if (PointSigma > 0.0)
{
    glow = float3(1.0, 0.96, 0.9) * PointLuminance * exp(-r * r / (2.0 * PointSigma * PointSigma));
}
if (r < 0.98)
{
    return glow; // the disk itself is drawn by the photosphere
}
float angle = atan2(p.y, p.x);

// K-corona: steep near the limb (~r^-8), a faint r^-3 tail farther out.
float falloff = 0.85 * pow(r, -8.0) + 0.15 * pow(r, -3.0);
// Slowly shifting streamers.
float streak = 0.55 + 0.45 * pow(abs(sin(angle * 3.0 + sin(angle * 7.0 + Time * 0.01) * 0.6)), 3.0);
float edge = saturate((CoronaExtent - r) / (CoronaExtent * 0.25)); // fade out before the quad edge
float3 tint = float3(1.0, 0.93, 0.85);
// Cinematic halo: a warm glow hugging the limb and a softer outer aura (Solar System Scope look).
float3 halo = lerp(float3(1.0, 0.97, 0.92), float3(1.0, 0.78, 0.4), HaloWarmth) * HaloLuminance * (exp(-(r - 1.0) * 4.5) * 0.8 + exp(-(r - 1.0) * 0.9) * 0.4) * edge;
return glow + halo + tint * Luminance * falloff * lerp(1.0, streak, saturate((r - 1.0) * 2.0)) * edge;
