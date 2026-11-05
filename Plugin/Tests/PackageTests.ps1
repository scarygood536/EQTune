param(
    [Parameter(Mandatory = $true)]
    [string]$Archive
)

$ErrorActionPreference = "Stop"
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ("EqTune-plugin-package-test-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $testRoot | Out-Null
try {
    $extracted = Join-Path $testRoot "download"
    $pluginDirectory = Join-Path $testRoot "VST3"
    Expand-Archive -LiteralPath $Archive -DestinationPath $extracted
    foreach ($file in @(
        "EqTune.vst3\Contents\x86_64-win\EqTune.vst3",
        "EqTune.vst3\Contents\Resources\moduleinfo.json",
        "EqTune.vst3\Contents\Resources\Steinberg-SDK-LICENSE.txt",
        "EqTune.vst3\Contents\Resources\WarzoneAudioSetup.ps1",
        "Install-EqTune.cmd", "Uninstall-EqTune.cmd", "README.md", "LICENSE"
    )) {
        if (-not (Test-Path -LiteralPath (Join-Path $extracted $file) -PathType Leaf)) {
            throw "Release file missing: $file"
        }
    }
    if (Get-ChildItem -LiteralPath $extracted -Filter "*.exe" -Recurse) {
        throw "The plugin package must not ship a standalone audio app."
    }
    New-Item -ItemType Directory -Path $pluginDirectory | Out-Null
    $unrelated = Join-Path $pluginDirectory "Unrelated-plugin.txt"
    Set-Content -LiteralPath $unrelated -Value "Keep me"
    $install = Join-Path $extracted "Install-EqTune.ps1"
    $uninstall = Join-Path $extracted "Uninstall-EqTune.ps1"
    & $install -PluginDirectory $pluginDirectory
    $installedBinary = Join-Path $pluginDirectory "EqTune.vst3\Contents\x86_64-win\EqTune.vst3"
    $sourceBinary = Join-Path $extracted "EqTune.vst3\Contents\x86_64-win\EqTune.vst3"
    if ((Get-FileHash $installedBinary).Hash -ne (Get-FileHash $sourceBinary).Hash) {
        throw "Installed plugin does not match packaged binary."
    }
    & (Join-Path $PSScriptRoot "GraphicsTests.ps1") -HelperPath (Join-Path $pluginDirectory "EqTune.vst3\Contents\Resources\WarzoneAudioSetup.ps1")
    $refusedOverwrite = $false
    try {
        & $install -PluginDirectory $pluginDirectory
    } catch {
        if ($_.Exception.Message -notmatch "already installed") { throw }
        $refusedOverwrite = $true
    }
    if (-not $refusedOverwrite) { throw "Installer overwrote an existing plugin." }
    & $uninstall -PluginDirectory $pluginDirectory
    if (Test-Path -LiteralPath $installedBinary) { throw "Uninstall left the plugin binary behind." }
    if (-not (Test-Path -LiteralPath $unrelated)) { throw "Uninstall removed an unrelated plugin." }
    & $uninstall -PluginDirectory $pluginDirectory
    Write-Output "PASS: plugin-only ZIP, required files/notices, exact installed binary, overwrite refusal, safe uninstall."
} finally {
    Remove-Item -LiteralPath $testRoot -Recurse -Force
}
