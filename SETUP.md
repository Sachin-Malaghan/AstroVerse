# Setting up AstroVerse

Repository: https://github.com/Sachin-Malaghan/AstroVerse

## 1. One-time installs (manual, ~1-3 hours, mostly the engine download)

These need a person: they are GUI installers behind accounts and licence prompts.

| What | Notes |
|---|---|
| **Unreal Engine 5.8** | Epic Games account -> Epic Games Launcher -> Unreal Engine tab -> install 5.8 (30-100 GB). The project is associated with 5.8. |
| **Visual Studio 2022** (Community is fine) | Workload **"Game development with C++"**. The repo's `.vsconfig` lists the exact components: open the VS Installer -> More -> Import configuration -> pick `.vsconfig`. |
| **Git** + **Git LFS** | https://git-scm.com and https://git-lfs.com. All Unreal assets, textures and the level are stored with LFS (~190 MB). |
| **Python 3** (optional) | Only for downloading the real elevation data. Without it the terrain is procedural. |

## 2. Clone and bootstrap

```
git clone https://github.com/Sachin-Malaghan/AstroVerse.git
cd AstroVerse
powershell -ExecutionPolicy Bypass -File Tools\Setup\bootstrap.ps1 -RunTests -Launch
```

`bootstrap.ps1` does everything after the installs and is safe to re-run:

1. Checks Git LFS, finds Unreal Engine 5.8 (registry or default path; pass `-Engine <path>` otherwise) and Visual Studio's C++ game workload.
2. `git lfs pull` - fetches the real textures, materials and level (a clone without LFS gets tiny pointer files and the project won't open).
3. Generates the Visual Studio solution. Launcher builds of UE have no `GenerateProjectFiles.bat` and the right-click menu may be missing, so this uses `Build.bat -projectfiles`.
4. Compiles `AstroVerseEditor` (first build ~5-15 min).
5. Downloads the real elevation data with `Tools/Data/fetch_dems.py` (~900 MB, public NASA/NOAA data, not stored in git) into `Content/Bodies/Terrain/DEM/`. Skip it with `-SkipDEMs`.
6. `-RunTests`: runs all automated tests (expect them all to pass; OpenXR "no active runtime" messages are normal without a headset).
7. `-Launch`: starts the game in a window.

### Doing it by hand instead

```
git lfs install
git lfs pull
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="%CD%\AstroVerse.uproject" -game -rocket -progress
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AstroVerseEditor Win64 Development -project="%CD%\AstroVerse.uproject" -waitmutex
python Tools\Data\fetch_dems.py
```

## 3. Running

- **Play:** double-click `Play.bat` (or `PlayTour.bat` for the guided tour). Same as `"C:Program Filespic GamesE_5.8ngineBinariesWin64NREALEDITOR.EXE" "<REPO>ASTROVERSE.UPROJECT" -GAME -WINDOWED -RESX=1600 -RESY=900`
- **Classroom / kiosk:** add `-AstroTour` to start the guided tour automatically.
- **Editor:** double-click `AstroVerse.uproject` (if Windows doesn't know the file type, run `"C:\Program Files (x86)\Epic Games\Launcher\Engine\Binaries\Win64\UnrealVersionSelector.exe" /fileassociations` once).
- **Controls:** F1 in game. Highlights: F orbit + wheel zoom, Home back to Earth, F2 tour, L live UTC, U face the Sun, V Milky Way guide, Esc menu (sun at a site, settings).

## 4. What is not in git, and how it comes back

| Not committed | Recreated by |
|---|---|
| `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`, `*.sln` | the build (step 2.3-2.4); shaders compile on first launch (the first run is slow) |
| `Content/Bodies/Terrain/DEM/` (real elevation, ~900 MB) | `python Tools/Data/fetch_dems.py` |

The generated Unreal assets (data tables, materials, the level) *are* committed (LFS). If you change the CSV data tables or `Tools/Editor/shaders/*.hlsl`, rebuild them:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<repo>\AstroVerse.uproject" -run=pythonscript -script="<repo>\Tools\Editor\setup_content.py" -unattended -nosplash
```

## 5. Notes

- **Network time:** in Live mode the game makes an HTTPS HEAD request to cloudflare.com / google.com / microsoft.com to read the current time (`[/Script/AstroTime.AstroTimeSettings]` in `Config/DefaultGame.ini`; set `bUseNetworkTime=False` to disable). Offline it uses the PC clock.
- **Hardware:** desktop needs a DirectX 12 GPU; VR (parked for now) needs an RTX 2070-class GPU or better and an OpenXR headset.
- **Texture credit:** planet textures by Solar System Scope (CC BY 4.0) - see `SourceArt/Textures/SolarSystemScope/ATTRIBUTION.md`; shown in the in-game credits.
- **Working with Claude Code:** `CLAUDE.md` is the project spec and records build/test commands; run `claude` from the repo folder.
