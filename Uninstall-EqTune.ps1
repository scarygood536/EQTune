param(
    [string]$PluginDirectory = (Join-Path $env:CommonProgramFiles "VST3")
)

$ErrorActionPreference = "Stop"
$target = Join-Path $PluginDirectory "EqTune.vst3"
if (Test-Path -LiteralPath $target) {
    Remove-Item -LiteralPath $target -Recurse -Force
    Write-Host "Removed the EqTune plugin. Restart Wave Link to refresh its effects list."
} else {
    Write-Host "EqTune is not installed in $PluginDirectory."
}
Write-Host "No Windows, game, or Wave Link routing settings were changed."
Write-Host "Host-saved presets are kept. Remove or disable the effect in Wave Link for immediate normal playback."
