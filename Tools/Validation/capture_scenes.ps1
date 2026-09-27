# Launches AstroVerse in a window, captures validation screenshots, and exits.
# Screenshots land in Saved\Screenshots\WindowsEditor\. See CLAUDE.md Phase 4.
#
#   powershell -ExecutionPolicy Bypass -File Tools\Validation\capture_scenes.ps1
#
# Shot 1: Earth from 50,000 km, true scale (checks lighting/phase and near-field placement).
# Shot 2: inner solar system from 3.5 AU above the ecliptic after ~2 simulated years at
#         30 days/s, with orbit trails and planet radii exaggerated 1500x (display only).
param(
    [string]$Engine = "C:\Program Files\Epic Games\UE_5.8\Engine",
    [int]$Width = 1600,
    [int]$Height = 900
)

$Repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Shots = Join-Path $Repo "Saved\Screenshots\WindowsEditor"
Get-ChildItem "$Shots\*.png" -ErrorAction SilentlyContinue | Remove-Item

$cmds = @(
    "astro.Debug.DelayedExec 3 astro.Camera.LookAt Earth",
    "astro.Debug.DelayedExec 6 shot",
    "astro.Debug.DelayedExec 8 astro.Origin.Set 0 0 3.5",
    "astro.Debug.DelayedExec 8 astro.Camera.LookDown",
    "astro.Debug.DelayedExec 8 astro.Debug.RadiusScale 1500",
    "astro.Debug.DelayedExec 8 astro.Debug.StarRadiusScale 15",
    "astro.Debug.DelayedExec 8 astro.Debug.Trails 1",
    "astro.Debug.DelayedExec 8 astro.Time.Scale 2592000",
    "astro.Debug.DelayedExec 32 shot",
    "astro.Debug.DelayedExec 33 astro.Time.Status",
    "astro.Debug.DelayedExec 35 quit"
) -join ","

$uproject = Join-Path $Repo "AstroVerse.uproject"
$p = Start-Process -FilePath "$Engine\Binaries\Win64\UnrealEditor.exe" -PassThru -Wait -ArgumentList `
    "`"$uproject`" -game -windowed -ResX=$Width -ResY=$Height -nosplash -log=Capture.log -ExecCmds=`"$cmds`""
Write-Host "Game exited with code $($p.ExitCode)"
Get-ChildItem "$Shots\*.png" | ForEach-Object { Write-Host $_.FullName }
