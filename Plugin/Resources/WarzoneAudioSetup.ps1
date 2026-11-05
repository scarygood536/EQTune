param(
    [ValidateSet("UI", "Preview")]
    [string]$Action = "UI",
    [ValidateSet("Audio", "Graphics")]
    [string]$Mode = "Audio",
    [string]$PlayersDirectory = (Join-Path ([Environment]::GetFolderPath("MyDocuments")) "Call of Duty\players"),
    [string[]]$ConfigFiles = @()
)

$ErrorActionPreference = "Stop"

function Assert-GameClosed {
    $running = @(Get-Process | Where-Object { $_.ProcessName -match "^(cod[0-9]*|bootstrapper)$" })
    if ($running.Count -gt 0) { throw "Close Call of Duty and its bootstrapper before applying or restoring game settings." }
}

function Read-ConfigText([string]$Path) {
    $item = Get-Item -LiteralPath $Path
    if ($item.Length -gt 2MB) { throw "Unexpectedly large config file: $Path" }
    $bytes = [IO.File]::ReadAllBytes($item.FullName)
    if ($bytes -contains 0) { throw "Binary or UTF-16 configs are not supported: $Path" }
    $utf8 = New-Object Text.UTF8Encoding($false, $true)
    return $utf8.GetString($bytes)
}

function Get-AudioTargets {
    return [ordered]@{
        MusicVolume = "0.000000"
        CinematicVolume = "0.000000"
        WarTracksVolume = "0.000000"
        VoiceVolume = "0.500000"
        MonoSound = "false"
        AltShellShock = "true"
    }
}

function Get-GraphicsTargets([bool]$NvidiaAvailable) {
    $targets = [ordered]@{
        VSync = "disabled"
        VSyncInMenu = "disabled"
        DepthOfField = "false"
        DofEnable = "Off"
        DofWeaponDisable = "true"
        DofWorldDisable = "true"
        MotionBlur = "Off"
        WeaponMotionBlur = "false"
        EnableVelocityBasedBlur = "false"
        DLSSFrameGeneration = "false"
        FSRFrameInterpolation = "false"
        IntelXeFG = "false"
        DynamicSceneResolution = "false"
        DxrMode = "Off"
        ShadowQuality = "Very_Low"
        ScreenSpaceShadowQuality = "Off"
        SSRQuality = "Off"
        VolumetricQuality = "QUALITY_LOW"
        WeatherGridVolumesQuality = "Off"
        WaterCausticsMode = "Off"
        WaterWaveWetness = "false"
        ParticleQuality = "very low"
        ShaderQuality = "Low"
        ModelQuality = "Low Quality"
        WorldStreamingQuality = "Low"
        ShadingQuality = "Low"
        ShadowFilteringQuality = "Low"
        TerrainQuality = "Very Low"
        Tessellation = "0_Off"
        DeferredPhysics = "Low Quality"
        ReflectionProbeHalfResolution = "true"
        SubdivisionLevel = "0"
        MaxFpsInMenu = "60"
        MaxFpsOutOfFocus = "30"
    }
    if ($NvidiaAvailable) { $targets["NvidiaReflex"] = "Enabled" }
    return $targets
}

function Get-SettingsTargets([string]$SettingsMode, [bool]$NvidiaAvailable = $false) {
    if ($SettingsMode -eq "Audio") { return Get-AudioTargets }
    if ($SettingsMode -eq "Graphics") { return Get-GraphicsTargets $NvidiaAvailable }
    throw "Unsupported settings mode."
}

