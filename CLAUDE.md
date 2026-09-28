# AstroVerse — instructions for Claude Code

Read this file in full before writing or editing anything in this repo. It is the standing spec for the project — treat it as higher priority than assumptions from general Unreal Engine knowledge when the two conflict.

## What this is

A production-grade C++ solar system simulator in Unreal Engine: true physical scale (size and distance share one scale factor, so the Sun correctly looks tiny from Neptune), full N-body gravity, a god-mode time clock the user controls, every planet and moon landable and walkable, and a separate Milky Way galaxy view. Desktop + VR first; mobile is a later port. Cinematic visual quality is a hard requirement, not a stretch goal — this is not a diagram-style visualizer.

## Before you touch any build commands

**Unreal Engine itself is not installed by this repo and cannot be installed by you.** It requires a human to log into the Epic Games Launcher, accept the EULA, and download a 30–100GB engine build through a GUI installer — see `SETUP.md`. Do not attempt to invoke `UnrealBuildTool`, open the `.uproject`, or run any engine binary until the person confirms Unreal Engine 5.4+ and Visual Studio 2022 (with the "Game development with C++" workload) are installed. If asked to build or run the project before that's confirmed, say so plainly and point to `SETUP.md` instead of attempting it.

## Build and test commands (UE 5.8 Launcher build, Windows)

Launcher engine builds do not ship `GenerateProjectFiles.bat`; drive UnrealBuildTool through `Build.bat` instead. Engine root: `C:\Program Files\Epic Games\UE_5.8`.

```
# Regenerate AstroVerse.sln (after adding/removing source files or modules)
"<Engine>\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="<repo>\AstroVerse.uproject" -game -rocket -progress

# Compile the editor target
"<Engine>\Engine\Build\BatchFiles\Build.bat" AstroVerseEditor Win64 Development -project="<repo>\AstroVerse.uproject" -waitmutex

# (or: powershell -ExecutionPolicy Bypass -File Tools\Validation\run_tests.ps1   -- prints a pass/fail summary)
# Run all AstroVerse automation tests headless (results in Saved/Logs/Tests.log)
"<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<repo>\AstroVerse.uproject" -nullrhi -unattended -nosplash -nopause -ExecCmds="Automation RunTests AstroVerse; Quit" -TestExit="Automation Test Queue Empty" -log=Tests.log
```

After adding or removing a `.cpp` file, regenerate project files before building — the installed engine's UnrealBuildTool caches the file list and otherwise fails with unresolved externals.

OpenXR "failed to find active runtime" errors in headless runs are expected when no headset runtime is installed.

Generated content (Data Table assets, placeholder materials, `L_SolarSystem`) is rebuilt by `Tools/Editor/setup_content.py` (run headless with `-run=pythonscript -script=...`). Visual validation: `Tools/Validation/capture_scenes.ps1` launches the game, captures screenshots to `Saved/Screenshots/WindowsEditor/`, and exits. Dev console commands live under `astro.*` (`astro.Time.*`, `astro.Origin.*`, `astro.Camera.*`, `astro.Debug.*`).

## Non-negotiable architecture rule

Four layers, strictly one-directional dependency:

```
Foundation   → AstroCore
Simulation   → AstroBodies, AstroTime, AstroActivation
Presentation → AstroRendering, AstroTravel, AstroGalaxy
Application  → AstroInput, AstroUI, AstroApp
```

A module's `.Build.cs` `PublicDependencyModuleNames` may only list modules in its own layer or below. `AstroCore` must never depend on anything. `AstroApp` is thin wiring (GameModes, pawns, level setup) — if you find yourself adding real simulation or rendering logic to `AstroApp`, it belongs in a lower-layer module instead. The `.Build.cs` files already in this repo encode the correct dependency lists — treat a change to one as a change to the architecture, not just a build tweak, and don't make it without flagging it.

## Physics and simulation rules

- **Full N-body gravitational simulation** for the Sun + 8 planets (and major moons once in scope), not fixed Keplerian ellipses — bodies actually perturb each other.
- **Integrator: symplectic (Leapfrog / velocity-Verlet), never Runge-Kutta.** RK-family integrators leak energy over long runs, which becomes visible as orbital decay/expansion once the user runs the clock at high time-acceleration. `AstroCore/Public/Physics/LeapfrogSolver.h` is the implementation target.
- **Double precision throughout** for position/velocity/mass. `FAstroVector3d` in `AstroCore` exists specifically to keep this math engine-agnostic and unit-testable outside Unreal.
- **Fixed physics timestep**, decoupled from render framerate — the render thread interpolates between physics steps.
- **Time source of truth is `AstroTime::UTimeController`.** No module reads wall-clock time or `GetWorld()->GetTimeSeconds()` for simulation purposes — everything reads simulated time from the time controller, which is what makes pause/rewind/timescale behave globally instead of per-module.

