param(
    [string]$PluginDirectory = (Join-Path $env:CommonProgramFiles "VST3")
)

$ErrorActionPreference = "Stop"
$source = Join-Path $PSScriptRoot "EqTune.vst3"
$target = Join-Path $PluginDirectory "EqTune.vst3"
$binary = Join-Path $source "Contents\x86_64-win\EqTune.vst3"
if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
    throw "Extract the complete plugin ZIP first. The EqTune.vst3 bundle must be beside this installer."
}
if ([IO.Path]::GetFullPath($source).TrimEnd('\') -eq [IO.Path]::GetFullPath($target).TrimEnd('\')) {
    throw "Run this installer from the extracted download folder, not the installed VST3 folder."
}
if (Test-Path -LiteralPath $target) {
    throw "EqTune is already installed. Close Wave Link and run Uninstall-EqTune.cmd before installing this version. Save a host preset first if you want to keep a tune."
}
New-Item -ItemType Directory -Path $PluginDirectory -Force | Out-Null
try {
    Copy-Item -LiteralPath $source -Destination $target -Recurse
} catch {
    throw "Installation failed. Close Wave Link and retry with administrator permission. A partial EqTune.vst3 folder may need removing with the uninstaller. Details: $($_.Exception.Message)"
}
Write-Host "Installed $target"
Write-Host "Restart Wave Link 3, rescan VST3 effects if needed, and add EqTune to your game channel."
Write-Host "No VB-CABLE, extra virtual input, or standalone EqTune app is required."
