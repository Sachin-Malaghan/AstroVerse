// Star/Milky Way background as a post-process: replaces pixels where nothing was drawn
// (depth at infinity) with the galactic-coordinate sky map. See CLAUDE.md Phase 6.
//
// The Solar System Scope map is equirectangular in galactic coordinates with the
// galactic center at u = 0.5 and longitude increasing to the left, but stored
// south-up (the Magellanic Clouds, at b ~ -33..-44 deg, appear above the band).
// Hence u = 0.5 - l/360 and v = 0.5 + b/180.
//
// Inputs:
//   ViewDirWS     float3  camera -> pixel direction, engine world space
//   SceneColor    float3
//   ScreenUV      float2  viewport UV (ScreenPosition)
//   GalX, GalY, GalZ  float3  galactic axes (x -> center, z -> north pole) in engine world space
//   StarTex       Texture
//   StarNits      float   luminance of a full-white texel
//
// Empty space is detected from the raw depth buffer: with reverse-Z, nothing drawn = 0.
// (A linear-depth threshold can't work here: scaled-space bodies are also ~1e11 cm away.)

if (LookupDeviceZ(ViewportUVToBufferUV(ScreenUV)) > 0.0)
{
    return SceneColor;
}
const float PI = 3.14159265;
float3 v = normalize(ViewDirWS);
float gx = dot(v, GalX), gy = dot(v, GalY), gz = dot(v, GalZ);
float l = atan2(gy, gx);            // galactic longitude, radians
float b = asin(clamp(gz, -1.0, 1.0)); // galactic latitude
float2 uv = float2(frac(0.5 - l / (2.0 * PI)), 0.5 + b / PI);
float3 stars = StarTex.SampleLevel(StarTexSampler, uv, 0).rgb;
return SceneColor + stars * StarNits;