function Assert-SettingValue([string]$Key, [string]$Value, [string]$SettingsMode) {
    if ($SettingsMode -eq "Audio") {
        if ($Key -in @("MonoSound", "AltShellShock")) {
            if ($Value -cnotin @("true", "false")) { throw "Unsupported boolean for $Key." }
        } elseif ($Value -notmatch "^[0-9]+(?:\.[0-9]+)?$" -or
            [double]::Parse($Value, [Globalization.CultureInfo]::InvariantCulture) -gt 1) {
            throw "Unsupported volume for $Key."
        }
    } elseif ($Key -in @("DepthOfField", "EnableVelocityBasedBlur", "DLSSFrameGeneration",
        "FSRFrameInterpolation", "DynamicSceneResolution", "WaterWaveWetness", "IntelXeFG",
        "DofWeaponDisable", "DofWorldDisable", "WeaponMotionBlur", "ReflectionProbeHalfResolution")) {
        if ($Value -cnotin @("true", "false")) { throw "Unsupported boolean for $Key." }
    } elseif ($Key -eq "SubdivisionLevel") {
        if ($Value -notmatch "^[0-8]$") { throw "Unsupported subdivision level." }
    } elseif ($Key -in @("MaxFpsInMenu", "MaxFpsOutOfFocus")) {
        $minimum = if ($Key -eq "MaxFpsInMenu") { 30 } else { 5 }
        if ($Value -notmatch "^[0-9]{1,3}$" -or [int]$Value -lt $minimum -or [int]$Value -gt 300) {
            throw "Unsupported frame limit for $Key."
        }
    } else {
        $allowed = switch ($Key) {
            { $_ -in @("VSync", "VSyncInMenu") } { @("disabled", "100%", "50%", "33%", "25%") }
            "NvidiaReflex" { @("Disabled", "Enabled", "Enabled + boost") }
            "DofEnable" { @("Off", "On", "Script") }
            "MotionBlur" { @("Off", "On", "Cinematic Only") }
            "DxrMode" { @("Off", "On", "Low", "Medium", "High", "Ultra") }
            "ShadowQuality" { @("Very_Low", "Low", "Medium", "High", "Very_High") }
            "ScreenSpaceShadowQuality" { @("Off", "Low", "High") }
            "SSRQuality" { @("Off", "Low", "Medium", "High") }
            "VolumetricQuality" { @("QUALITY_LOW", "QUALITY_MEDIUM", "QUALITY_HIGH", "QUALITY_VERYHIGH") }
            "WeatherGridVolumesQuality" { @("Off", "Low", "Medium", "High", "Ultra") }
            "WaterCausticsMode" { @("Off", "Low Quality", "High Quality") }
            "ParticleQuality" { @("very low", "low", "medium", "high") }
            "ShaderQuality" { @("Default", "Medium", "Low") }
            { $_ -in @("ModelQuality", "DeferredPhysics") } { @("Low Quality", "Medium Quality", "High Quality") }
            "WorldStreamingQuality" { @("Low", "High") }
            "ShadingQuality" { @("Low", "High") }
            "ShadowFilteringQuality" { @("Low", "Medium", "High") }
            "TerrainQuality" { @("Very Low", "Low", "Medium", "High", "Very High") }
            "Tessellation" { @("0_Off", "1_Near", "2_All") }
            default { throw "Unsupported graphics key '$Key'." }
        }
        if ($Value -cnotin @($allowed)) { throw "Unsupported graphics value for $Key. This config needs a verified mapping." }
    }
}

function Get-SettingScopes([string]$Key) {
    if ($Key -in @("DxrMode", "VolumetricQuality", "SSRQuality")) { return @(0, 1) }
    return @(0)
}

function Get-KeyMatch([string]$Text, [string]$Key, [int]$Scope = 0) {
    if ($Scope -notin @(Get-SettingScopes $Key)) { throw "Unsupported scope for '$Key'." }
    $pattern = "(?m)^(?<prefix>\uFEFF?" + [regex]::Escape($Key) + "@" + $Scope + ";[0-9;]+[ `t]*=[ `t]*)(?<value>[^/\r\n]+?)(?=[ `t]*(?://[^\r\n]*)?\r?$)"
    $matches = [regex]::Matches($Text, $pattern)
    if ($matches.Count -ne 1 -or [string]::IsNullOrWhiteSpace($matches[0].Groups["value"].Value)) {
        throw "Unsupported or duplicate game setting '$Key'. No changes made to this file."
    }
    return $matches[0]
}

