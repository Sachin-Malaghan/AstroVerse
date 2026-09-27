// Planet / moon surface radiance (cd/m^2) for AstroVerse. Inlined into a material
// Custom node by Tools/Editor/setup_content.py. See CLAUDE.md Phase 6.
//
// All lighting happens in the body's local frame (local Z = north pole, local X =
// prime meridian; engine-local Y is body -Y because the engine is left-handed), so
// the sun direction must come from the simulation per body, never a scene light.
//
// Inputs:
//   LocalPos      float3  mesh-local position (sphere of radius 50)
//   SunDirLocal   float3  direction to the star, body-local
//   ViewDirLocal  float3  direction to the camera, body-local
//   SunLux        float   illuminance at this body (lux)
//   DayTex / NightTex / CloudTex / SpecTex / NormalTex  Texture objects (1x1 defaults when unused)
//   Tint          float3  multiplies the day map (procedural bodies use a flat map + tint)
//   NightNits     float   luminance of the night map at full value (Earth city lights)
//   CloudAmount   float   0..1 cloud layer opacity
//   CloudOffsetU  float   cloud drift (u offset, wraps)
//   SpecAmount    float   ocean glint strength (uses SpecTex as mask)
//   NormalAmount  float   0..1 normal map strength
//   LunarMix      float   0 = Lambert, 1 = Lommel-Seeliger (airless regolith: flat full moon)
//   Terminator    float   softening width of the day/night edge (thick atmospheres)
//   Procedural    float   0..1 blend of fractal noise into the albedo (bodies with no map)
//   RingInner / RingOuter  float  ring radii in planet radii (0 = no rings)
//   RingTex       Texture  ring alpha/color strip (u = radial)

const float PI = 3.14159265;
float3 d = normalize(LocalPos);

// Equirectangular UV, east longitude positive, north up.
float lon = atan2(-d.y, d.x);
float lat = asin(clamp(d.z, -1.0, 1.0));
float2 uv = float2(0.5 + lon / (2.0 * PI), 0.5 - lat / PI);

// Seam-free gradients: use whichever wrap of u has the smaller derivative.
float2 uvAlt = float2(frac(uv.x + 0.5), uv.y);
float2 gx = ddx(uv), gy = ddy(uv);
float2 gxAlt = ddx(uvAlt), gyAlt = ddy(uvAlt);
if (abs(gx.x) + abs(gy.x) > abs(gxAlt.x) + abs(gyAlt.x)) { gx = gxAlt; gy = gyAlt; }

float3 albedo = DayTex.SampleGrad(DayTexSampler, uv, gx, gy).rgb * Tint;

if (Procedural > 0.0)
{
    // Cheap value-noise fBm on the unit sphere for bodies with no texture map.
    float n = 0.0, amp = 0.5;
    float3 p = d * 4.0;
    for (int o = 0; o < 6; ++o)
    {
        float3 i = floor(p), f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        float h000 = frac(sin(dot(i,                     float3(127.1, 311.7, 74.7))) * 43758.5453);
        float h100 = frac(sin(dot(i + float3(1, 0, 0),   float3(127.1, 311.7, 74.7))) * 43758.5453);
        float h010 = frac(sin(dot(i + float3(0, 1, 0),   float3(127.1, 311.7, 74.7))) * 43758.5453);
        float h110 = frac(sin(dot(i + float3(1, 1, 0),   float3(127.1, 311.7, 74.7))) * 43758.5453);
        float h001 = frac(sin(dot(i + float3(0, 0, 1),   float3(127.1, 311.7, 74.7))) * 43758.5453);
        float h101 = frac(sin(dot(i + float3(1, 0, 1),   float3(127.1, 311.7, 74.7))) * 43758.5453);
        float h011 = frac(sin(dot(i + float3(0, 1, 1),   float3(127.1, 311.7, 74.7))) * 43758.5453);
        float h111 = frac(sin(dot(i + float3(1, 1, 1),   float3(127.1, 311.7, 74.7))) * 43758.5453);
        float v = lerp(lerp(lerp(h000, h100, f.x), lerp(h010, h110, f.x), f.y),
                       lerp(lerp(h001, h101, f.x), lerp(h011, h111, f.x), f.y), f.z);
        n += v * amp;
        amp *= 0.5;
        p *= 2.03;
    }
    albedo *= lerp(1.0, 0.55 + 0.9 * n, Procedural);
}

