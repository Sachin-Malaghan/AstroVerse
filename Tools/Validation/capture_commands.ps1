# Generic scripted capture: shots are separated by '|'; each shot is a ';'-separated list of
# console commands run before one screenshot. Screenshots land in Saved\Screenshots\WindowsEditor\.
#   powershell -ExecutionPolicy Bypass -File Tools\Validation\capture_commands.ps1 `
#       -Shots "astro.Origin.Land Earth 0 0 2;astro.Camera.Look 90 5|astro.Camera.Frame Moon 3 30 5"
# See CLAUDE.md Phase 4.
param(
    [Parameter(Mandatory = $true)][string]$Shots,
    [string]$Setup = "astro.Time.Pause",
    [string]$Engine = "C:\Program Files\Epic Games\UE_5.8\Engine",
    [int]$Width = 1600,
    [int]$Height = 900,
    [double]$SettleSeconds = 6
)

$Repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ShotDir = Join-Path $Repo "Saved\Screenshots\WindowsEditor"
Get-ChildItem "$ShotDir\*.png" -ErrorAction SilentlyContinue | Remove-Item

$cmds = @($Setup -split ";" | Where-Object { $_ })
$t = 4.0
foreach ($shot in ($Shots -split '\|')) {
    foreach ($c in ($shot -split ";")) { if ($c.Trim()) { $cmds += "astro.Debug.DelayedExec $t $($c.Trim())" } }
    $t += $SettleSeconds
    $cmds += "astro.Debug.DelayedExec $t shot"
    $t += 1.0
}
$cmds += "astro.Debug.DelayedExec $($t + 1) quit"

$uproject = Join-Path $Repo "AstroVerse.uproject"
$p = Start-Process -FilePath "$Engine\Binaries\Win64\UnrealEditor.exe" -PassThru -Wait -ArgumentList `
    "`"$uproject`" -game -windowed -ResX=$Width -ResY=$Height -nosplash -log=Capture.log -ExecCmds=`"$($cmds -join ',')`""
Write-Host "Game exited with code $($p.ExitCode)"
Get-ChildItem "$ShotDir\*.png" | Sort-Object Name | ForEach-Object { Write-Host $_.FullName }
