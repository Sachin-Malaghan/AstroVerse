// Streaking warp tunnel on the inside of a long cylinder around the viewer. Returns
// display-referred color; the material scales it by inverse eye adaptation so it reads the
// same at any exposure. Additive. See CLAUDE.md Phase 9.
//
// Inputs:
//   UV         float2  cylinder UVs (U around, V along)
//   Time       float
//   Intensity  float   0..1

float lane = floor(UV.x * 96.0);
float h1 = frac(sin(lane * 12.9898) * 43758.5453);
float h2 = frac(sin(lane * 78.233) * 12543.123);
float speed = 1.5 + 3.0 * h1;
float v = frac(UV.y * (2.0 + 4.0 * h2) - Time * speed);
float streak = pow(saturate(1.0 - abs(v - 0.5) * 2.0), 18.0) * step(0.55, h2);
float3 color = lerp(float3(0.55, 0.75, 1.0), float3(1.0, 0.85, 0.7), h1);
// Fade the tunnel ends so its length never shows.
float ends = saturate(min(UV.y, 1.0 - UV.y) * 6.0);
return color * streak * Intensity * ends * 1.6;
