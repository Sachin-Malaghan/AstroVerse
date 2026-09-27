# Captures a rendering gallery of key bodies at true scale (clock paused), then exits.
# Screenshots land in Saved\Screenshots\WindowsEditor\ in the order listed below.
#   powershell -ExecutionPolicy Bypass -File Tools\Validation\capture_gallery.ps1
# See CLAUDE.md Phase 6.
param(
    [string]$Engine = "C:\Program Files\Epic Games\UE_5.8\Engine",
    [int]$Width = 1600,
    [int]$Height = 900,
    [double]$SettleSeconds = 7
)

# BodyID, distance (radii), phase angle (deg), elevation (deg), FOV
$shots = @(
    @("Earth", 3.2, 55, 15, 40),
    @("Earth", 2.6, 150, 10, 50),
    @("Moon", 3.5, 25, 5, 40),
    @("Mars", 3.5, 35, 10, 40),
    @("Jupiter", 3.5, 20, 5, 40),
    @("Saturn", 7.0, 35, 22, 40),
    @("Venus", 4.0, 135, 5, 40),
    @("Sun", 40.0, 0, 20, 30),
    @("Earth", 14.0, 70, 12, 60)
)

$Repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ShotDir = Join-Path $Repo "Saved\Screenshots\WindowsEditor"
Get-ChildItem "$ShotDir\*.png" -ErrorAction SilentlyContinue | Remove-Item

$cmds = @("astro.Time.Pause")
$t = 4.0
foreach ($s in $shots) {
    $cmds += "astro.Debug.DelayedExec $t astro.Camera.Frame $($s[0]) $($s[1]) $($s[2]) $($s[3])"
    $cmds += "astro.Debug.DelayedExec $t astro.Camera.FOV $($s[4])"
    $t += $SettleSeconds   # let textures stream and exposure adapt
    $cmds += "astro.Debug.DelayedExec $t shot"
    $t += 1.0
}
$cmds += "astro.Debug.DelayedExec $($t + 1) quit"

$uproject = Join-Path $Repo "AstroVerse.uproject"
$p = Start-Process -FilePath "$Engine\Binaries\Win64\UnrealEditor.exe" -PassThru -Wait -ArgumentList `
    "`"$uproject`" -game -windowed -ResX=$Width -ResY=$Height -nosplash -log=Gallery.log -ExecCmds=`"$($cmds -join ',')`""
Write-Host "Game exited with code $($p.ExitCode)"
$i = 0
Get-ChildItem "$ShotDir\*.png" | Sort-Object Name | ForEach-Object { Write-Host "$($_.Name)  <- $($shots[$i][0])"; $i++ }
