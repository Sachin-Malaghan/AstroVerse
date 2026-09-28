// Light halo (additive, unlit) on a sphere: brightest through the middle, fading to nothing at
// the silhouette, so a small sphere reads as a soft point of light (station navigation lights,
// strobes, docking lights). Returns cd/m^2 (the caller scales Glow to the current exposure).
// See CLAUDE.md "Missions" (Odyssey).
//
// Inputs:
//   N     float3  world normal
//   V     float3  camera vector (pixel -> camera), world
//   Glow  float3  colour * luminance
float facing = saturate(abs(dot(normalize(N), normalize(V))));
return Glow * pow(facing, 4.0);
