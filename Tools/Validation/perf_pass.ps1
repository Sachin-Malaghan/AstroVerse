# Scripted performance pass: visits representative scenes and logs one "AstroPerf:" line per
# scene (average frame / game / render / GPU ms against the active budget). See CLAUDE.md Phase 12.
#
#   powershell -ExecutionPolicy Bypass -File Tools\Validation\perf_pass.ps1              # desktop
#   powershell -ExecutionPolicy Bypass -File Tools\Validation\perf_pass.ps1 -Budget vr   # VR, no headset
#
# -Budget vr emulates a headset: -emulatestereo renders two views side by side, and screen
# percentage 200 on a 1920x1032 window gives ~1920x2064 per eye (Quest 3 / Index class).
param(
    [ValidateSet("desktop", "vr", "mobile")][string]$Budget = "desktop",
    [string]$Engine = "C:\Program Files\Epic Games\UE_5.8\Engine",
    [double]$SampleSeconds = 4,
    [double]$SettleSeconds = 4
)

$Repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Scenes = [ordered]@{
    "EarthOrbit"   = "astro.Camera.Frame Earth 3.2 55 15"
    "SaturnRings"  = "astro.Camera.Frame Saturn 4 40 20"
    "MarsSurface"  = "astro.Origin.Land Mars 0 0 2;astro.Camera.Look 90 5"
    "MoonSurface"  = "astro.Origin.Land Moon 10 20 2;astro.Camera.Look 60 10"
    "EarthSurface" = "astro.Origin.Land Earth 45 7 2;astro.Camera.Look 120 8"
    "InnerSystem"  = "astro.Origin.Set 0 0 3.5;astro.Camera.LookDown"
    "Galaxy"       = "astro.Domain Galaxy"
}

$cmds = @("astro.Time.Pause", "astro.Render.Budget $Budget", "astro.Render.ApplyBudget", "t.MaxFPS 0", "r.VSync 0")
$extra = ""
$w = 1600; $h = 900
if ($Budget -eq "vr") {
    $cmds += "r.ScreenPercentage 200"
    $extra = "-emulatestereo"
    $w = 1920; $h = 1032
}
$t = 5.0
foreach ($name in $Scenes.Keys) {
    $k = 0
    foreach ($c in ($Scenes[$name] -split ";")) { $cmds += "astro.Debug.DelayedExec $($t + 0.3 * $k) $($c.Trim())"; $k++ }
    $t += $SettleSeconds
    $cmds += "astro.Debug.DelayedExec $t astro.Perf.Sample $SampleSeconds $name"
    $t += $SampleSeconds + 1.0
}
$cmds += "astro.Debug.DelayedExec $($t + 1) quit"

$uproject = Join-Path $Repo "AstroVerse.uproject"
$log = "Perf_$Budget.log"
$p = Start-Process -FilePath "$Engine\Binaries\Win64\UnrealEditor.exe" -PassThru -Wait -ArgumentList `
    "`"$uproject`" -game -windowed -ResX=$w -ResY=$h -nosplash $extra -log=$log -ExecCmds=`"$($cmds -join ',')`""
Write-Host "Game exited with code $($p.ExitCode)"
Select-String -Path (Join-Path $Repo "Saved\Logs\$log") -Pattern "AstroPerf:" | ForEach-Object { ($_.Line -split "AstroPerf: ")[-1] }
