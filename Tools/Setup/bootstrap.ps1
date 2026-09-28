# One-shot setup for a fresh clone of AstroVerse (Windows). See SETUP.md.
#
#   powershell -ExecutionPolicy Bypass -File Tools\Setup\bootstrap.ps1
#   powershell -ExecutionPolicy Bypass -File Tools\Setup\bootstrap.ps1 -RunTests -Launch
#
# Steps: check prerequisites -> pull Git LFS assets -> generate project files -> compile the
# editor target -> download the real elevation data (~900 MB, optional) -> run the automated
# tests (optional) -> launch (optional). Safe to re-run: finished steps are skipped or cheap.
param(
    [string]$Engine = "",          # UE root, e.g. "C:\Program Files\Epic Games\UE_5.8" (auto-detected if empty)
    [switch]$SkipDEMs,             # skip the elevation download (terrain falls back to procedural)
    [switch]$RunTests,
    [switch]$Launch
)
$ErrorActionPreference = "Stop"
$Repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Project = Join-Path $Repo "AstroVerse.uproject"
function Step($text) { Write-Host ""; Write-Host "==> $text" -ForegroundColor Cyan }
function Fail($text) { Write-Host "ERROR: $text" -ForegroundColor Red; exit 1 }

# ---------------------------------------------------------------- 1. prerequisites
Step "Checking prerequisites"
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Fail "Git is not installed (https://git-scm.com)." }
& git lfs version *> $null
if ($LASTEXITCODE -ne 0) { Fail "Git LFS is not installed (https://git-lfs.com) - the Unreal assets are stored with it." }

if (-not $Engine) {
    $key = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8"
    if (Test-Path $key) { $Engine = (Get-ItemProperty $key).InstalledDirectory }
    elseif (Test-Path "C:\Program Files\Epic Games\UE_5.8") { $Engine = "C:\Program Files\Epic Games\UE_5.8" }
}
if (-not $Engine -or -not (Test-Path (Join-Path $Engine "Engine\Build\BatchFiles\Build.bat"))) {
    Fail "Unreal Engine 5.8 not found. Install it from the Epic Games Launcher, or pass -Engine <path>."
}
Write-Host "  Unreal Engine: $Engine"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = if (Test-Path $vswhere) { & $vswhere -latest -requires Microsoft.VisualStudio.Workload.NativeGame -property installationPath } else { "" }
if (-not $vs) { Fail "Visual Studio 2022 with the 'Game development with C++' workload is required (see .vsconfig)." }
Write-Host "  Visual Studio: $vs"

$python = Get-Command python3 -ErrorAction SilentlyContinue
if (-not $python) { $python = Get-Command python -ErrorAction SilentlyContinue }
if (-not $python -and -not $SkipDEMs) { Write-Host "  Python not found: skipping the elevation download (install Python 3 to get real terrain)." -ForegroundColor Yellow; $SkipDEMs = $true }

# ---------------------------------------------------------------- 2. LFS content
Step "Fetching Git LFS assets (textures, materials, level)"
Push-Location $Repo
& git lfs install --local *> $null
& git lfs pull
if ($LASTEXITCODE -ne 0) { Pop-Location; Fail "git lfs pull failed." }
$probe = Get-Content -Path (Join-Path $Repo "Content\Maps\L_SolarSystem.umap") -TotalCount 1 -ErrorAction SilentlyContinue
if ($probe -like "version https://git-lfs*") { Pop-Location; Fail "Assets are still LFS pointers - check your Git LFS access to the repository." }
Pop-Location

# ---------------------------------------------------------------- 3. project files + build
$Build = Join-Path $Engine "Engine\Build\BatchFiles\Build.bat"
Step "Generating Visual Studio project files"
& $Build -projectfiles -project="$Project" -game -rocket -progress
if ($LASTEXITCODE -ne 0) { Fail "Project file generation failed." }

Step "Compiling AstroVerseEditor (first build takes a while)"
& $Build AstroVerseEditor Win64 Development -project="$Project" -waitmutex
if ($LASTEXITCODE -ne 0) { Fail "Build failed - see the output above." }

# ---------------------------------------------------------------- 4. elevation data
if (-not $SkipDEMs) {
    Step "Downloading real elevation data (NASA LOLA / MOLA, NOAA ETOPO 2022, ~900 MB)"
    & $python.Source (Join-Path $Repo "Tools\Data\fetch_dems.py")
    if ($LASTEXITCODE -ne 0) { Write-Host "  Elevation download failed; terrain will be procedural until it succeeds (re-run this script)." -ForegroundColor Yellow }
}

# ---------------------------------------------------------------- 5. tests / launch
if ($RunTests) {
    Step "Running automated tests"
    & powershell -ExecutionPolicy Bypass -File (Join-Path $Repo "Tools\Validation\run_tests.ps1") -Engine (Join-Path $Engine "Engine")
}

$Editor = Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor.exe"
Write-Host ""
Write-Host "Setup complete." -ForegroundColor Green
Write-Host "  Play:         `"$Editor`" `"$Project`" -game -windowed -ResX=1600 -ResY=900"
Write-Host "  Guided tour:  add -AstroTour to that command"
Write-Host "  Editor:       double-click AstroVerse.uproject"
if ($Launch) {
    Start-Process -FilePath $Editor -ArgumentList "`"$Project`" -game -windowed -ResX=1600 -ResY=900 -nosplash"
}
