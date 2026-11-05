@echo off
cd /d "%~dp0"
set "EQTUNE_INSTALL_SCRIPT=%~dp0Install-EqTune.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference = 'Stop'; try { $p = Start-Process powershell.exe -Verb RunAs -Wait -PassThru -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File ' + [char]34 + $env:EQTUNE_INSTALL_SCRIPT + [char]34); exit $p.ExitCode } catch { Write-Host $_; exit 1 }"
if errorlevel 1 echo Installation failed or was cancelled. Close Wave Link and check the error above.
pause
