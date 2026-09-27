// Planetary ring luminance on a flat annulus (engine Plane mesh, local XY, radius 50 =
// ring outer edge). Returns float4(rgb cd/m^2, opacity). See CLAUDE.md Phase 6.
//
// Inputs:
//   LocalPos      float3  mesh-local position
//   SunDirLocal   float3  direction to the star, ring-local (ring plane = XY)
//   ViewDirLocal  float3  direction to the camera, ring-local
//   SunLux        float
//   RingInner     float   inner edge / outer edge
//   PlanetRadius  float   planet equatorial radius / outer edge
//   PlanetPolar   float   planet polar radius / outer edge
//   RingTex       Texture strip, u = radial position inner->outer, rgb color, a opacity
//   Brightness    float   albedo scale

const float PI = 3.14159265;
float2 p = LocalPos.xy / 50.0;
float r = length(p);
if (r < RingInner || r > 1.0)
{
    return float4(0, 0, 0, 0);
}
float u = (r - RingInner) / (1.0 - RingInner);
float4 ring = RingTex.SampleLevel(RingTexSampler, float2(u, 0.5), 0);

float3 L = normalize(SunDirLocal);
float3 V = normalize(ViewDirLocal);

// Planet shadow: does the ray from this ring point toward the sun hit the (oblate) planet?
float3 q = float3(p, 0.0);
float3 scale = float3(1.0 / PlanetRadius, 1.0 / PlanetRadius, 1.0 / PlanetPolar);
float3 qs = q * scale;
float3 ls = normalize(L * scale);
float b = dot(qs, ls);
float c = dot(qs, qs) - 1.0;
float shadow = (b < 0.0 && b * b - c > 0.0) ? 0.03 : 1.0;

// Lit side vs unlit side: sunlight diffuses through the ring (thin, icy particles),
// and backlit rings forward-scatter strongly.
bool sameSide = (L.z * V.z) > 0.0;
float incidence = abs(L.z);
float lit = sameSide ? incidence * 1.0 : incidence * 0.35 * (1.0 - ring.a);
float forward = pow(saturate(-dot(L, V)), 8.0) * 0.6;
float3 color = ring.rgb * Brightness * (lit + forward) * shadow * SunLux / PI;
return float4(color, ring.a);
