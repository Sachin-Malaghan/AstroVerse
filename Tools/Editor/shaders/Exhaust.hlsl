// Rocket / ship engine plume (additive, unlit) on an engine basic cone (apex = nozzle at local
// +Z 50, open end at -50). Brightest along the line of sight through the middle and near the
// nozzle; fades to nothing at the silhouette and toward the open end, so the cone's flat cap
// never shows. Returns cd/m^2 (the caller scales Glow to the current exposure).
// See CLAUDE.md "Missions".
//
// Inputs:
//   N     float3  world normal
//   V     float3  camera vector (pixel -> camera), world
//   Z     float   local z (-50 open end .. +50 nozzle)
//   Glow  float3  colour * luminance
float facing = saturate(abs(dot(normalize(N), normalize(V))));
float along = saturate((Z + 50.0) / 100.0); // 1 at the nozzle, 0 at the open end
return Glow * pow(facing, 2.5) * pow(along, 1.6);
