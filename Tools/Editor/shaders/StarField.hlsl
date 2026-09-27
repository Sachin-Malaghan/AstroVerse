// Star/Milky Way background as a post-process: replaces pixels where nothing was drawn
// (depth at infinity) with the galactic-coordinate sky. See CLAUDE.md Phase 6 and
// "Milky Way in the sky".
//
// The Solar System Scope map is equirectangular in galactic coordinates with the
// galactic center at u = 0.5 and longitude increasing to the left, but stored
// south-up (the Magellanic Clouds, at b ~ -33..-44 deg, appear above the band).
// Hence u = 0.5 - l/360 and v = 0.5 + b/180.
//
// Two renditions:
//  - Realistic (BandStrength 0): the map as photographed. Its band is ~1% of white in linear
//    terms, so next to a sunlit planet it is (correctly) almost invisible.
//  - Enhanced (BandStrength > 0): the stars from the map at their own size, plus a smooth
//    procedural band in galactic coordinates styled like a long-exposure photograph - the
//    bright bulge toward Sagittarius (l = 0), the Great Rift dust lane along the plane from
//    Cygnus to Sagittarius, clumpy star clouds, warm core and blue-white edges. Its position
//    is exact (galactic coordinates); its look is a teaching / cinematic rendition. The map
//    itself can't be stretched this far: its 8-bit JPEG steps turn into blotches.
//
// Inputs:
//   ViewDirWS     float3  camera -> pixel direction, engine world space
//   SceneColor    float3
//   ScreenUV      float2  viewport UV (ScreenPosition)
//   GalX, GalY, GalZ  float3  galactic axes (x -> center, z -> north pole) in engine world space
//   StarTex       Texture
//   StarNits      float   luminance of a full-white texel
//   BandStrength  float   enhanced band brightness (0 = realistic)
//   StarGain      float   star brightness multiplier in enhanced mode
//
// Empty space is detected from the raw depth buffer: with reverse-Z, nothing drawn = 0.
// (A linear-depth threshold can't work here: scaled-space bodies are also ~1e11 cm away.)

struct FSky
{
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
    // Fractal noise on the sky sphere (no seams: sampled by direction).
    float Fbm(float3 p)
    {
        float s = 0.0, a = 0.5;
        for (int k = 0; k < 5; ++k)
        {
            s += Noise(p) * a;
            p = p * 2.03 + 17.1;
            a *= 0.5;
        }
        return s;
    }
};

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
if (BandStrength <= 0.0)
{
    return SceneColor + stars * StarNits;
}

FSky S;
float3 g = float3(gx, gy, gz);
float ld = degrees(l), bd = degrees(b);
// Longitude weight toward the centre (the bulge region spans roughly l = -60..+60).
float centre = exp(-ld * ld / (2.0 * 28.0 * 28.0));
float core = exp(-(ld * ld + bd * bd * 3.0) / (2.0 * 9.0 * 9.0));
// Disc brightness falls off with |b|; thicker toward the centre.
float width = 3.5 + 7.0 * centre;
float disc = exp(-abs(bd) / width) * (0.28 + 0.72 * centre);
// Star clouds: clumpy, strongest in the plane.
float clouds = S.Fbm(g * 9.0);
float fine = S.Fbm(g * 34.0);
disc *= 0.35 + 1.3 * clouds * clouds + 0.25 * fine;
float glow = disc + core * 1.6 * (0.7 + 0.6 * fine);
// Dust: the Great Rift (Cygnus -> Sagittarius, l ~ +60 .. -20) and thinner lanes elsewhere,
// wandering a little about the plane.
float laneCentre = 0.8 * sin(radians(ld * 3.0)) + (S.Fbm(g * 6.0) - 0.5) * 3.0;
float laneWidth = 1.8 + 3.0 * centre;
float lane = exp(-pow((bd - laneCentre) / laneWidth, 2.0));
float rift = smoothstep(80.0, 45.0, ld) * smoothstep(-35.0, -10.0, ld); // strongest across the rift
float dust = lane * (0.5 + 0.35 * rift) * (0.4 + 1.2 * S.Fbm(g * 16.0));
// Filaments and dark clouds off the main lane.
dust += smoothstep(0.5, 0.8, S.Fbm(g * 22.0 + 5.0)) * exp(-abs(bd) / 7.0) * 0.35;
glow *= 1.0 - 0.78 * smoothstep(0.0, 1.1, dust); // dust dims, never punches holes
// Colour: blue-white disc, warm (older stars, reddened by dust) toward the core.
float warm = saturate(core * 1.5 + centre * 0.3 + dust * 0.35);
float3 colour = lerp(float3(0.6, 0.66, 1.0), float3(1.0, 0.8, 0.62), warm);
colour += float3(0.12, 0.0, 0.1) * saturate(glow * 2.0) * (1.0 - warm); // violet haze in the star clouds
float3 band = colour * glow * BandStrength;
return SceneColor + (band + stars * StarGain) * StarNits;
