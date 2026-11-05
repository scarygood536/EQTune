param(
    [string]$HelperPath = (Join-Path $PSScriptRoot "..\Resources\WarzoneAudioSetup.ps1")
)

$ErrorActionPreference = "Stop"
. $HelperPath
function Assert-GameClosed {}
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Assert-Refused([scriptblock]$Operation, [string]$Message) {
    $refused = $false
    try { & $Operation | Out-Null } catch { $refused = $true }
    Assert-True $refused $Message
}

$root = Join-Path ([IO.Path]::GetTempPath()) ("EqTune-graphics-test-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $root | Out-Null
try {
    $players = Join-Path $root "players"
    New-Item -ItemType Directory -Path $players | Out-Null
    $utf8 = New-Object Text.UTF8Encoding($false)
    $targets = Get-GraphicsTargets $true
    $lines = @()
    foreach ($key in $targets.Keys) {
        $old = switch ($key) {
            { $_ -in @("VSync", "VSyncInMenu") } { "100%" }
            "NvidiaReflex" { "Disabled" }
            "MaxFpsInMenu" { "120" }
            "MaxFpsOutOfFocus" { "60" }
            "SubdivisionLevel" { "3" }
            { $_ -in @("DofEnable", "MotionBlur", "DxrMode") } { "On" }
            "VolumetricQuality" { "QUALITY_HIGH" }
            "WaterCausticsMode" { "High Quality" }
            "ParticleQuality" { "high" }
            "ShaderQuality" { "Medium" }
            { $_ -in @("ModelQuality", "DeferredPhysics") } { "High Quality" }
            "Tessellation" { "2_All" }
            default {
                if ($targets[$key] -eq "false") { "true" }
                elseif ($targets[$key] -eq "true") { "false" }
                else { "High" }
            }
        }
        $lines += "$key@0;1;2 = $old  // synthetic fixture"
    }
    $lines += @(
        "MusicVolume@0;1;2 = 0.750000",
        "Volume@0;1;2 = 0.840000",
        "EffectsVolume@0;1;2 = 1.000000",
        "Sensitivity@0;1;2 = 3.500000",
        "Fov@0;1;2 = 110",
        "Resolution@0;1;2 = 2560x1440",
        "RefreshRate@0;1;2 = 240.000",
        "RendererWorkerCount@0;1;2 = 14",
        "VideoMemoryScaleMP@0;1;2 = 0.900000",
        "GPUName@0;1;2 = Synthetic Recipient GPU",
        "LastUsedGPU@0;1;2 = Synthetic Recipient GPU",
        "Monitor@0;1;2 = Synthetic Recipient Monitor",
        "DetectedMemoryAmountMB@0;1;2 = 16384",
        "DetectedFrequencyGHz@0;1;2 = 18.000000",
        "DisplayDriverVersion@0;1;2 = unchanged",
        "GPUDriverVersion@0;1;2 = unchanged",
        "TextureQuality@0;1;2 = 0",
        "TextureFilter@0;1;2 = aniso 16x",
        "ResolutionMultiplier@0;1;2 = 100",
        "AATechniquePreferredMP@0;1;2 = DLSS",
        "DLSSPerfModeMP@0;1;2 = Maximum Quality",
        "AMDFidelityFX@0;1;2 = CAS",
        "AMDFidelityFX@1;1;2 = Off",
        "GSync@0;1;2 = false",
        "MaxFpsInGame@0;1;2 = 237",
        "UnknownGraphics@0;1;2 = Keep Me"
    )
    $config = $lines -join "`r`n"
    $paths = @("s.1.0.cod24.txt0", "s.1.0.cod24.txt1") | ForEach-Object { Join-Path $players $_ }
    foreach ($path in $paths) { [IO.File]::WriteAllText($path, $config, $utf8) }
    foreach ($name in @("s.1.0.cod25.txt0", "g.p.1.0.l.txt0")) {
        [IO.File]::WriteAllText((Join-Path $players $name), $config, $utf8)
    }
    $plan = Get-AudioPlan $players "Graphics" $true
    Assert-True ($plan.Mode -eq "Graphics" -and $plan.Inspected -eq 2 -and $plan.Files.Count -eq 2) "Graphics must select only supported system files"
    Assert-True ($plan.Coverage.Count -eq 2 -and $plan.Coverage[0].Recognized -eq $targets.Count) "Coverage must have one complete entry per inspected file"
    Assert-True ((Read-ConfigText $paths[0]) -ceq $config) "Preview wrote a live fixture"
    foreach ($file in $plan.Files) {
        Assert-True ($file.Changes.Count -eq $targets.Count) "Missing requested graphics targets"
        $expected = $config
        foreach ($change in $file.Changes) {
            Assert-True ($change.After -ceq $targets[$change.Key]) "Graphics target differs from preset"
            $expected = Set-KeyValue $expected $change.Key $change.After $change.Before
        }
        Assert-True ($expected -ceq $file.After) "Preview changed an unrelated token"
    }
    $nonNvidia = Get-AudioPlan $players "Graphics" $false
    Assert-True (@($nonNvidia.Files[0].Changes | Where-Object { $_.Key -eq "NvidiaReflex" }).Count -eq 0) "Non-NVIDIA PC must not change Reflex"
    Assert-True ((Get-KeyMatch $nonNvidia.Files[0].After "NvidiaReflex").Groups["value"].Value -ceq "Disabled") "Non-NVIDIA Reflex value changed"
    $backups = Join-Path $root "backups"
    (Get-Item -LiteralPath $paths[1]).IsReadOnly = $true
    try {
        Assert-Refused { Apply-AudioPlan $plan $backups } "Read-only preflight must refuse"
        Assert-True ((Read-ConfigText $paths[0]) -ceq $config) "Read-only preflight partially applied another file"
    } finally { (Get-Item -LiteralPath $paths[1]).IsReadOnly = $false }
    $backup = Apply-AudioPlan $plan $backups
    Assert-True ((Get-AudioPlan $players "Graphics" $true).Files.Count -eq 0) "Graphics reapply must be a no-op"
    $manifestPath = Join-Path $backup "manifest.json"
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    Assert-True ($manifest.Mode -eq "Graphics" -and $manifest.Status -eq "Applied") "Graphics backup mode/status missing"
    Assert-True ((Read-ConfigText (Join-Path $backup "0.original")) -ceq $config) "Full graphics backup is not exact"
    foreach ($path in $paths) {
        $updated = (Read-ConfigText $path).Replace("MusicVolume@0;1;2 = 0.750000", "MusicVolume@0;1;2 = 0.250000")
        [IO.File]::WriteAllText($path, $updated, $utf8)
    }
    Restore-AudioBackup $backup
    foreach ($path in $paths) {
        Assert-True ((Read-ConfigText $path) -ceq $config.Replace("MusicVolume@0;1;2 = 0.750000", "MusicVolume@0;1;2 = 0.250000")) "Graphics restore must preserve later audio edits"
    }
    foreach ($name in @("s.1.0.cod25.txt0", "g.p.1.0.l.txt0")) {
        Assert-True ((Read-ConfigText (Join-Path $players $name)) -ceq $config) "Unsupported or account file was edited"
    }
    $plan = Get-AudioPlan $players "Graphics" $true
    [IO.File]::AppendAllText($paths[0], "`r`n// external change", $utf8)
    Assert-Refused { Apply-AudioPlan $plan $backups } "Stale graphics preview must refuse"
    foreach ($path in $paths) { [IO.File]::WriteAllText($path, $config, $utf8) }
    $plan = Get-AudioPlan $players "Graphics" $true
    $backup = Apply-AudioPlan $plan $backups
    [IO.File]::WriteAllText($paths[1], (Read-ConfigText $paths[1]).Replace("MaxFpsInMenu@0;1;2 = 60", "MaxFpsInMenu@0;1;2 = 90"), $utf8)
    Assert-Refused { Restore-AudioBackup $backup } "Changed graphics value must conflict"
    Assert-True ((Read-ConfigText $paths[0]) -ceq $plan.Files[0].After) "Restore conflict must preflight all files"
    function Assert-GameClosed { throw "Synthetic running game" }
    Assert-Refused { Apply-AudioPlan $plan $backups } "Graphics apply must require game closed"
    Assert-Refused { Restore-AudioBackup $backup } "Graphics restore must require game closed"
    function Assert-GameClosed {}
    foreach ($bad in @(
        $config.Replace("DepthOfField@0;1;2 = true", "DepthOfField@0;1;2 = invalid"),
        $config.Replace("ShadowQuality@0;1;2 = High", "ShadowQuality@0;1;2 = UnverifiedEnum"),
        $config.Replace("MaxFpsInMenu@0;1;2 = 120", "MaxFpsInMenu@0;1;2 = 9999"),
        ($config + "`r`nVSync@1;1;2 = disabled")
    )) {
        [IO.File]::WriteAllText($paths[0], $bad, $utf8)
        Assert-Refused { Get-AudioPlan $players "Graphics" $true } "Malformed/duplicate setting must refuse"
        Assert-True ((Read-ConfigText $paths[0]) -ceq $bad) "Rejected preview changed config"
    }
    $lfConfig = $config.Replace("`r`n", "`n")
    foreach ($path in $paths) { [IO.File]::WriteAllText($path, $lfConfig, (New-Object Text.UTF8Encoding($true))) }
    $bytesBefore = [Convert]::ToBase64String([IO.File]::ReadAllBytes($paths[0]))
    $backup = Apply-AudioPlan (Get-AudioPlan $players "Graphics" $true) $backups
    Restore-AudioBackup $backup
    Assert-True ([Convert]::ToBase64String([IO.File]::ReadAllBytes($paths[0])) -ceq $bytesBefore) "Graphics round-trip must preserve BOM, first key, comments and LF bytes"
    $backup = Apply-AudioPlan (Get-AudioPlan $players "Graphics" $true) $backups
    $manifestPath = Join-Path $backup "manifest.json"
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $manifest.Files[0].Changes[0].Before = "bad`nInjected@0;1;2 = true"
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    $beforeRestore = Read-ConfigText $paths[0]
    Assert-Refused { Restore-AudioBackup $backup } "Invalid backup values must refuse"
    Assert-True ((Read-ConfigText $paths[0]) -ceq $beforeRestore) "Invalid backup modified config"
    Assert-Refused { Get-AudioPlan $players "Unknown" } "Unknown mode must refuse"
    $selectedFolder = Join-Path $root "selected"
    New-Item -ItemType Directory -Path $selectedFolder | Out-Null
    foreach ($name in @("s.1.0.cod26.txt0", "options.new-title.cst", "custom.cfg", "custom.ini", "custom.txt", "custom.txt2")) {
        $path = Join-Path $selectedFolder $name
        $sibling = Join-Path $selectedFolder "s.1.0.cod26.txt1"
        $text = $config + "`r`nFutureFrameGeneration@0;1;2 = true"
        [IO.File]::WriteAllText($path, $text, $utf8)
        [IO.File]::WriteAllText($sibling, $text, $utf8)
        $selectedPlan = Get-AudioPlan $players "Graphics" $true @($path)
        Assert-True ($selectedPlan.Inspected -eq 1 -and $selectedPlan.Files.Count -eq 1 -and $selectedPlan.Files[0].Path -ceq $path) "Explicit selection must edit only the chosen file, regardless of filename"
        Assert-True ($selectedPlan.Coverage.Count -eq 1) "Selected-file coverage was duplicated"
        Assert-True ($selectedPlan.Coverage[0].UnverifiedFrameGeneration -contains "FutureFrameGeneration") "Unknown frame generation must be reported"
        $backup = Apply-AudioPlan $selectedPlan $backups
        $manifest = Get-Content -LiteralPath (Join-Path $backup "manifest.json") -Raw | ConvertFrom-Json
        Assert-True ($manifest.Version -eq 2 -and $manifest.SelectedFiles -contains $path) "Selected-file backup must record exact paths"
        $actual = Read-ConfigText $path
        Assert-True ((Get-KeyMatch $actual "AATechniquePreferredMP").Groups["value"].Value -ceq "DLSS") "Do not replace allowed DLSS selection"
        Assert-True ($actual.Contains("AMDFidelityFX@0;1;2 = CAS") -and $actual.Contains("AMDFidelityFX@1;1;2 = Off")) "CAS choice must remain untouched, including repeated scopes"
        Assert-True ($actual.Contains("GSync@0;1;2 = false")) "Never enable G-SYNC"
        Assert-True ($actual.Contains("FutureFrameGeneration@0;1;2 = true")) "Unknown frame-generation values must not be guessed"
        Assert-True ((Read-ConfigText $sibling) -ceq $text) "Unselected generation was modified"
        Restore-AudioBackup $backup
        Assert-True ((Read-ConfigText $path) -ceq $text) "New filename restore failed"
    }
    $firstSelected = Join-Path $selectedFolder "custom.txt"
    $secondSelected = Join-Path $selectedFolder "custom.cfg"
    $selectedPlan = Get-AudioPlan $players "Graphics" $true @($firstSelected, $secondSelected)
    Assert-True ($selectedPlan.Inspected -eq 2) "Multi-selection lost a file"
    $backup = Apply-AudioPlan $selectedPlan $backups
    [IO.File]::WriteAllText($secondSelected, (Read-ConfigText $secondSelected).Replace("MaxFpsInMenu@0;1;2 = 60", "MaxFpsInMenu@0;1;2 = 90"), $utf8)
    Assert-Refused { Restore-AudioBackup $backup } "Selected-file restore must reject conflicts before writing"
    Assert-True ((Read-ConfigText $firstSelected) -ceq $selectedPlan.Files[0].After) "Selected-file restore partially wrote before a conflict"
    Assert-Refused { Get-AudioPlan $players "Graphics" $true @($firstSelected, $paths[0]) } "Multi-selection must refuse mixed directories"
    $badPath = Join-Path $selectedFolder "unknown-format.cst"
    [IO.File]::WriteAllText($badPath, 'VSync:0.0 = "enabled"', $utf8)
    Assert-Refused { Get-AudioPlan $players "Graphics" $true @($badPath) } "Unknown content format must be refused even with a recognized extension"
    [IO.File]::WriteAllText($badPath, "DLSSFrameGeneration@0;1;2 = invalid", $utf8)
    Assert-Refused { Get-AudioPlan $players "Graphics" $true @($badPath) } "Malformed known settings in new files must be refused"
    [IO.File]::WriteAllText($badPath, "VSync@0;1;2 = disabled", $utf8)
    $partialPlan = Get-AudioPlan $players "Graphics" $true @($badPath)
    Assert-True ($partialPlan.Files.Count -eq 0 -and $partialPlan.Coverage[0].Missing -contains "DLSSFrameGeneration") "Already-matching partial configs must still report missing generation settings"
    $binaryPath = Join-Path $selectedFolder "binary.txt0"
    [IO.File]::WriteAllBytes($binaryPath, [byte[]]@(0, 1, 2))
    Assert-Refused { Get-AudioPlan $players "Graphics" $true @($binaryPath) } "Selected binary content must be refused"
    $marker = Join-Path $selectedFolder "config.m"
    [IO.File]::WriteAllText($marker, $config, $utf8)
    Assert-Refused { Get-AudioPlan $players "Graphics" $true @($marker) } "Marker file must be refused even if it contains recognized keys"
    $duplicatePlan = Get-AudioPlan $players "Graphics" $true @($firstSelected, $firstSelected.ToUpperInvariant())
    Assert-True ($duplicatePlan.Inspected -eq 1) "Repeated Windows paths must not be backed up or edited twice"
    $audioPlan = Get-AudioPlan $players "Audio" $false @($firstSelected)
    Assert-True ($audioPlan.Files.Count -eq 1 -and $audioPlan.Files[0].Changes[0].Key -eq "MusicVolume") "Audio must also support explicit file selection"
    $audioBackup = Apply-AudioPlan $audioPlan $backups
    Restore-AudioBackup $audioBackup
    $manifestPath = Join-Path $audioBackup "manifest.json"
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $manifest.Status = "Applied"
    $manifest.SelectedFiles = @($secondSelected)
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Assert-Refused { Restore-AudioBackup $audioBackup } "Restore must require recorded selected-file membership"
    $scopedPath = Join-Path $selectedFolder "scoped-cod26.txt"
    $scopedConfig = $config + (@(
        "",
        "DxrMode@1;8707;7764 = Ultra // one of Off, Low, Medium, High, Ultra",
        "VolumetricQuality@1;4276;58629 = QUALITY_HIGH // one of QUALITY_LOW, QUALITY_MEDIUM, QUALITY_HIGH, QUALITY_VERYHIGH",
        "SSRQuality@1;63075;43096 = High // one of Off, Low, Medium, High",
        "DLSSFrameGenerationmulti@0;61843;11445 = 0",
        "DLSSFrameGenerationmulti@1;21286;11445 = 1",
        "IntelXeLL@0;57752;48403 = false",
        "IntelXeLL@1;57752;20945 = true"
    ) -join "`r`n")
    [IO.File]::WriteAllText($scopedPath, $scopedConfig, $utf8)
    $scopedPlan = Get-AudioPlan $players "Graphics" $true @($scopedPath)
    foreach ($key in @("DxrMode", "VolumetricQuality", "SSRQuality")) {
        Assert-True (@($scopedPlan.Files[0].Changes | Where-Object { $_.Key -eq $key }).Count -eq 2) "Both verified scopes of $key must be planned separately"
    }
    $scopedBackup = Apply-AudioPlan $scopedPlan $backups
    foreach ($scope in @(0, 1)) {
        Assert-True ((Get-KeyMatch (Read-ConfigText $scopedPath) "DxrMode" $scope).Groups["value"].Value -ceq "Off") "Scoped raytracing was not disabled"
        Assert-True ((Get-KeyMatch (Read-ConfigText $scopedPath) "SSRQuality" $scope).Groups["value"].Value -ceq "Off") "Scoped reflections were not disabled"
    }
    $scopedActual = Read-ConfigText $scopedPath
    Assert-True ($scopedActual.Contains("DLSSFrameGenerationmulti@0;61843;11445 = 0") -and $scopedActual.Contains("DLSSFrameGenerationmulti@1;21286;11445 = 1")) "Do not guess multiplier enums"
    Restore-AudioBackup $scopedBackup
    Assert-True ((Read-ConfigText $scopedPath) -ceq $scopedConfig) "Scoped restore failed"
    [IO.File]::AppendAllText($scopedPath, "`r`nDxrMode@1;1;2 = Off", $utf8)
    Assert-Refused { Get-AudioPlan $players "Graphics" $true @($scopedPath) } "Duplicate entries in the same scope must still refuse"
    [IO.File]::WriteAllText($scopedPath, $config + "`r`nSSRQuality@2;1;2 = High", $utf8)
    Assert-Refused { Get-AudioPlan $players "Graphics" $true @($scopedPath) } "Unverified scopes must refuse"
    Write-Output "PASS: graphics targets, no G-SYNC enable, preserved DLSS/CAS, frame-generation warnings, exact single/multi-file selection across filenames, audio selection, backups/restore, format and conflict guards."
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force
}
