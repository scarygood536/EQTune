$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "..\Resources\WarzoneAudioSetup.ps1")
# These tests use synthetic configs only and never inspect running games.
function Assert-GameClosed {}
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
$root = Join-Path ([IO.Path]::GetTempPath()) ("EqTune-game-settings-test-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $root | Out-Null
try {
    $players = Join-Path $root "players"
    $account = Join-Path $players "test-account"
    New-Item -ItemType Directory -Path $account -Force | Out-Null
    $config = @(
        "// synthetic audio fixture",
        "MusicVolume@0;54363;21371 = 0.750000 // 0.000000 to 1.000000",
        "CinematicVolume@0;54363;21371 = 0.600000",
        "WarTracksVolume@0;54363;21371 = 0.250000",
        "VoiceVolume@0;54363;21371 = 1.000000",
        "MonoSound@0;57752;48403 = true",
        "AltShellShock@0;57752;48403 = false",
        "Volume@0;1;2 = 0.840000",
        "EffectsVolume@0;1;2 = 0.700000",
        "AudioMixPreset@0;5555;35888 = ",
        "ImmerseSpatialAudio@0;57752;48403 = true",
        "SoundOutputDevice@0;2059;35888 = unchanged",
        "GraphicsSetting@0;1;2 = 5"
    ) -join "`r`n"
    $utf8 = New-Object Text.UTF8Encoding($false)
    foreach ($name in @("s.1.0.cod24.txt0", "s.1.0.cod24.txt1")) {
        [IO.File]::WriteAllText((Join-Path $players $name), $config, $utf8)
    }
    foreach ($name in @("g.p.1.0.l.txt0", "g.p.1.0.l.txt1")) {
        [IO.File]::WriteAllText((Join-Path $account $name), $config, $utf8)
    }
    $unsupported = Join-Path $players "s.1.0.cod25.txt0"
    [IO.File]::WriteAllText($unsupported, $config, $utf8)
    $plan = Get-AudioPlan $players
    Assert-True ($plan.Inspected -eq 4 -and $plan.Files.Count -eq 4) "Expected both generations of system/account fixtures"
    $backups = Join-Path $root "backups"
    $backup = Apply-AudioPlan $plan $backups
    Assert-True ((Get-AudioPlan $players).Files.Count -eq 0) "Reapply should be a no-op"
    Assert-True ((Read-ConfigText $unsupported) -ceq $config) "Unknown file format was modified"
    $manifest = Get-Content -LiteralPath (Join-Path $backup "manifest.json") -Raw | ConvertFrom-Json
    Assert-True ($manifest.Status -eq "Applied") "Backup must record completed application"
    foreach ($file in $plan.Files) {
        $actual = Read-ConfigText $file.Path
        Assert-True ($actual -ceq $file.After) "Written config differs from preview"
        $edited = $actual.Replace("GraphicsSetting@0;1;2 = 5", "GraphicsSetting@0;1;2 = 8")
        [IO.File]::WriteAllText($file.Path, $edited, $utf8)
    }
    $manifest.PSObject.Properties.Remove("Mode")
    foreach ($file in $manifest.Files) {
        foreach ($change in $file.Changes) { $change.PSObject.Properties.Remove("Scope") }
    }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $backup "manifest.json") -Encoding UTF8
    Restore-AudioBackup $backup
    foreach ($file in $plan.Files) {
        Assert-True ((Read-ConfigText $file.Path) -ceq $config.Replace("GraphicsSetting@0;1;2 = 5", "GraphicsSetting@0;1;2 = 8")) "Restore must preserve unrelated later edits"
    }
    $secondPlan = Get-AudioPlan $players
    $secondBackup = Apply-AudioPlan $secondPlan $backups
    $first = $secondPlan.Files[0].Path
    $text = Read-ConfigText $first
    [IO.File]::WriteAllText($first, $text.Replace("VoiceVolume@0;54363;21371 = 0.500000", "VoiceVolume@0;54363;21371 = 0.300000"), $utf8)
    $refused = $false
    try { Restore-AudioBackup $secondBackup } catch {
        if ($_.Exception.Message -notmatch "changed since") { throw }
        $refused = $true
    }
    Assert-True $refused "Restore must refuse a later edit to an affected audio key"
    Assert-True ((Read-ConfigText $secondPlan.Files[1].Path) -ceq $secondPlan.Files[1].After) "Conflict should abort before restoring other files"
    $stalePlan = Get-AudioPlan $players
    [IO.File]::AppendAllText($first, "`r`n// external edit", $utf8)
    $refused = $false
    try { Apply-AudioPlan $stalePlan $backups } catch {
        if ($_.Exception.Message -notmatch "changed since preview") { throw }
        $refused = $true
    }
    Assert-True $refused "Stale preview should be refused"
    $bad = $config + "`r`nMusicVolume@0;1;2 = 1.000000"
    $refused = $false
    try { Get-KeyMatch $bad "MusicVolume" | Out-Null } catch { $refused = $true }
    Assert-True $refused "Duplicate settings should be rejected"
    Assert-True ((Read-ConfigText (Join-Path $backup "0.original")) -ceq $config) "Full original backup is not exact"
    function Assert-GameClosed { throw "Close Call of Duty before applying settings." }
    $refused = $false
    try { Apply-AudioPlan (Get-AudioPlan $players) $backups } catch {
        if ($_.Exception.Message -notmatch "Close Call of Duty") { throw }
        $refused = $true
    }
    Assert-True $refused "Apply must block when the game guard fails"
    $refused = $false
    try { Restore-AudioBackup $secondBackup } catch {
        if ($_.Exception.Message -notmatch "Close Call of Duty") { throw }
        $refused = $true
    }
    Assert-True $refused "Restore must block when the game guard fails"
    function Assert-GameClosed {}
    $single = Join-Path $root "single-account"
    New-Item -ItemType Directory -Path $single | Out-Null
    $singleFile = Join-Path $single "g.p.1.0.l.txt0"
    $lfConfig = $config.Replace("`r`n", "`n")
    [IO.File]::WriteAllText($singleFile, $lfConfig, (New-Object Text.UTF8Encoding($true)))
    $originalBytes = [Convert]::ToBase64String([IO.File]::ReadAllBytes($singleFile))
    $singleBackup = Apply-AudioPlan (Get-AudioPlan $single) $backups
    Restore-AudioBackup $singleBackup
    Assert-True ([Convert]::ToBase64String([IO.File]::ReadAllBytes($singleFile)) -ceq $originalBytes) "Restore must retain UTF-8 BOM and LF line endings"
    $secondAccount = Join-Path $players "another-account"
    New-Item -ItemType Directory -Path $secondAccount | Out-Null
    [IO.File]::WriteAllText((Join-Path $secondAccount "g.p.1.0.l.txt0"), $config, $utf8)
    $refused = $false
    try { Get-AudioPlan $players | Out-Null } catch {
        if ($_.Exception.Message -notmatch "Multiple game profiles") { throw }
        $refused = $true
    }
    Assert-True $refused "Multiple accounts require explicit folder selection"
    Write-Output "PASS: audio-only config edits, preview, backup, no-op, both file generations, restore preserving other settings, conflicts, stale previews, duplicate keys, unknown format."
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force
}
