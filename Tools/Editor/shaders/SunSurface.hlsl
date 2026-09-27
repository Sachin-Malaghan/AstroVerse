// Solar photosphere luminance (cd/m^2): texture map with slow animated granulation and
// wavelength-dependent limb darkening. See CLAUDE.md Phase 6.
//
// Inputs:
//   LocalPos      float3  mesh-local position (sphere radius 50)
//   ViewDirLocal  float3  direction to the camera, sun-local
//   SunTex        Texture equirect photosphere map
//   Luminance     float   disk-center luminance (real Sun ~1.6e9; clamped by caller for FP16)
//   Time          float   seconds, for granulation drift

const float PI = 3.14159265;
float3 d = normalize(LocalPos);
float lon = atan2(-d.y, d.x);
float lat = asin(clamp(d.z, -1.0, 1.0));
float2 uv = float2(0.5 + lon / (2.0 * PI), 0.5 - lat / PI);
float2 uvAlt = float2(frac(uv.x + 0.5), uv.y);
float2 gx = ddx(uv), gy = ddy(uv);
float2 gxAlt = ddx(uvAlt), gyAlt = ddy(uvAlt);
if (abs(gx.x) + abs(gy.x) > abs(gxAlt.x) + abs(gyAlt.x)) { gx = gxAlt; gy = gyAlt; }

// Two offset samples drifting against each other read as boiling granulation.
float2 drift = float2(Time * 0.0006, Time * 0.0002);
float3 a = SunTex.SampleGrad(SunTexSampler, uv + drift, gx, gy).rgb;
float3 b = SunTex.SampleGrad(SunTexSampler, uv - drift * 1.3 + float2(0.37, 0.11), gx, gy).rgb;
float3 surface = lerp(a, b, 0.5);
float lum = dot(surface, float3(0.2126, 0.7152, 0.0722));
surface = surface / max(lum, 1e-3);
// The source map is strongly orange; the real photosphere is white (~5800 K). Keep only
// a hint of the map's hue so bloom and the tonemapper don't turn the Sun into an orange ball.
surface = lerp(float3(1.0, 0.98, 0.95), surface, 0.15);

// Limb darkening, I(mu)/I(1) = 1 - u (1 - mu), stronger in blue (Neckel & Labs-like).
float mu = saturate(dot(d, normalize(ViewDirLocal)));
float3 u = float3(0.55, 0.65, 0.78);
float3 limb = 1.0 - u * (1.0 - mu);
return surface * limb * Luminance * lerp(0.85, 1.15, lum);