function Set-KeyValue([string]$Text, [string]$Key, [string]$Value, [string]$Expected, [int]$Scope = 0) {
    $match = Get-KeyMatch $Text $Key $Scope
    if ($match.Groups["value"].Value -cne $Expected) {
        throw "Game setting '$Key' changed since the preview/backup. Nothing was overwritten; review it in-game."
    }
    $token = $match.Groups["value"]
    return $Text.Substring(0, $token.Index) + $Value + $Text.Substring($token.Index + $token.Length)
}

function Assert-ConfigFile([string]$Path) {
    $item = Get-Item -LiteralPath $Path
    if ($item.PSIsContainer -or $item.Name -notmatch "\.(txt[0-9]*|cfg|ini|cst)$" -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Select a regular text game config (.txt, .txt0/.txt1, .cfg, .ini or .cst), not a binary, marker or link: $Path"
    }
    return $item
}

function Get-AudioPlan([string]$Directory, [string]$SettingsMode = "Audio", [bool]$NvidiaAvailable = $false,
    [string[]]$SelectedFiles = @()) {
    $targets = Get-SettingsTargets $SettingsMode $NvidiaAvailable
    $explicitSelection = @($SelectedFiles).Count -gt 0
    if ($explicitSelection) {
        $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        $files = @($SelectedFiles | ForEach-Object {
            $item = Assert-ConfigFile $_
            if ($seen.Add($item.FullName)) { $item }
        })
        $root = $files[0].DirectoryName
        foreach ($file in $files) {
            if ($file.DirectoryName -ine $root) { throw "Choose config files from one folder at a time. No files were changed." }
        }
    } else {
        $root = (Get-Item -LiteralPath $Directory).FullName
        $files = @(Get-ChildItem -LiteralPath $root -File | Where-Object { $_.Name -match "^(s\.1\.0\.cod24|g\.p\.1\.0\.l)\.txt[01]$" })
        $profiles = @(Get-ChildItem -LiteralPath $root -Directory | Where-Object {
            Test-Path -LiteralPath (Join-Path $_.FullName "g.p.1.0.l.txt0")
        })
        if ($SettingsMode -eq "Audio" -and $profiles.Count -gt 1) {
            throw "Multiple game profiles found. Choose the specific account folder inside 'players' instead. No files were changed."
        }
        if ($SettingsMode -eq "Audio" -and $profiles.Count -eq 1) {
            $files += @(Get-ChildItem -LiteralPath $profiles[0].FullName -File | Where-Object { $_.Name -match "^g\.p\.1\.0\.l\.txt[01]$" })
        }
        if ($SettingsMode -eq "Graphics") { $files = @($files | Where-Object { $_.Name -match "^s\.1\.0\.cod24\.txt[01]$" }) }
    }
    if ($files.Count -eq 0) { throw "No supported cod24/player $SettingsMode configs found. Open the game once or select its players folder. Newer formats require manual setup." }
    $plans = @()
    $coverage = @()
    foreach ($file in $files) {
        $text = Read-ConfigText $file.FullName
        $updated = $text
        $changes = @()
        $missing = @()
        $recognized = 0
        foreach ($key in $targets.Keys) {
            $entries = [regex]::Matches($text, "(?m)^\uFEFF?" + [regex]::Escape($key) + "@(?<scope>[0-9]+);")
            $present = [regex]::Matches($text, "(?m)^\uFEFF?" + [regex]::Escape($key) + "@").Count
            if ($present -eq 0) { $missing += $key; continue }
            if ($entries.Count -ne $present) { throw "Malformed setting '$key' in $($file.Name)." }
            $recognized++
            foreach ($entry in $entries) {
                $scope = [int]$entry.Groups["scope"].Value
                $old = (Get-KeyMatch $text $key $scope).Groups["value"].Value
                $new = $targets[$key]
                Assert-SettingValue $key $old $SettingsMode
                if ($SettingsMode -eq "Audio" -and $new -match "^[0-9]") {
                    if ([double]::Parse($old, [Globalization.CultureInfo]::InvariantCulture) -eq
                        [double]::Parse($new, [Globalization.CultureInfo]::InvariantCulture)) { continue }
                }
                if ($old -ceq $new) { continue }
                $updated = Set-KeyValue $updated $key $new $old $scope
                $changes += [pscustomobject]@{ Key = $key; Scope = $scope; Before = $old; After = $new }
            }
        }
        if ($explicitSelection -and $recognized -eq 0) {
            throw "No recognized $SettingsMode settings in $($file.Name). This format is unsupported; no files were changed. Supported entries use Key@numbers = value."
        }
        $unverifiedGeneration = @([regex]::Matches($text, "(?m)^\uFEFF?(?<key>[A-Za-z][A-Za-z0-9_]*)(?=@|:|[ `t]*=)") |
            ForEach-Object { $_.Groups["key"].Value } |
            Where-Object { $_ -match "frame.*gen|interpol|frame.*synth" -and $_ -notin @($targets.Keys) } |
            Select-Object -Unique)
        $coverage += [pscustomobject]@{
            Path = $file.FullName; Recognized = $recognized; Missing = $missing
            UnverifiedFrameGeneration = $unverifiedGeneration
        }
        if ($changes.Count -gt 0) {
            $plans += [pscustomobject]@{ Path = $file.FullName; Before = $text; After = $updated; Changes = $changes }
        }
    }
    return [pscustomobject]@{
        Root = $root; Files = $plans; Inspected = $files.Count; Mode = $SettingsMode
        SelectedFiles = $(if ($explicitSelection) { @($files | ForEach-Object { $_.FullName }) } else { @() })
        Coverage = $coverage
    }
}