## Fidelity tier system (do not build a second physics path)

One system, three tiers (`EFidelityTier` in `AstroActivation`):

| Tier | What runs | Membership |
|---|---|---|
| `Dormant` | Cheap analytic two-body propagation | Default, every body, always |
| `Ambient` | Promoted detail | Currently in camera frustum |
| `Active` | Full N-body precision | Player's current reference frame, or a locked observation target |

"Walking mode" and "observation mode" are UI states that change *what counts as active* — they are not two separate physics implementations. Do not branch simulation logic on a "mode" enum; branch on tier membership instead.

- Dormant bodies must never stop updating — this is what prevents pop-in. A promotion to Ambient/Active is a handoff with a short positional blend, never a snap.
- Observation locks: capped at **3 concurrent**. Locking a 4th demotes the oldest lock; surface this to the user, never fail silently.
- Landing on a body makes it (and its parent, if it's a moon) force-`Active` and becomes the local floating-origin reference frame — this is structural, not a lock, and doesn't count against the cap.
- Galaxy-scale "observation" is a different system (`AstroGalaxy::FGalaxyView`) — never extend N-body simulation to individual stars.

## Scale and precision

- Distance and size share one true physical scale factor. Do not artistically compress distances the way casual solar-system visualizers do — the correct-looking Sun-from-Jupiter effect depends on this.
- Solar System and Galaxy are **separate scale-domains** with a deliberate origin-reanchoring transition between them — not one coordinate space. Unreal 5's Large World Coordinates (already enabled by default) solve precision within one domain; they do not solve spanning ~10^18 km of galactic scale in the same space as ~10^10 km of solar-system scale.
- Floating origin: camera stays logically at the world origin; the universe shifts around it, standard practice in this genre (Space Engine, Elite Dangerous, KSP use the same technique).

## Coding conventions

- Follow Epic's standard UE C++ conventions (`A` prefix for Actors, `U` for UObjects, `F` for plain structs, `E` for enums, `I` for interfaces).
- Every new class gets a one-line comment pointing back to the CLAUDE.md phase it belongs to, as the stub files in this repo already do — keep that pattern going.
- Real astronomical data (masses, radii, orbital elements) goes in Data Tables under `Content/Bodies/DataTables/`, never hardcoded in C++.

## Phase checklist — work through in order, one phase at a time

Do not start a phase until the previous one compiles and the relevant module's classes are wired together. Check items off in this file as you complete them.

- [x] **Phase 0 — Environment.** Manual, human-only. See `SETUP.md`. Confirm before doing anything else.
- [x] **Phase 1 — AstroCore.** Implement `FAstroVector3d` operators, `LeapfrogSolver::KickDriftKick`, `FNBodyIntegrator::Step`, `FSimClock::Advance`.
- [x] **Phase 2 — AstroBodies.** Implement `FBodyRegistry::LoadFromDataTables`; author `DT_Planets.csv` with real masses/radii/orbital elements for the Sun + 8 planets; wire `ACelestialBody::Tick` to read integrator output.
- [x] **Phase 3 — AstroTime.** Implement `UTimeController` play/pause/rewind/timescale; confirm every Phase 1-2 class reads time from here, not `GetWorld()`.
- [x] **Phase 4 — Minimal proof-of-motion scene.** Placeholder spheres orbiting correctly in `L_SolarSystem`, no art pass yet — this validates the physics visually before any rendering investment.
- [x] **Phase 5 — AstroActivation.** Implement `FActivationManager` tier promotion/demotion, the 3-lock cap with demotion-of-oldest, reference-frame handling.
- [x] **Phase 6 — AstroRendering.** Corona/flare, atmospheric scattering, ring shaders; split Lumen/Nanite budgets per platform (desktop full, VR trimmed for 90Hz).
- [x] **Phase 7 — Scale and precision infrastructure.** Confirm floating-origin behavior at true scale; build the Solar-System ↔ Galaxy scale-domain transition.
- [x] **Phase 8 — AstroInput + AstroApp pawns.** Desktop flycam pawn first, then VR pawn with room-scale/teleport locomotion.
- [x] **Phase 9 — AstroTravel.** Cinematic warp and player-piloted ship, user-selectable; clock-during-transit is a user setting. See "Decided — travel" below.
- [x] **Phase 10 — AstroGalaxy.** Milky Way disc representation, Sun position/velocity marker, scale-domain transition polish.
- [x] **Phase 11 — AstroUI.** God-mode time HUD, teaching-mode facts panels, VR world-space diegetic panels.
- [x] **Phase 12 — Platform polish.** VR performance budget pass; groundwork for the later mobile port.

## UI (Phase 11)

- All UMG is built in C++ (`UAstroHUDWidget`, `UAstroPauseMenuWidget`), owned per local player by `UAstroUISubsystem`; no widget Blueprints. Teaching-mode text lives in `Content/UI/DataTables/DT_Facts.csv` (row = BodyID); the numbers on the card (radius, gravity, orbit, current distance and speed) are computed live from the simulation, not typed in.
- The subsystem turns engine events into toasts: a 4th observation lock demoting the oldest, full N-body physics suspended at high time scales, travel start/arrival, scale-domain changes.
- VR: the same HUD (compact) is hosted on a `UWidgetComponent` on the left controller; the right controller carries a `UWidgetInteractionComponent` pointer (Select presses it). Untested without a headset.
- Validation: `capture_commands.ps1 -ShowUI` (screenshots include UMG); dev commands `astro.Select <Body>`, `astro.UI.Toggle HUD|Help|Menu`.

## Performance budgets (Phase 12)

- One budget system: `UAstroRenderingSubsystem` picks Desktop / VR / Mobile (auto from device; `astro.Render.Budget desktop|vr|mobile` + `astro.Render.ApplyBudget` forces one) and applies the matching cvar list from `[/Script/AstroRendering.AstroRenderingSettings]` in DefaultGame.ini. Targets: desktop 60 Hz, VR 90 Hz, mobile 30 Hz (`*TargetHz`).
- Measure with `Tools/Validation/perf_pass.ps1 [-Budget vr]`: seven scenes, one `AstroPerf:` line each (avg frame / game / render / GPU ms vs budget). `-Budget vr` uses `-emulatestereo` at screen percentage 200 on a 1920x1032 window (~1920x2064 per eye, Quest 3 / Index class). `astro.Perf.Sample <s> <label>` works in any session; `profilegpu` gives the per-pass breakdown.
- Findings on the dev machine (Quadro M4000, Maxwell 2015 — below VR min-spec), VR budget, before -> after: Earth orbit 29.3 -> 17.1 ms, Mars surface 34.2 -> 22.1, inner system 23.1 -> 11.0, galaxy 194 -> 20.0. Wins: SSAO off in VR (~5 ms per eye, no visual value under a single point-like sun); translucency lighting volume off everywhere (all translucency is unlit); galaxy raymarch clipped to the luminous disc, empty-halo and opacity early-outs, sine-free hash, 48 steps in VR (`astro.Galaxy.RaySteps`) and half-resolution translucency while in the galaxy domain (`astro.Galaxy.TranslucencyScreenPercentage`).
- Remaining VR cost on this GPU is spread (base pass ~4 ms, sun lighting + shadows ~3.5, reflections/sky ~1.8, TAA). About 4 ms is editor-hosted `-game` overhead (`CompositeDebugPrimitives`, `ClearGPUMessageBuffer`) that a packaged build does not pay. On a VR min-spec GPU (RTX 2070 class, ~3x this card) every scene should fit 11.1 ms; verify on hardware with a headset.
- Mobile groundwork (not yet run on a device): `MobileCVars` budget; `Config/DefaultDeviceProfiles.ini` caps TEXTUREGROUP_World (the 8K body maps) at 2K on Android/iOS; a tap (Touch1) selects under the reticle. Still to do for the port: touch look/move gestures, an ES3.1 / Vulkan shader-compile pass over the custom HLSL materials, a mobile HUD layout.

## Post-phase features (decided 2026-09-27)

VR stays built but is parked for later (user decision); the work below is desktop-first.

### Live time
- `UTimeController` has a **Live** mode (default at start, `bStartLive`): simulated time locked to real UTC at 1x. Real UTC = the system clock + an offset measured from the HTTP `Date` header of `NetworkTimeUrls` (HEAD requests; halved round trip; resynced every `NetworkResyncMinutes`, retried while offline). Offline it silently uses the system clock. This is the one sanctioned wall-clock read, inside the time source of truth.
- Any manual change (pause, speed, rewind, jump) leaves Live; `L`, the HUD's LIVE button or `astro.Time.Live` return to it. The HUD shows UTC plus local time (this PC's zone, or the active site's).

### Camera: orbit, zoom, home
- `AAstroPawnBase` orbit mode: `F` orbits the selection (or the nearest body); mouse / A-D / Space-C circle it, wheel / W-S zoom on a log scale from 100 AU down to 30 m above the real terrain; below the co-rotating threshold the orbit rides with the surface. `F` again = free flight. Travel arrival drops you into orbit around the destination.
- Zoom (2026-09-28): the wheel always zooms (Solar System Scope style) - in orbit it changes distance, in free flight it starts an orbit zoom about the selection or nearest body; 0.45 ln per notch (`astro.Camera.ZoomStep`), x3 with Shift, 5 m to 200 AU. Flight speed is on + / -. Telescope: hold Z / X to narrow / widen the field of view (down to 0.05 deg, ~1400x, `astro.Camera.TelescopeMinFOV`), middle click resets; the status line shows the factor.
- `Home` / `Backspace` (`astro.Home`) always returns to the home view (Earth, sunlit side) — from the galaxy, mid-travel, anywhere.

### Guided tour
- `UAstroTourSubsystem` (AstroUI) plays `Content/UI/DataTables/DT_Tour.csv` in row order: Sun -> Mercury ... Neptune (cinematic warp + orbit with slow drift, narration caption + facts card) -> looking back from 80 AU -> the Milky Way with the Sun marked. `F2` start/stop, `N` next; `-AstroTour` on the command line autostarts it (classroom / kiosk). Teachers edit the CSV, not code. The pawn owns the camera: the tour asks via `OnCameraRequest`.

### Sun at a site
- `FAstroSolarGeometry` (AstroBodies): sun azimuth / elevation (geometric and refracted), sunrise / solar noon / sunset, day length, whole-day paths, for any lat/lon on any body, from the live N-body state (osculating conic for other dates) and the body's rotation model. **Validated against NOAA's solar-position algorithm: worst 0.05 deg elevation / 0.08 deg azimuth** over 4 sites x 4 dates (test `AstroVerse.Sun.MatchesNOAA`; New Delhi 21 Jun 2026: sunrise 05:24, sunset 19:22 IST).
- Earth's pole now precesses (IAU rates `PoleRA/DecRateDegPerCentury` in DT_Planets): without them the Sun was 0.2 deg off in 2026.
- `UAstroSiteSubsystem` + `AAstroSiteActor` (AstroRendering): stand at a place (pause menu fields, `astro.Site <lat> <lon> [utc_offset] [name]`, `astro.Site.On <Body> ...`): compass rose with the eight vastu directions (Uttara, Ishanya, Purva, Agneya, Dakshina, Nairutya, Paschima, Vayavya), the Sun's paths for today / 21 Jun / 21 Dec / equinox with local-hour marks on a viewer-centred sky dome, a 1 m gnomon whose shadow the engine casts, and a HUD panel (sun now, rise/noon/set with azimuths, daylight, shadow length and direction). Run the clock (`[ ]`) to watch a day or a year.
- **Latitude conventions** (`AstroGeodesy.h`): GPS and Earth maps are geodetic; MOLA/LOLA are planetocentric. `bGeodeticLatitude` in DT_Terrain (Earth) makes the DEM, ocean mask, surface/terrain shaders (`LatScale`) and landing use geodetic latitude — otherwise a site lands up to 21 km from where the coordinates say.

### Real elevation data
- `python Tools/Data/fetch_dems.py` downloads (git-ignored, ~900 MB) into `Content/Bodies/Terrain/DEM/`: Moon LRO LOLA LDEM_64 (474 m/px), Mars MGS MOLA MEGDR 32 px/deg (1.85 km/px), Earth NOAA ETOPO 2022 at 2 arc-min (3.7 km/px), each with a `.json` descriptor. `FEquirectMap::LoadRawDEM` reads them at full resolution (int16, ~0.8 GB RAM total, ~1 s load). Procedural relief then only adds detail below the DEM pixel, scaled by local ruggedness so plains stay flat. Missing files fall back to the `Fallback*` procedural columns. Test `AstroVerse.Terrain.RealElevation` checks Everest, Tibet, Delhi, Mariana, Olympus Mons, Hellas, Tycho.
- Earth at 3.7 km is regional, not plot-level; a site's ground height is right to tens of metres on plains. Finer local DEMs (SRTM 30 m tiles) drop in the same way if needed.

### Milky Way in the sky
- The sky is placed in galactic coordinates, so from any planet or site the band sits where it really is and turns with the body. Two renditions (pause menu, `astro.Sky.MilkyWay 0|1`): **Enhanced** (default) draws the map's stars plus a smooth procedural band - bulge toward Sagittarius, Great Rift dust, star clouds, warm core / violet-blue haze, styled like a long-exposure photo - because the photographed band is ~1% of white and can't be stretched (8-bit JPEG blotches). **Realistic** shows the photograph as is. Only empty sky is touched; bodies, the Sun and exposure are unchanged, and a sunlit sky still washes it out on a planet by day.
- `V` (`astro.Sky.MilkyWayGuide`) adds labels: galactic centre, anticentre, the Sun's direction of motion, the galactic plane.
- The Sun from anywhere: off screen, a HUD pointer shows its direction, distance and light travel time; `U` (`astro.Face <Body>`) turns to face it.
- Far away, the Sun becomes a flux-conserving point source (corona shader `PointSigma`) so it stays the brightest point in the sky from beyond Neptune.

### Small bodies, dwarf planets, more moons (2026-09-28)
- `AAstroBeltActor` (AstroRendering): main asteroid belt (20k, Kirkwood gaps at the 3:1, 5:2, 7:3, 2:1 resonances), Jupiter Trojans (3k at L4/L5), Kuiper belt (10k: cold classical, Plutinos at 39.4 AU, hot classical). Statistical ensembles of heliocentric Kepler orbits solved per frame on worker threads - never N-body (fidelity rule). Constant-size instanced dots through scaled space; a belt fades when it shrinks on screen. Toggle `Belts` / `astro.UI.Belts`.
- Dwarf planets Ceres, Pluto (system barycenter), Eris, Haumea, Makemake are `Planet`-type N-body particles (cheap); Charon, Mimas, Enceladus, Tethys, Dione, Rhea, Iapetus, Miranda, Ariel, Umbriel, Titania, Oberon, Triton are moons. Orbits: JPL Horizons osculating elements at J2000 (`Tools/Data/fetch_elements.py`); rotation IAU WGCCRE 2015; surfaces procedural until maps are sourced.

### UI and controls (2026-09-28)
- Solar System Scope-style: free cursor, drag to look, click to select (disc, dot or label), double-click to orbit, K to lock. Orbit lines are screen-space polylines drawn by `UAstroHUDWidget::NativePaint` from osculating elements; body list (left) with the selection's moons; toggles (Orbits, Labels, Belts, Milky Way, Sky guide).
- Sun looks (`astro.Sun.Look`, menu): natural (default; white as seen from space, surface visible), stylized (Solar System Scope orange), physical. Exposure is capped at EV 23.5 (FP16 cached-lighting limit) - only within ~0.1 AU of the Sun.

### Real-world Earth tiles (2026-09-28)
- `UAstroEarthTilesSubsystem` (AstroRendering) bridges to Cesium for Unreal by reflection (no build dependency): below `ActivateBelowKm` over Earth, in Earth's co-rotating frame (axes already match Cesium's +X east / +Y south / +Z up), it spawns a georeference at our render origin (kept on it through rebases) and a tileset (Google Photorealistic 3D Tiles by API key, or a Cesium ion asset), hiding our Earth there. Keys live in per-user GameUserSettings, never git. Untested until the plugin is installed; see SETUP.md section 6 for setup and known limitations.

