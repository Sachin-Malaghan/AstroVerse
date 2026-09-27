// Milky Way as a raymarched emission/absorption volume (statistical, not per-star; see
// CLAUDE.md "Fidelity tier" note on galaxy scale). Drawn on a box around the galaxy with
// AlphaComposite: rgb = emitted light reaching the eye (display-referred), a = 1 - T.
// See CLAUDE.md Phase 10.
//
// Galaxy frame (kpc): origin at the galactic center, +Z toward the north galactic pole,
// the Sun at (-8.2, 0, 0.021); rotation carries the Sun toward +Y (galactic longitude 90).
//
// Inputs:
//   CamKpc, PixKpc  float3  camera / shaded point in the galaxy frame (kpc)
//   FaceSign        float   TwoSidedSign
//   CameraInside    float   1 when the camera is inside the box
//   Brightness      float   overall emission scale
//   HalfSize        float3  box half extents (kpc)
//   Steps           float   raymarch samples (render budget: astro.Galaxy.RaySteps)
//
// Cost control (Phase 12): the ray is clipped to the luminous disc (r < DiscRadius), the
// expensive arm/noise terms are skipped where the disc is negligible, and the march stops
// once the volume is opaque.

struct FGalaxy
{
    // Sine-free hash (Hoskins): cheaper than frac(sin()) and stable on half-precision GPUs.
    float Hash(float3 p)
    {
        p = frac(p * 0.1031);
        p += dot(p, p.zyx + 31.32);
        return frac((p.x + p.y) * p.z);
    }

    float Noise(float3 p)
    {
        float3 i = floor(p), f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(lerp(Hash(i), Hash(i + float3(1, 0, 0)), f.x), lerp(Hash(i + float3(0, 1, 0)), Hash(i + float3(1, 1, 0)), f.x), f.y),
                    lerp(lerp(Hash(i + float3(0, 0, 1)), Hash(i + float3(1, 0, 1)), f.x), lerp(Hash(i + float3(0, 1, 1)), Hash(i + float3(1, 1, 1)), f.x), f.y), f.z);
    }

    // 0..1: how close p is to one of four logarithmic spiral arms (pitch ~12 deg).
    float Arms(float3 p)
    {
        float r = max(length(p.xy), 0.05);
        float theta = atan2(p.y, p.x);
        const float TanPitch = 0.2126; // tan(12 deg)
        float phase = log(r / 3.0) / TanPitch; // arm angle at this radius
        float best = 0.0;
        for (int k = 0; k < 4; ++k)
        {
            float d = theta - phase - k * 1.5707963;
            d = d - 6.2831853 * floor((d + 3.14159265) / 6.2831853); // wrap to [-pi, pi]
            float width = 0.6 / r + 0.1; // ~0.6 kpc across, in radians
            best = max(best, exp(-d * d / (width * width)));
        }
        return best * smoothstep(1.5, 3.5, r);
    }

    float2 Box(float3 o, float3 d, float3 h)
    {
        float3 inv = 1.0 / d;
        float3 t0 = (-h - o) * inv, t1 = (h - o) * inv;
        float3 tmin = min(t0, t1), tmax = max(t0, t1);
        return float2(max(max(tmin.x, tmin.y), tmin.z), min(min(tmax.x, tmax.y), tmax.z));
    }

    // Ray against the infinite cylinder x^2 + y^2 < R^2 (the disc's luminous extent).
    float2 Cylinder(float3 o, float3 d, float R)
    {
        float a = dot(d.xy, d.xy);
        float b = dot(o.xy, d.xy);
        float c = dot(o.xy, o.xy) - R * R;
        if (a < 1e-8) return c < 0.0 ? float2(-1e9, 1e9) : float2(1, 0);
        float disc = b * b - a * c;
        if (disc < 0.0) return float2(1, 0);
        float s = sqrt(disc);
        return float2((-b - s) / a, (-b + s) / a);
    }
};

bool wantFront = CameraInside < 0.5;
if ((FaceSign > 0.0) != wantFront)
{
    return float4(0, 0, 0, 0);
}

FGalaxy G;
float3 o = CamKpc;
float3 dir = normalize(PixKpc - CamKpc);
float2 hit = G.Box(o, dir, HalfSize);
float2 cyl = G.Cylinder(o, dir, 17.0); // exp(-17/2.6) ~ 1e-3 of the central disc
float t0 = max(max(hit.x, cyl.x), 0.0), t1 = min(hit.y, cyl.y);
if (t1 <= t0) return float4(0, 0, 0, 0);

int STEPS = clamp((int)Steps, 8, 256);
float dt = (t1 - t0) / STEPS;
float3 light = 0;
float3 transmit = 1;
// Bar: 27 deg to the Sun-center line, axis ratio 0.4.
const float cb = 0.891, sb = 0.454;
for (int i = 0; i < STEPS; ++i)
{
    float3 p = o + dir * (t0 + (i + G.Hash(dir * 91.7 + i) ) * dt);
    float r = length(p.xy);
    float z = abs(p.z);

    // Thin young disc (arms, blue-white), thick old disc (yellowish), bar + bulge (orange).
    float disc = exp(-r / 2.6) * exp(-z / 0.3);
    float thick = exp(-r / 2.0) * exp(-z / 0.9);
    float2 q = float2(p.x * cb + p.y * sb, -p.x * sb + p.y * cb);
    float bar = exp(-(q.x * q.x / 9.0 + q.y * q.y / 1.44 + p.z * p.z / 0.64));
    float bulge = exp(-length(p) / 0.5);
    if (disc < 1e-4 && thick < 1e-4 && bar < 1e-4 && bulge < 1e-4)
    {
        continue; // empty halo: nothing emits or absorbs here
    }
    float arms = 0.0, clump = 0.0;
    if (disc > 1e-4)
    {
        arms = G.Arms(p);
        clump = G.Noise(p * 2.3) * 0.6 + G.Noise(p * 7.1) * 0.4;
    }

    // Arms carry most of the young light: strong contrast against the inter-arm disc.
    float3 emission = disc * (0.12 + 5.0 * arms * clump) * float3(0.7, 0.82, 1.0)
                    + thick * 0.35 * float3(1.0, 0.9, 0.75)
                    + (bar * 1.2 + bulge * 3.0) * float3(1.0, 0.78, 0.5);
    // HII regions: pink knots strung along the arms.
    if (disc * arms > 1e-3)
    {
        emission += disc * arms * pow(G.Noise(p * 11.0), 8.0) * 6.0 * float3(1.0, 0.45, 0.6);
    }

    // Dust: thinner than the stars, concentrated on the inner edges of arms; reddens.
    float dust = exp(-r / 3.2) * exp(-z / 0.1) * (0.4 + 2.2 * arms * clump) * 9.0;
    float3 extinct = exp(-dust * dt * float3(1.3, 1.0, 0.75));
    light += transmit * emission * dt * Brightness;
    transmit *= extinct;
    if (dot(transmit, float3(0.333, 0.333, 0.334)) < 0.01)
    {
        break; // opaque: nothing behind contributes
    }
}
float opacity = saturate(1.0 - dot(transmit, float3(0.333, 0.333, 0.334)));
return float4(light, opacity);
