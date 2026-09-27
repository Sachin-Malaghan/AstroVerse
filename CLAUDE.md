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

# Run AstroCore automation tests headless (results in Saved/Logs/Tests.log)
"<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<repo>\AstroVerse.uproject" -nullrhi -unattended -nosplash -nopause -ExecCmds="Automation RunTests AstroVerse.Core; Quit" -TestExit="Automation Test Queue Empty" -log=Tests.log
```

OpenXR "failed to find active runtime" errors in headless runs are expected when no headset runtime is installed.

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
- [ ] **Phase 2 — AstroBodies.** Implement `FBodyRegistry::LoadFromDataTables`; author `DT_Planets.csv` with real masses/radii/orbital elements for the Sun + 8 planets; wire `ACelestialBody::Tick` to read integrator output.
- [ ] **Phase 3 — AstroTime.** Implement `UTimeController` play/pause/rewind/timescale; confirm every Phase 1-2 class reads time from here, not `GetWorld()`.
- [ ] **Phase 4 — Minimal proof-of-motion scene.** Placeholder spheres orbiting correctly in `L_SolarSystem`, no art pass yet — this validates the physics visually before any rendering investment.
- [ ] **Phase 5 — AstroActivation.** Implement `FActivationManager` tier promotion/demotion, the 3-lock cap with demotion-of-oldest, reference-frame handling.
- [ ] **Phase 6 — AstroRendering.** Corona/flare, atmospheric scattering, ring shaders; split Lumen/Nanite budgets per platform (desktop full, VR trimmed for 90Hz).
- [ ] **Phase 7 — Scale and precision infrastructure.** Confirm floating-origin behavior at true scale; build the Solar-System ↔ Galaxy scale-domain transition.
- [ ] **Phase 8 — AstroInput + AstroApp pawns.** Desktop flycam pawn first, then VR pawn with room-scale/teleport locomotion.
- [ ] **Phase 9 — AstroTravel.** **Blocked** — the transit style (player-piloted warp vs. fixed cinematic vs. instant cut) is an open decision; do not implement until it's resolved. See the "Blocked" note below.
- [ ] **Phase 10 — AstroGalaxy.** Milky Way disc representation, Sun position/velocity marker, scale-domain transition polish.
- [ ] **Phase 11 — AstroUI.** God-mode time HUD, teaching-mode facts panels, VR world-space diegetic panels.
- [ ] **Phase 12 — Platform polish.** VR performance budget pass; groundwork for the later mobile port.

## Blocked — needs a decision before Phase 9

The travel/warp transit style is undecided: a cinematic camera sequence, a flyable ship the player actively pilots through a stylized warp, or an instant cut with a loading beat. Also open: whether travel time scales with anything, how the destination gets selected, and whether the god-mode clock keeps advancing during transit. Do not guess an implementation for Phase 9 — surface this to the user and wait for a decision, or work on any other unblocked phase in the meantime.

## Open items not yet scoped

- Asteroid belt representation: GPU-instanced/Niagara-driven per the fidelity system (never individually N-body simulated), but the exact rendering approach isn't decided.
- Moon coverage for v1: all real moons system-wide, or a curated subset first (Earth's Moon, the Galilean moons, Titan) with the architecture supporting the rest later.