### Missions (2026-09-28)
- `UAstroMissionSubsystem` (AstroTravel) directs a crewed mission: T-10 countdown on a real pad (the active Earth site, else Sriharikota; jumps to 10:00 local if it is dark), ascent along a tabulated gravity-turn profile to a 200 km orbit (T+0-20 s at 1x, then ~9x; Max-Q, MECO + staging with the booster falling away, second-stage ignition, fairing separation, SECO at T+9:00, 7.8 km/s), coast and rendezvous with the ring ship *Odyssey* (our own design, not a film replica), docking, crew transfer, then `BeginTravelWithStyle(RealFlight)` with the ship riding in front of the camera (`OnPathApplied`), and parking in orbit at the destination. Launch through docking is in Earth's body-fixed frame (the pad stays put). The camera is placed *before* the vehicles each frame (they are positioned in the frame it sets up).
- Vehicles: `AAstroRocketActor`, `AAstroRingShipActor` - engine basic shapes, `M_ShipHull`, and the additive `M_Exhaust` plume (fades toward the open end, so the cone cap never shows). Start: pause menu "Mission", or `astro.Mission.Launch [Body] [lat lon]`; `astro.Mission.Abort`.

## Decided — travel (Phase 9, decided 2026-09-27)

- **Two player-selectable transit styles**, both built: a **cinematic warp** (fixed camera sequence with a stylized warp effect) and a **player-piloted ship** the user flies through the warp. The choice is a user setting, not a build-time switch.
- **Third style, real-time flight (added 2026-09-28, now the default):** a straight flight through real space from where you are to the destination - no tunnel; distance covered grows exponentially from the start and shrinks exponentially into the arrival, so the cruise reaches hundreds to thousands of c and every decade of distance takes the same screen time (~1.6x the warp duration, `RealFlightTimeScale`). HUD shows speed in c and distance to go. `astro.Travel.Style RealFlight`.
- **God-mode clock during transit is a user setting:** either pause for the duration of the trip, or keep running at the current timescale.
- Defaults until someone says otherwise: destination is picked from the body list / by pointing at a body; transit duration grows with the log of distance so short hops stay short.

