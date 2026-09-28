@echo off
rem Starts AstroVerse in a window (see SETUP.md). Run Tools\Setup\bootstrap.ps1 once first.
set "UE=C:\Program Files\Epic Games\UE_5.8"
for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.8" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE=%%b"
start "" "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0AstroVerse.uproject" -game -windowed -ResX=1600 -ResY=900 -nosplash %*
