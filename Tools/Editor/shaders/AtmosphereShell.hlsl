// Single-scattering atmosphere (Rayleigh + Mie) for a planet seen from space or from
// inside its atmosphere. Drawn on a shell sphere around the body with AlphaComposite
// blending: rgb = in-scattered luminance (cd/m^2), a = 1 - view transmittance, so the
// same pass adds sky glow and dims the surface beneath it. See CLAUDE.md Phase 6.
// Inlined into a material Custom node by Tools/Editor/setup_content.py.
//
// Units: shell-local, atmosphere top radius = 1. Coefficients arrive pre-multiplied
// by the top radius in meters (beta[1/m] * R_top), scale heights as fractions of R_top.
//
// Inputs:
//   CamLocal, PixLocal  float3  camera / shaded point, shell-local (mesh units / 50)
//   SunDirLocal         float3  direction to the star, shell-local
//   SunLux              float   illuminance at the body (lux)
//   GroundRadius        float   solid surface / cloud-top radius (fraction of R_top)
//   RayleighBeta        float3
//   RayleighH           float
//   MieBeta             float
//   MieH                float
//   MieG                float   Mie asymmetry
//   MieTint             float3  colors the Mie term (dust, haze)
//   CameraInside        float   1 when the camera is inside the shell
//   FaceSign            float   TwoSidedSign (+1 front face)

struct FAtmo
{
    // Schuler's Chapman-function approximation: column density toward the sun from
    // normalized height h (in scale heights) at planet radius X (in scale heights).
    float Chapman(float X, float h, float cosZ)
    {
        float c = sqrt(X + h);
        if (cosZ >= 0.0)
        {
            return c / (c * cosZ + 1.0) * exp(-h);
        }
        float x0 = sqrt(1.0 - cosZ * cosZ) * (X + h);
        if (x0 < X)
        {
            return 1e9; // sun ray hits the ground: shadowed
        }
        float c0 = sqrt(x0);
        return 2.0 * c0 * exp(X - x0) - c / (1.0 - c * cosZ) * exp(-h);
    }

    float2 Sphere(float3 o, float3 d, float r)
    {
        float b = dot(o, d);
        float c = dot(o, o) - r * r;
        float disc = b * b - c;
        if (disc < 0.0) return float2(1e9, -1e9);
        float s = sqrt(disc);
        return float2(-b - s, -b + s);
    }
};

// Draw only front faces from outside, only back faces from inside.
bool wantFront = CameraInside < 0.5;
if ((FaceSign > 0.0) != wantFront)
{
    return float4(0, 0, 0, 0);
}

FAtmo A;
const float PI = 3.14159265;
float3 o = CamLocal;
float3 dir = normalize(PixLocal - CamLocal);
float3 L = normalize(SunDirLocal);

float2 top = A.Sphere(o, dir, 1.0);
float tStart = max(top.x, 0.0);
float tEnd = top.y;
float2 ground = A.Sphere(o, dir, GroundRadius);
if (ground.x > 0.0) tEnd = min(tEnd, ground.x);
if (tEnd <= tStart) return float4(0, 0, 0, 0);

// Concentrate samples around the ray's lowest point, where nearly all the air is.
float tLow = clamp(-dot(o, dir), tStart, tEnd);
const int STEPS = 16;
float3 inscatter = 0;
float odR = 0.0, odM = 0.0;
float cosTheta = dot(dir, L);
float phaseR = 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
float g2 = MieG * MieG;
float phaseM = 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + cosTheta * cosTheta))
             / ((2.0 + g2) * pow(max(1.0 + g2 - 2.0 * MieG * cosTheta, 1e-4), 1.5));
float XR = GroundRadius / RayleighH;
float XM = GroundRadius / MieH;

for (int seg = 0; seg < 2; ++seg)
{
    // Both segments march away from the camera, so the running optical depth is exact;
    // sample spacing is densest at the ray's lowest point in each.
    float a = seg == 0 ? tStart : tLow;
    float b = seg == 0 ? tLow : tEnd;
    float prev = a;
    for (int i = 1; i <= STEPS; ++i)
    {
        float x = (float)i / STEPS;
        float w = seg == 0 ? 1.0 - (1.0 - x) * (1.0 - x) : x * x;
        float t = lerp(a, b, w);
        float ds = abs(t - prev);
        float tm = 0.5 * (t + prev);
        prev = t;

        float3 p = o + dir * tm;
        float r = length(p);
        float h = max(r - GroundRadius, 0.0);
        float dR = exp(-h / RayleighH) * ds;
        float dM = exp(-h / MieH) * ds;
        odR += dR;
        odM += dM;

        float cosZ = dot(p / r, L);
        float sunR = A.Chapman(XR, h / RayleighH, cosZ) * RayleighH;
        float sunM = A.Chapman(XM, h / MieH, cosZ) * MieH;
        float3 tau = RayleighBeta * (odR + sunR) + MieBeta * 1.1 * (odM + sunM);
        inscatter += exp(-tau) * (RayleighBeta * dR * phaseR + MieBeta * MieTint * dM * phaseM);
    }
}

float3 viewT = exp(-(RayleighBeta * odR + MieBeta * 1.1 * odM));
float opacity = saturate(1.0 - dot(viewT, float3(0.3333, 0.3333, 0.3334)));
return float4(inscatter * SunLux, opacity);