// Surface normal (sphere, optionally perturbed by the tangent-space normal map).
float3 N = d;
if (NormalAmount > 0.0)
{
    // East on the body is cross(Z, d) in the right-handed body frame; mirrored into engine-local Y
    // that becomes (d.y, -d.x, 0). North is the pole projected onto the tangent plane.
    float2 eastXY = float2(d.y, -d.x);
    float3 east = dot(eastXY, eastXY) > 1e-10 ? float3(normalize(eastXY), 0.0) : float3(1, 0, 0);
    float3 north = normalize(float3(0, 0, 1) - d * d.z + east * 1e-6);
    float3 tn = NormalTex.SampleGrad(NormalTexSampler, uv, gx, gy).xyz * 2.0 - 1.0;
    tn.xy *= NormalAmount;
    N = normalize(tn.x * east + tn.y * north + tn.z * d);
}

float3 L = normalize(SunDirLocal);
float3 V = normalize(ViewDirLocal);
float mu0 = dot(N, L);
float muGeo = dot(d, L);
float mu = saturate(dot(N, V));

// Day side: Lambert blended with Lommel-Seeliger for regolith.
float lambert = saturate(mu0);
// Lommel-Seeliger, scaled to equal Lambert at normal incidence and viewing.
float lommel = mu0 > 0.0 ? 2.0 * mu0 / (mu0 + mu + 1e-4) : 0.0;
float diffuse = lerp(lambert, lommel, LunarMix);
// Thick atmospheres soften the day/night edge.
if (Terminator > 0.0)
{
    diffuse *= smoothstep(-Terminator, Terminator, muGeo);
}

// Ring shadow on the planet: march from the surface toward the sun to the equatorial plane.
float ringShadow = 1.0;
if (RingOuter > 0.0 && abs(L.z) > 1e-4)
{
    float t = -d.z / L.z;
    if (t > 0.0)
    {
        float r = length((d + L * t).xy);
        if (r > RingInner && r < RingOuter)
        {
            float ru = (r - RingInner) / (RingOuter - RingInner);
            ringShadow = 1.0 - 0.85 * RingTex.SampleLevel(RingTexSampler, float2(ru, 0.5), 0).a;
        }
    }
}

float3 radiance = albedo * diffuse * ringShadow * SunLux / PI;

// Clouds: brighten with their own diffuse term, shadow nothing (kept cheap).
if (CloudAmount > 0.0)
{
    float2 cuv = float2(frac(uv.x + CloudOffsetU), uv.y);
    float c = CloudTex.SampleGrad(CloudTexSampler, cuv, gx, gy).r * CloudAmount;
    float cloudLight = saturate(muGeo) * ringShadow * SunLux / PI;
    radiance = lerp(radiance, float3(0.9, 0.9, 0.9) * cloudLight, c);
    NightNits *= (1.0 - c);
    SpecAmount *= (1.0 - c);
}

// Ocean glint (Blinn-Phong on the sphere normal).
if (SpecAmount > 0.0 && muGeo > 0.0)
{
    float mask = SpecTex.SampleGrad(SpecTexSampler, uv, gx, gy).r;
    float3 H = normalize(L + V);
    radiance += mask * SpecAmount * pow(saturate(dot(d, H)), 180.0) * saturate(muGeo) * SunLux / PI;
}

// City lights fade in across the terminator.
if (NightNits > 0.0)
{
    float night = 1.0 - smoothstep(-0.08, 0.08, muGeo);
    radiance += NightTex.SampleGrad(NightTexSampler, uv, gx, gy).rgb * NightNits * night;
}

return radiance;