function Write-ConfigAtomic([string]$Path, [string]$Text, [string]$Expected) {
    if ((Read-ConfigText $Path) -cne $Expected) { throw "Config changed while preparing the update: $Path" }
    $temporary = $Path + ".eqtune-" + [guid]::NewGuid().ToString("N") + ".tmp"
    try {
        [IO.File]::WriteAllText($temporary, $Text, (New-Object Text.UTF8Encoding($false)))
        [IO.File]::Replace($temporary, $Path, [NullString]::Value)
        if ((Read-ConfigText $Path) -cne $Text) { throw "Config verification failed: $Path" }
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    }
}

function Apply-AudioPlan($Plan, [string]$BackupRoot) {
    Assert-GameClosed
    if (@($Plan.Files).Count -eq 0) { throw "The supported settings are already configured. No backup or changes needed." }
    foreach ($file in $Plan.Files) {
        if ((Read-ConfigText $file.Path) -cne $file.Before) { throw "Config changed since preview: $($file.Path)" }
        if ((Get-Item -LiteralPath $file.Path).IsReadOnly) { throw "Config is read-only: $($file.Path)" }
    }
    $backup = Join-Path $BackupRoot ((Get-Date -Format "yyyyMMdd-HHmmss") + "-" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    $entries = @()
    for ($i = 0; $i -lt $Plan.Files.Count; $i++) {
        $file = $Plan.Files[$i]
        Copy-Item -LiteralPath $file.Path -Destination (Join-Path $backup "$i.original") -ErrorAction Stop
        $entries += [pscustomobject]@{ Path = $file.Path; Changes = @($file.Changes) }
    }
    $manifestPath = Join-Path $backup "manifest.json"
    $manifest = [pscustomobject]@{ Version = 1; Root = $Plan.Root; Mode = $Plan.Mode; Status = "Prepared"; Files = $entries }
    if (@($Plan.SelectedFiles).Count -gt 0) {
        $manifest.Version = 2
        $manifest | Add-Member -NotePropertyName SelectedFiles -NotePropertyValue @($Plan.SelectedFiles)
    }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    try {
        foreach ($file in $Plan.Files) { Write-ConfigAtomic $file.Path $file.After $file.Before }
        $manifest.Status = "Applied"
        $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    } catch {
        throw "Apply failed or was partial. Backup: $backup. Use Restore previous settings to undo completed changes. Details: $($_.Exception.Message)"
    }
    return $backup
}

function Restore-AudioBackup([string]$Backup) {
    Assert-GameClosed
    $manifestPath = Join-Path $Backup "manifest.json"
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.Version -notin @(1, 2) -or $manifest.Status -notin @("Applied", "Prepared")) { throw "This backup is unsupported or already restored." }
    $settingsMode = if ($null -eq $manifest.Mode) { "Audio" } else { [string]$manifest.Mode }
    $targets = Get-SettingsTargets $settingsMode $true
    $root = [IO.Path]::GetFullPath($manifest.Root).TrimEnd('\') + "\"
    $plans = @()
    foreach ($file in $manifest.Files) {
        $path = [IO.Path]::GetFullPath($file.Path)
        if (-not $path.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw "Backup contains an unsupported config path." }
        if ($manifest.Version -eq 2) {
            if ($path -notin @($manifest.SelectedFiles) -or
                ([IO.Path]::GetDirectoryName($path).TrimEnd('\') + "\") -ine $root) {
                throw "Backup path does not match the explicitly selected config files."
            }
            Assert-ConfigFile $path | Out-Null
        } else {
            if ([IO.Path]::GetFileName($path) -notmatch "^(s\.1\.0\.cod24|g\.p\.1\.0\.l)\.txt[01]$") {
                throw "Backup contains an unsupported config path."
            }
            if ($settingsMode -eq "Graphics" -and [IO.Path]::GetFileName($path) -notmatch "^s\.1\.0\.cod24\.txt[01]$") {
                throw "Graphics backup contains an unsupported config path."
            }
        }
        $before = Read-ConfigText $path
        $after = $before
        foreach ($change in $file.Changes) {
            if ($change.Key -notin @($targets.Keys) -or $change.After -cne $targets[$change.Key]) {
                throw "Unsupported backup setting."
            }
            Assert-SettingValue $change.Key $change.Before $settingsMode
            $scope = if ($null -eq $change.Scope) { 0 } else { [int]$change.Scope }
            $current = (Get-KeyMatch $after $change.Key $scope).Groups["value"].Value
            if ($current -ceq $change.Before) { continue }
            $after = Set-KeyValue $after $change.Key $change.Before $change.After $scope
        }
        if ($before -cne $after) {
            if ((Get-Item -LiteralPath $path).IsReadOnly) { throw "Config is read-only: $path" }
            $plans += [pscustomobject]@{ Path = $path; Before = $before; After = $after }
        }
    }
    foreach ($file in $plans) { Write-ConfigAtomic $file.Path $file.After $file.Before }
    $manifest.Status = "Restored"
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
}

function Show-SettingsPreview([string]$Preview, [bool]$CanApply = $true) {
    $form = New-Object Windows.Forms.Form
    try {
        $form.Text = "EqTune - preview $Mode changes"
        $form.Size = New-Object Drawing.Size(800, 640)
        $form.MinimumSize = New-Object Drawing.Size(600, 400)
        $form.StartPosition = "CenterScreen"
        $text = New-Object Windows.Forms.TextBox
        $text.Multiline = $true
        $text.ReadOnly = $true
        $text.ScrollBars = "Both"
        $text.WordWrap = $false
        $text.Dock = "Fill"
        $text.Text = $Preview.Replace("`n", "`r`n")
        $text.Font = New-Object Drawing.Font("Consolas", 10)
        $buttons = New-Object Windows.Forms.FlowLayoutPanel
        $buttons.Dock = "Bottom"
        $buttons.Height = 48
        $buttons.FlowDirection = "RightToLeft"
        $cancel = New-Object Windows.Forms.Button
        $cancel.Text = "Cancel"
        $cancel.DialogResult = "Cancel"
        $apply = New-Object Windows.Forms.Button
        $apply.Text = "Back up + apply"
        $apply.Width = 150
        $apply.DialogResult = "OK"
        $apply.Enabled = $CanApply
        $buttons.Controls.Add($cancel)
        $buttons.Controls.Add($apply)
        $form.Controls.Add($text)
        $form.Controls.Add($buttons)
        $form.CancelButton = $cancel
        $form.ActiveControl = $cancel
        return $form.ShowDialog() -eq "OK"
    } finally { $form.Dispose() }
}

function Get-GraphicsHardware {
    $names = @(Get-CimInstance Win32_VideoController -ErrorAction Stop | ForEach-Object { $_.Name })
    if ($names.Count -eq 0) { throw "Could not detect the graphics hardware. No settings were changed." }
    return [pscustomobject]@{
        Names = $names
        NvidiaAvailable = @($names | Where-Object { $_ -match "NVIDIA" }).Count -gt 0
    }
}

function Show-AudioSetup {
    Add-Type -AssemblyName System.Windows.Forms
    $backupFolder = if ($Mode -eq "Graphics") { "GameGraphicsBackups" } else { "GameAudioBackups" }
    $backups = Join-Path $env:LOCALAPPDATA "EqTune\$backupFolder"
    $details = if ($Mode -eq "Graphics") {
        "Competitive clarity / low latency starting preset, not a benchmarked best configuration. Known frame-generation options and V-Sync are disabled. DLSS super resolution / FidelityFX CAS may be used: upscaling/sharpening selection stays unchanged. EqTune NEVER enables G-SYNC/VRR and does not change driver settings; turn G-SYNC off yourself in NVIDIA Control Panel. Resolution, refresh rate, match FPS cap, controls, sensitivity, FOV and audio stay unchanged. Tearing/pop-in is possible."
    } else {
        "This is not a guaranteed best mix. Master/effects volume, device routing, audio mix and spatial settings stay unchanged."
    }
    $choice = [Windows.Forms.MessageBox]::Show(
        "Close Call of Duty first. No game files have been changed yet.`n`nYES: Preview and apply the $Mode baseline.`nNO: Restore a previous EqTune settings backup.`nCANCEL: Exit.`n`n$details",
        "EqTune - Warzone $Mode setup", "YesNoCancel", "Information")
    if ($choice -eq "Cancel") { return }
    if ($choice -eq "No") {
        $dialog = New-Object Windows.Forms.OpenFileDialog
        try {
            $dialog.Title = "Choose an EqTune settings backup manifest"
            $dialog.Filter = "EqTune backup (manifest.json)|manifest.json"
            if (Test-Path -LiteralPath $backups) { $dialog.InitialDirectory = $backups }
            if ($dialog.ShowDialog() -ne "OK") { return }
            $backup = Split-Path $dialog.FileName
            if ([Windows.Forms.MessageBox]::Show("Restore only the values changed by this backup? Later changes to those values cause a conflict rather than being overwritten.", "Confirm restore", "YesNo", "Warning") -ne "Yes") { return }
            Restore-AudioBackup $backup
            [Windows.Forms.MessageBox]::Show("Previous values restored. Unrelated settings were kept.", "EqTune") | Out-Null
        } finally { $dialog.Dispose() }
        return
    }
    $picker = New-Object Windows.Forms.OpenFileDialog
    $hardware = if ($Mode -eq "Graphics") { Get-GraphicsHardware } else { $null }
    try {
        $picker.Title = "Select the active COD $Mode config file(s) - only selected files are edited"
        $picker.Filter = "Text game configs|*.txt*;*.cfg;*.ini;*.cst|All files|*.*"
        $picker.Multiselect = $true
        $picker.CheckFileExists = $true
        if (Test-Path -LiteralPath $PlayersDirectory -PathType Container) { $picker.InitialDirectory = $PlayersDirectory }
        $selected = $ConfigFiles
        if (@($selected).Count -eq 0) {
            if ($picker.ShowDialog() -ne "OK") { return }
            $selected = $picker.FileNames
        }
        $plan = Get-AudioPlan $PlayersDirectory $Mode ($null -ne $hardware -and $hardware.NvidiaAvailable) $selected
    } finally { $picker.Dispose() }
    $preview = @("$Mode setup - inspected $($plan.Inspected) supported files.", $details,
        "Only the explicitly selected files will be edited; sibling generations and account files are NOT added automatically.",
        "Choose the recipient PC's own game-generated config, not a downloaded donor config. GPU/monitor identity, display mode, VRAM budget and worker count are preserved, NOT adapted from a donor file.",
        "A new filename is not proof of compatibility. Only recognized Key@numbers = value entries are supported. Verify these files belong to the active game.", "")
    if ($null -ne $hardware) {
        $preview += "Detected graphics: " + ($hardware.Names -join ", ")
        $preview += "Reflex is enabled only on NVIDIA PCs; Boost is not forced. Only the listed known frame-generation keys can be disabled."
        $preview += "Unknown/new frame-generation keys are NOT automatically disabled. Missing options require checking the game menu."
        $preview += "Verify your intended resolution/refresh rate in-game. No 240 FPS or latency improvement is guaranteed."
        $preview += "Menu/background caps take effect only if the game's custom frame limiter is enabled."
        $preview += ""
    }
    foreach ($entry in $plan.Coverage) {
        $preview += "Coverage: $($entry.Path)"
        $preview += "  Recognized keys: $($entry.Recognized)"
        if ($entry.Missing.Count -gt 0) { $preview += "  Missing / manual verification required: " + ($entry.Missing -join ", ") }
        if ($entry.UnverifiedFrameGeneration.Count -gt 0) {
            $preview += "  WARNING - unrecognized possible frame-generation options LEFT UNCHANGED: " + ($entry.UnverifiedFrameGeneration -join ", ")
        }
    }
    foreach ($file in $plan.Files) {
        $preview += $file.Path
        foreach ($change in $file.Changes) { $preview += "  $($change.Key)@$($change.Scope): $($change.Before) -> $($change.After)" }
    }
    $canApply = @($plan.Files).Count -gt 0
    if (-not $canApply) { $preview += "`nPresent recognized settings already match the baseline. Nothing to apply; missing/unknown settings still need manual verification." }
    $preview += "`nA full backup is created before edits. Only the listed values change. Missing/unknown keys are left alone."
    if (-not (Show-SettingsPreview ($preview -join "`n") $canApply)) { return }
    $backup = Apply-AudioPlan $plan $backups
    [Windows.Forms.MessageBox]::Show("$Mode values updated and verified.`nBackup: $backup`n`nLaunch Warzone and verify its menus: cloud sync/game updates can overwrite local settings. Compare the same benchmark/scene before and after, including FPS, frame-time lows and latency where available. A successful file write does not prove the game loaded these settings.", "EqTune") | Out-Null
}

if ($MyInvocation.InvocationName -ne ".") {
    $mutex = $null
    $ownsMutex = $false
    try {
        if ($Action -eq "Preview") {
            $nvidia = if ($Mode -eq "Graphics") { (Get-GraphicsHardware).NvidiaAvailable } else { $false }
            Get-AudioPlan $PlayersDirectory $Mode $nvidia $ConfigFiles | Select-Object Root, Mode, Inspected, SelectedFiles, Coverage, Files
        } else {
            $created = $false
            $mutex = New-Object Threading.Mutex($true, "Local\EqTune.WarzoneSettings", [ref]$created)
            $ownsMutex = $created
            if (-not $ownsMutex) { throw "EqTune game setup is already open. Close it before starting another audio or graphics setup." }
            Show-AudioSetup
        }
    } catch {
        if ($Action -eq "UI") {
            Add-Type -AssemblyName System.Windows.Forms
            [Windows.Forms.MessageBox]::Show($_.Exception.Message, "EqTune game setup error", "OK", "Error") | Out-Null
        } else { Write-Error $_ }
        exit 1
    } finally {
        if ($ownsMutex) { $mutex.ReleaseMutex() }
        if ($null -ne $mutex) { $mutex.Dispose() }
    }
}