## Decided — design interpretations (2026-09-27)

- **Planets are always N-body.** The Sun + 8 planets are integrated with full mutual gravity at every timescale (this is cheap: 9 particles). The fidelity tiers govern **moons** and render/detail budget: a Dormant moon follows an analytic conic about its parent's N-body position; an Active moon is split out as its own N-body particle. While its moons are Dormant, a planet's N-body particle represents the planet-system barycenter.
- **Scaled-space rendering for far bodies.** Beyond a render-distance threshold, a body is drawn at a monotonically compressed distance and scaled down by exactly the same factor, so its angular size and draw order stay physically correct. This is a rendering technique (as in KSP), not artistic distance compression — simulation positions are always true scale.
- **Moon coverage v1:** curated set — Earth's Moon, Phobos, Deimos, Io, Europa, Ganymede, Callisto, Titan. Adding more is a data change.
- **Sim time** is seconds since J2000.0 (TDB, treated as uniform). Positions are meters in the J2000 ecliptic frame, origin at the solar-system barycenter.

## Rendering notes (Phase 6)

- Bodies are shaded by custom HLSL (Tools/Editor/shaders/*.hlsl, inlined into materials by setup_content.py) from a per-body sun direction and illuminance computed from the simulation — never from the scene light, which is only valid near the camera in scaled space. The star's directional light is kept, at the physically right lux, for lit meshes near the camera (terrain, craft).
- Exposure is an incident-light meter on the real sunlight at the camera (EV100 = log2(E/2.5)); `astro.Render.ExposureCompensation` offsets it. Stars are held at a constant display brightness (StarDisplayBrightness) — a cinematic choice, flagged in settings.
- Planet textures: Solar System Scope, CC BY 4.0 — the credits screen must show "Planet textures by Solar System Scope (solarsystemscope.com), CC BY 4.0" (see SourceArt/Textures/SolarSystemScope/ATTRIBUTION.md).
- Galilean moons, Titan, Phobos, Deimos are procedural until maps are sourced.

## Surfaces and locomotion (Phase 8)

- `FBodyTerrain` (AstroBodies, `DT_Terrain.csv`) is the single height model: the walker stands on it and `AAstroTerrainActor`'s clipmap draws it, so what you see is what you stand on. Relief is procedural (fBm + micro-relief + Pike-law craters) until real DEMs are sourced; `HeightmapFile` / `OceanMaskFile` take equirect images from `Content/Bodies/Terrain/` (staged as non-UFS).
- Gas and ice giants have no solid surface: you can fly down to the cloud tops but not land or walk there (design decision, 2026-09-27).
- Pawns: shared `AAstroPawnBase` — reference body by sphere of influence, inertial frame in space, co-rotating frame below 100 km, altitude-scaled flight, kinematic walking with each body's real surface gravity. `AAstroGameMode` picks `AAstroVRPawn` when a headset is active. The VR pawn is built but untested without a headset.

## Open items not yet scoped

- VR: parked (built, untested without a headset).
- Plot-level elevation for sites (SRTM / local survey tiles); building footprints for shading studies.


- Asteroid belt: done (see Small bodies). Comets and named asteroids (Vesta, Pallas...) are a data change away.
