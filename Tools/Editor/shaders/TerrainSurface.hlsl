// Close-range terrain albedo (lit material; the engine does the lighting and shadows).
// Returns float4(albedo, ocean mask). See CLAUDE.md Phase 8.
//
// Inputs:
//   Dir         float3  body-fixed unit direction (from UV0.xy, UV1.x), right-handed
//   LocalPos    float3  patch-local position (cm) - stable across floating-origin rebases
//   DayTex      Texture equirect surface map (1x1 white for procedural bodies)
//   SpecTex     Texture ocean/specular mask (1x1 black when none)
//   Tint        float3
//   Procedural  float   0..1 fractal albedo variation for bodies without a map
//   SpecAmount  float   >0 when SpecTex marks water
//   LatScale    float   maps' latitude from the geocentric direction: (A/C)^2 for geodetic maps, else 1

const float PI = 3.14159265;
float3 d = normalize(Dir);
float lon = atan2(d.y, d.x);
float lat = atan2(d.z * LatScale, length(d.xy));
float2 uv = float2(0.5 + lon / (2.0 * PI), 0.5 - lat / PI);
float2 uvAlt = float2(frac(uv.x + 0.5), uv.y);
float2 gx = ddx(uv), gy = ddy(uv);
float2 gxAlt = ddx(uvAlt), gyAlt = ddy(uvAlt);
if (abs(gx.x) + abs(gy.x) > abs(gxAlt.x) + abs(gyAlt.x)) { gx = gxAlt; gy = gyAlt; }

float3 albedo = DayTex.SampleGrad(DayTexSampler, uv, gx, gy).rgb * Tint;
float ocean = SpecAmount > 0.0 ? SpecTex.SampleGrad(SpecTexSampler, uv, gx, gy).r : 0.0;

// Metre-scale detail the orbital map can't carry: value-noise fBm on patch-local meters.
float3 p = LocalPos * 0.01;
float n = 0.0, amp = 0.5, freq = 0.02;
for (int o = 0; o < 7; ++o)
{
    float3 q = p * freq;
    float3 i = floor(q), f = frac(q);
    f = f * f * (3.0 - 2.0 * f);
    float h000 = frac(sin(dot(i,                   float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h100 = frac(sin(dot(i + float3(1, 0, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h010 = frac(sin(dot(i + float3(0, 1, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h110 = frac(sin(dot(i + float3(1, 1, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h001 = frac(sin(dot(i + float3(0, 0, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h101 = frac(sin(dot(i + float3(1, 0, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h011 = frac(sin(dot(i + float3(0, 1, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h111 = frac(sin(dot(i + float3(1, 1, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
    n += amp * lerp(lerp(lerp(h000, h100, f.x), lerp(h010, h110, f.x), f.y),
                    lerp(lerp(h001, h101, f.x), lerp(h011, h111, f.x), f.y), f.z);
    amp *= 0.5;
    freq *= 2.3;
}
// Fade detail with distance so far rings don't shimmer.
float fade = saturate(1.0 - length(ddx(p)) * 0.5);
float detail = lerp(1.0, 0.7 + 0.6 * n, fade * (1.0 - ocean));
albedo *= detail * lerp(1.0, 0.55 + 0.9 * n, Procedural);
return float4(albedo, ocean);
