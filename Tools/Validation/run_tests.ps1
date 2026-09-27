# Runs the AstroVerse automation tests headless and prints a pass/fail summary.
#   powershell -ExecutionPolicy Bypass -File Tools\Validation\run_tests.ps1 [-Filter AstroVerse.Bodies]
# Exit code is 0 only if every test passed. Full log: Saved\Logs\Tests.log
param(
    [string]$Engine = "C:\Program Files\Epic Games\UE_5.8\Engine",
    [string]$Filter = "AstroVerse"
)

$Repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$uproject = Join-Path $Repo "AstroVerse.uproject"
$log = Join-Path $Repo "Saved\Logs\Tests.log"

$p = Start-Process -FilePath "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -PassThru -Wait -NoNewWindow `
    -RedirectStandardOutput (Join-Path $env:TEMP "astro_tests_stdout.txt") -ArgumentList `
    "`"$uproject`" -nullrhi -unattended -nosplash -nopause -ExecCmds=`"Automation RunTests $Filter; Quit`" -TestExit=`"Automation Test Queue Empty`" -log=Tests.log"

$results = Select-String -Path $log -Pattern "Test Completed\. Result=\{(\w+)\} Name=\{[^}]+\} Path=\{([^}]+)\}"
$passed = 0; $failed = 0
foreach ($r in $results) {
    $status = $r.Matches[0].Groups[1].Value
    $path = $r.Matches[0].Groups[2].Value
    if ($status -eq "Success") { $passed++; Write-Host "  PASS  $path" } else { $failed++; Write-Host "  FAIL  $path" -ForegroundColor Red }
}
Select-String -Path $log -Pattern "LogAutomationController: Error: " | ForEach-Object {
    Write-Host ("        " + ($_.Line -replace '^.*LogAutomationController: Error: ', '')) -ForegroundColor Red
}
Write-Host "$passed passed, $failed failed"
if ($failed -gt 0 -or $passed -eq 0) { exit 1 } else { exit 0 }
