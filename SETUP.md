# Setup — manual steps (do this before pointing Claude Code at the repo)

This part genuinely can't be automated by any CLI or browser agent — Unreal Engine is a large, signed installer gated behind an Epic account login and a EULA click-through in a GUI launcher. Budget 1-3 hours depending on your connection (the engine download alone is 30-100GB).

1. **Create/sign in to an Epic Games account** at epicgames.com — required to download the engine.
2. **Install the Epic Games Launcher**, then under the Unreal Engine tab, install **Unreal Engine 5.4 or newer**. This is the step that takes the bulk of the time.
3. **Install Visual Studio 2022** (Community edition is fine) with the **"Game development with C++"** workload checked in the installer — this is what actually compiles the C++ modules in this repo.
4. **Install Git** if it isn't already, and initialize this folder as a repo (`git init`) if you want version history from the start.
5. Once the engine is installed, **right-click `AstroVerse.uproject` → "Generate Visual Studio project files"** (Windows) or the Mac equivalent — this creates the `.sln` and lets both Visual Studio and Unreal's own build tool see the modules in `Source/`.
6. Open `AstroVerse.uproject` in Unreal Editor once to confirm it loads with no errors — expect it to prompt to rebuild modules the first time, since only stub headers exist so far. Let it build.

**Once steps 1-6 are done, tell Claude Code so — it should not attempt to invoke `UnrealBuildTool`, open the project, or run any build command until you've confirmed this.** From there, hand it `CLAUDE.md` (it reads this automatically if you run `claude` from inside this folder) and let it work through the phase checklist.

## VR testing

For VR-specific phases (8 onward), you'll also want a headset connected — a Quest over Link/Air Link, or any OpenXR-compatible headset — since VR framerate/comfort issues generally aren't caught by desktop-only testing.

## What Claude Code can and can't do from here

**Can:** write and edit every `.h`/`.cpp` file in `Source/`, author Data Tables as CSV, edit `.Build.cs` and `.Target.cs` files, run `git`, and — once you confirm the engine is installed — invoke `UnrealBuildTool` and the editor's commandlet tools from the terminal to compile and run automated checks.

**Can't:** install Unreal Engine itself, click through the Epic Games Launcher GUI, accept the EULA on your behalf, or install Visual Studio. Those need a human, once, up front.
