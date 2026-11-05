$ErrorActionPreference = "Stop"
$cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue
if ($cmake) {
    $cmakePath = $cmake.Source
} else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "Install Visual Studio 2022 Build Tools with Desktop development with C++ and CMake tools."
    }
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw "Visual Studio C++ Build Tools were not found." }
    $cmakePath = Join-Path $visualStudio "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (-not (Test-Path $cmakePath)) { throw "Install the C++ CMake tools component in Visual Studio Installer." }
}

$build = Join-Path $PSScriptRoot "Plugin\build"
$configureArgs = @("-S", (Join-Path $PSScriptRoot "Plugin"), "-B", $build, "-G", "Visual Studio 17 2022", "-A", "x64")
$restoredSdk = Join-Path $build "sdk"
if (Test-Path (Join-Path $restoredSdk "CMakeLists.txt")) {
    $configureArgs += "-DFETCHCONTENT_SOURCE_DIR_VST3SDK=$restoredSdk"
}
& $cmakePath @configureArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }
& $cmakePath --build $build --config Release --target EqTune EqTuneDspTests EqTuneHostTests --parallel 4
if ($LASTEXITCODE -ne 0) { throw "Plugin build or Steinberg validator failed." }
$ctestPath = Join-Path (Split-Path $cmakePath) "ctest.exe"
& $ctestPath --test-dir $build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Plugin regression checks failed." }
& (Join-Path $PSScriptRoot "Plugin\Tests\GameAudioTests.ps1")
& (Join-Path $PSScriptRoot "Plugin\Tests\GraphicsTests.ps1")

$release = Join-Path $PSScriptRoot "release\EqTune-VST3"
New-Item -ItemType Directory -Path $release -Force | Out-Null
$bundle = Join-Path $build "VST3\Release\EqTune.vst3"
$packagedBundle = Join-Path $release "EqTune.vst3"
if (Test-Path $packagedBundle) { Remove-Item -LiteralPath $packagedBundle -Recurse -Force }
Copy-Item -LiteralPath $bundle -Destination $packagedBundle -Recurse
foreach ($file in @("Install-EqTune.ps1", "Uninstall-EqTune.ps1", "Install-EqTune.cmd", "Uninstall-EqTune.cmd", "README.md", "LICENSE")) {
    Copy-Item (Join-Path $PSScriptRoot $file) $release -Force
}
$sdkDirectory = if (Test-Path (Join-Path $restoredSdk "LICENSE.txt")) { $restoredSdk } else { Join-Path $build "_deps\vst3sdk-src" }
Copy-Item (Join-Path $sdkDirectory "LICENSE.txt") (Join-Path $release "Steinberg-SDK-LICENSE.txt") -Force
Copy-Item (Join-Path $sdkDirectory "LICENSE.txt") (Join-Path $packagedBundle "Contents\Resources\Steinberg-SDK-LICENSE.txt") -Force
$archive = Join-Path $PSScriptRoot "release\EqTune-VST3-win-x64.zip"
if (Test-Path $archive) { Remove-Item -LiteralPath $archive -Force }
Compress-Archive -Path (Join-Path $release "*") -DestinationPath $archive
& (Join-Path $PSScriptRoot "Plugin\Tests\PackageTests.ps1") -Archive $archive
Write-Host "Plugin-only release created: $archive"
