# EqTune for Wave Link

EqTune is a free Windows x64 VST3 EQ effect for your Call of Duty channel in **Wave Link 3**. No VB-CABLE, extra virtual input, separate capture app, account, activation key, subscription, or paid features.

**Call of Duty -> existing Wave Link game channel -> EqTune effect -> Wave Link mix -> your output devices**

Wave Link still uses its own audio driver/routing. EqTune itself installs no drivers and changes no Windows, game, or Wave Link routing settings. Its optional **Game audio setup** and **Graphics setup** helpers can change supported local game values only after a preview and confirmation.

## Install and use

1. Extract **EqTune-VST3-win-x64.zip** completely. Close Wave Link before installing.
2. Double-click **Install-EqTune.cmd** and approve administrator access. The installer copies only `EqTune.vst3` to `C:\Program Files\Common Files\VST3`.
3. Open Wave Link 3. Rescan VST3 plugins in its audio-effects/plugin settings if EqTune is not listed.
4. On your existing game channel, add **EqTune** as an audio effect and open its editor.
5. Keep managing your listening mixes and headphones/speakers in Wave Link as usual.

You can also copy the complete `EqTune.vst3` **folder**, not just the binary inside, to the standard VST3 directory manually. There is no standalone EXE to open.

If you previously routed the game through CABLE Input for the prototype, first restore the game's normal Wave Link output and confirm sound without EqTune. Remove duplicate/raw game captures to avoid doubled audio. The plugin does not need the legacy standalone EqTune app or VB-CABLE. Removing an unused VB-CABLE driver is optional and separate.

See [Elgato's Wave Link 3 overview](https://www.elgato.com/uk/en/explorer/products/wave/wave-link-3-0-software-overview/) for host routing and plugin settings. This is a standard third-party VST3 effect, not an Elgato product or proprietary integration.

## Controls

- **Headphone selector:** choose DT 990 PRO X, Open-back, Closed-back, IEM, or Gaming headset starting presets. These are experimental tonal starting points, not measured headphone correction or certified Warzone profiles. Selecting a preset replaces the five full-intensity band gains, preserves intensity and bypass, and retains your preamp or reduces it to -8 dB if it was higher. It never raises your preamp. Undo restores the prior tune in one step. The selector replaces the earlier Warzone clarity button; the DT 990 PRO X option uses that same curve.
- **Five frequency faders:** 120 Hz, 350 Hz, 900 Hz, 2.8 kHz, and 5 kHz. Each sets a full-intensity target from -6 to +6 dB (Q 0.8).
- **Intensity:** 0% is flat EQ, 50% is half of every target gain in dB, and 100% is the full selected curve. Both boosts and cuts scale together; frequencies do not move. This affects all sounds in those frequency ranges, not footsteps in isolation.
- **Loudness EQ:** optional stereo-linked dynamic equalization, off by default. Its separate **intensity slider** controls quiet-sound lift and loud-sound compression, not the five frequency bands. The stored starting intensity is 50%; 0% adds no dynamics processing. Start around 25% at a low listening volume. Full effect bypass skips this stage too. Headphone selection preserves both loudness controls; Undo, Restore on-open tune, host automation, and presets include them.
- **Strong:** optional stronger loudness mode (0.2.8), off by default. Enable Loudness EQ first, then Strong; start at 25% on the same loudness intensity slider. Standard mode is unchanged. Strong lifts quiet audio more and compresses loud audio harder, while preserving Preamp. It also raises ambience and can alter distance cues; lower headphone volume before enabling it. Undo, restore, automation, and presets include the mode.
- **Preamp:** -18 to 0 dB. It stays unchanged when intensity is reduced. 0% intensity is flat tonal balance with the selected preamp attenuation, not unity gain. Full effect bypass skips the preamp too.
- **Graph:** shows the applied filter response, excluding preamp, at a 48 kHz reference rate. It is not a spectrum analyzer.
- **Undo change:** reverses recent editor gestures, including intensity and bypass changes. A slider drag is one undo step.
- **Restore on-open tune:** restores the parameters present when you opened this editor, including intensity, preamp, and bypass. Restoration is undoable. Closing/reopening the editor establishes a new checkpoint; undo history is not persisted.
- **Bypass EQ / Wave Link effect disable:** returns the original audio, skipping both EQ and preamp. The plugin crossfades over 20 ms, then passes samples through exactly, without clipping. Uncheck/re-enable to restore the previous tune and preamp; settings are not erased. **Lower your listening volume first: bypass can be significantly louder than the attenuated processed signal.** For a tonal comparison at the same preamp setting, compare 0% and 100% intensity instead.
- **Game audio setup:** opens a separate Windows helper to preview/apply a limited Warzone audio baseline or restore a previous backup. Close the game first. Nothing is changed by simply clicking the button.
- **Graphics setup:** separately previews a competitive-clarity/low-latency starting preset, backs up before applying, and can restore previous graphics values. It is not a hardware benchmark or guaranteed maximum-FPS preset.

The starter curve is -4, -2, -1, +2, +3 dB, with -8 dB preamp and 100% intensity. The effect smooths EQ changes over roughly 20 ms, supports mono/stereo 32-bit and 64-bit audio, and reports zero added buffering latency. It does not add a virtual cable round-trip.

Wave Link/the host owns saving plugin settings and presets. Save an effect preset or host configuration before experimenting. Standalone prototype JSON settings are not automatically imported into the plugin.

### Headphone starting presets

| Selection | 120 Hz | 350 Hz | 900 Hz | 2.8 kHz | 5 kHz |
|---|---:|---:|---:|---:|---:|
| DT 990 PRO X | -6 | -3 | +1 | +4 | +1 |
| Open-back (generic) | -4 | -2 | 0 | +2 | +1 |
| Closed-back (generic) | -5 | -3 | 0 | +3 | +1 |
| IEM (generic) | -2 | -1 | 0 | +1 | 0 |
| Gaming headset (generic) | -4 | -3 | -1 | +3 | +2 |

Values are gain in dB, not differences measured from your headphones. Headphone type alone does not determine frequency response; adjust by listening, and do not assume these compensate for a particular model. IEM fit/seal can substantially affect the result.

The dropdown reflects the current five target gains. If they do not match a starting preset it displays **Custom - current EQ**, including the unchanged original starter curve. Editing a band switches to Custom unless the resulting curve matches another preset; changing intensity, preamp, or bypass does not change the preset label. Choosing Custom keeps the current tune rather than loading a separate preset. Host presets persist the actual gains, so the matching headphone label returns after reopening or reloading, without changing the existing plugin state format. No preset is automatically applied on update.

After selecting a profile, uncheck Bypass and raise Intensity above 0% to hear it. This selection does not enable spatial processing or identify footsteps.

Start at a comfortable listening level. This is an original general-purpose curve, not a measured profile for every Call of Duty title/headset, and it cannot guarantee better awareness. Avoid excessive boosts; output is bounded at full scale, which can distort if you remove too much headroom.

### Dynamic loudness equalization

Version **0.2.7** adds an original compressor/makeup stage inside the VST3. This is not the Windows Loudness Equalization enhancement and does not change Windows sound settings. It processes all game-channel audio, not footsteps alone; ambience, dialogue, hiss, and other quiet sounds can become more audible, and dynamics/distance cues can change. Avoid stacking it with another loudness equalizer or compressor while comparing.

At 100% intensity the linked peak detector uses a 5 ms attack and 200 ms release, a -30 dBFS compression threshold and 4:1 ratio, with up to 12 dB makeup gain. Quiet makeup fades out between -55 and -70 dBFS detector level so very low noise is not continually boosted; this is not a noise gate and does not mute input. Intensity scales the combined gain change in dB (for example a settled quiet signal gets about +6 dB at 50%, +12 dB at 100%). Loud sustained audio above the threshold is reduced. Gain and the instantaneous peak protection are linked across stereo channels to preserve channel level differences, with independent EQ filter histories and no crossfeed.

**Strong mode** uses a -36 dBFS threshold, 6:1 ratio, and up to 18 dB makeup gain, retaining the same timing, noise-floor fade, stereo linking, and peak guard. A settled -46 dBFS signal receives about +9 dB at 50% or +18 dB at 100%, versus +6/+12 dB in Standard. This is stronger dynamic leveling, not a constant output-volume increase; loud sustained signals are still reduced. Strong toggles blend gain curves over about 20 ms. It is not a reproduction of Windows Loudness Equalization and is not guaranteed to match its loudness.

The detector measures the EQ signal before preamp attenuation, so changing Preamp still changes output level rather than retuning the compressor. Preamp remains unchanged when either intensity slider moves. At full loudness intensity an instantaneous linked sample-peak guard limits the wet signal to -1 dBFS; its ceiling moves toward 0 dBFS at lower intensities. It adds no lookahead or buffering latency, **is not a true-peak/lookahead limiter**, and can distort abrupt peaks. The existing hard output clamp remains when loudness is off. Neither processing nor a digital ceiling guarantees a safe headphone listening level.

Loudness enable/intensity changes ramp over about 20 ms. Full effect bypass still settles to exact original samples (including preamp/dynamics/ceiling bypass), so it may be louder. The frequency graph remains a tonal EQ response, **not** a display of level-dependent loudness gain. Existing version-1 presets load unchanged with loudness off and 50% stored intensity. Version-2 presets retain all ten existing parameters with Strong off. Version-3 presets save all eleven parameters; older plugin builds cannot read the new state format, so keep old host presets before updating if you need to downgrade.

## If intensity sounds unchanged

Version **0.2.2** changes the shared host/editor bypass from EQ-only bypass to full dry bypass. Older versions kept preamp attenuation even when Wave Link disabled the effect. This corrects bypass behavior but does not prove that retained preamp was the reason for an inaudible tonal change on a particular system. The plugin version appears in host plugin metadata.

1. At a low listening volume, move Preamp from 0 to -18 dB. A clear volume drop confirms that at least some monitored audio is passing through EqTune. If there is no change, check the Wave Link channel, effect enable/bypass, and the mix/output you are actually listening to before changing the curve.
2. With EQ bypass unchecked, select a headphone starting preset (for example **DT 990 PRO X**). Compare intensity at 0% and 100% over the same repeatable game clip or training scenario. Keep preamp, game mix, and listening volume fixed. The difference is tonal balance, not an overall footstep volume control.
3. If Preamp works but intensity still seems unchanged, confirm the five target gains are not all zero, and check for a duplicate dry game route or later EQ/level processing that masks the difference. Save your existing Wave Link configuration before disabling other effects for comparison.
4. Test game-native headphone/binaural audio with a repeatable left/right/front/back scenario. EqTune applies the same filters independently to left and right: it preserves stereo separation, but does not add HRTF, surround, positional reconstruction, or selective footstep detection. Avoid stacking multiple spatial processors while comparing directional cues.

## Revert or uninstall

For immediate normal audio, remove EqTune from the game channel's effect chain (turn down volume first, since removing the preamp attenuation raises level). Your original Wave Link routing stays in place. There are no system-wide EQ settings to restore.

To uninstall, remove/disable the effect, close Wave Link, and double-click **Uninstall-EqTune.cmd** from the extracted download folder. It removes only the `EqTune.vst3` bundle; host-saved presets and unrelated plugins are kept.

If you used either game-setup button, restore its backup before uninstalling if you want to undo those game settings. Uninstall does not reset game settings or delete `%LOCALAPPDATA%\EqTune\GameAudioBackups` or `%LOCALAPPDATA%\EqTune\GameGraphicsBackups`.

For an update, save a host preset, close Wave Link, uninstall the previous bundle, then install the new one. The installer refuses to overwrite an existing plugin rather than risk a partial in-place update.

## Optional automatic game-audio setup

Version **0.2.4** added **Game audio setup** to the plugin editor. Click it with Call of Duty closed:

1. Choose **Yes** to set up audio, then select the active config file(s) in your `Call of Duty\players` folder or an account subfolder (the picker starts in your Windows Documents location, including OneDrive redirection). As of **0.2.6**, use Ctrl/Shift to select multiple files from the same folder; run setup separately for different folders.
2. Read the exact file/key/value preview and confirm. Full originals and a change manifest are saved under `%LOCALAPPDATA%\EqTune\GameAudioBackups` before any writes.
3. Relaunch the game and check the Audio menu. Game updates/cloud sync can overwrite local settings; a successful file write does not prove that the current game build uses that config.
4. To revert, click the button again, choose **No**, and select `manifest.json` from the desired backup. Only the audio values changed by that backup are restored, preserving unrelated later settings. A later change to an affected audio value causes a conflict instead of being overwritten. Restore newest backups first.

The conservative starting baseline is:

| Supported existing key | Requested value |
|---|---|
| MusicVolume, CinematicVolume, WarTracksVolume | 0% |
| VoiceVolume (dialogue/announcer, not voice chat) | 50% |
| MonoSound | Off (stereo) |
| AltShellShock (tinnitus reduction) | On |

Master volume, effects volume, voice chat, audio devices, Audio Mix, requested channel count, spatial/headphone processing, graphics, and controls are **unchanged**. Your local Audio Mix field did not provide a verified internal preset value, so the helper does not guess one. Choose a mix and headphone/spatial options manually by comparing a repeatable scenario; there is no verified universal "best competitive" combination.

The originally inspected UTF-8 configs were `s.1.0.cod24.txt0/.txt1` and `g.p.1.0.l.txt0/.txt1`. As of **0.2.6**, the UI edits only explicitly selected files: select both generations yourself if appropriate. It does not automatically add siblings or account files. New filenames can be selected, but their contents must contain recognized `Key@numbers = value` entries. A new filename does not establish support for a new title's settings. Files with no recognized keys, malformed/duplicate target keys, read-only files, stale previews, and a running game block changes. Missing/unknown keys are not invented or translated. The helper does not edit binary files, generation markers, checksums, game executables, registry values, or cloud settings. No game-process interaction is used beyond checking process names.

The helper runs as a separate user-level Windows PowerShell process, not on the audio thread, with no administrator request or global execution-policy change. Both plugin switches and this helper are optional; presets alone do not modify game files. Configuration backups can contain account/device information: keep them private.

If applying fails partway, the error shows the backup location; use Restore to undo completed edits. If restoring fails partway, keep the backup and retry once the file access problem is resolved. Neither operation locks out the launcher or cloud sync, so keep the game and launcher synchronization inactive during edits.

## Optional competitive graphics setup

Version **0.2.5** added a separate **Graphics setup** button; **0.2.6** adds explicit config-file selection and verified cod26-style scoped settings. It shares the audio helper's preview, full-original backups, atomic per-file writes, and selective restore, without applying the audio preset. Only one EqTune setup window can be open at a time.

1. Close Call of Duty and wait for launcher/cloud synchronization to finish.
2. Click **Graphics setup**, choose **Yes**, and select the recipient PC's own active, game-generated config file(s) inside `Call of Duty\players`, not a downloaded donor config. Ctrl/Shift selects multiple files in one folder. Only selected files are edited, including when newer COD releases use different filenames.
3. Review the scrollable file/key/value preview, then click **Back up + apply** if you accept the trade-offs. Cancel makes no changes. Backups go to `%LOCALAPPDATA%\EqTune\GameGraphicsBackups`.
4. Relaunch the game and verify every changed option in its menus. For a repeatable comparison, record average FPS, frame-time lows/1% lows, GPU usage, and available render/PC latency metrics in the same scene before and after.
5. To revert, click **Graphics setup**, choose **No**, and select the backup's `manifest.json`. Restore newest backups first. Later audio, controls, or unrelated graphics changes are preserved; conflicting edits to the backed-up graphics keys stop the restore.

The shareable preset uses the same conservative graphics targets on supported PCs, plus hardware-detected NVIDIA Reflex. It does **not** silently import the developer's hardware or display settings:

| Supported existing key(s) | Requested value / trade-off |
|---|---|
| VSync, VSyncInMenu | disabled; tearing is possible |
| NvidiaReflex | Enabled, only when a local NVIDIA graphics adapter is detected; does not force Boost |
| DepthOfField, EnableVelocityBasedBlur | false; removes supported focus/velocity blur effects |
| DofEnable, MotionBlur | Off; newer focus/world-motion blur controls |
| DofWeaponDisable, DofWorldDisable | true |
| WeaponMotionBlur | false |
| DLSSFrameGeneration, FSRFrameInterpolation, IntelXeFG | false; avoids prioritizing generated display FPS over response time |
| DynamicSceneResolution | false; preserves a stable render-resolution target rather than chasing a dynamic target |
| DxrMode | Off |
| ShadowQuality | Very_Low |
| ScreenSpaceShadowQuality, SSRQuality | Off |
| VolumetricQuality | QUALITY_LOW |
| WeatherGridVolumesQuality, WaterCausticsMode | Off |
| WaterWaveWetness | false |
| ParticleQuality | very low |
| ShaderQuality | Low |
| ModelQuality | Low Quality |
| WorldStreamingQuality | Low; lower detail/pop-in is possible |
| ShadingQuality, ShadowFilteringQuality | Low |
| TerrainQuality | Very Low |
| Tessellation | 0_Off |
| DeferredPhysics | Low Quality |
| ReflectionProbeHalfResolution | true |
| SubdivisionLevel | 0 |
| MaxFpsInMenu, MaxFpsOutOfFocus | 60 / 30; effective only if the game's custom frame limiter is enabled |

Version **0.2.6** accepts individually selected regular `.txt`, `.txt0/.txt1` (and other numbered `.txt` generations), `.cfg`, `.ini`, and `.cst` files. Files must be UTF-8 text with recognized `Key@numbers = value` entries; extensions alone do not imply content support. For example, an older `Key:0.0 = "value"` layout is refused if it has no recognized entries. Only known target keys and supported value types/enums are edited. Scope 0 is supported for all target keys; both scopes 0 and 1 are separately supported for `DxrMode`, `VolumetricQuality`, and `SSRQuality`, as verified in the supplied cod26 reference. Duplicate keys within the same scope and unverified scopes are refused rather than guessed.

The preview lists recognized-key counts, missing options requiring manual checks, and unrecognized possible frame-generation keys left unchanged. Unknown formats require a verified mapping, not guesses. A file with no recognized settings is refused before any writes; partially recognized files can apply only the listed supported changes. No sibling/title/account config is automatically selected. Full originals and the exact selected paths are recorded in version-2 backups, with each change's scope; older audio/graphics backups without scope fields remain restorable.

### Portable competitive reference, not a whole-file import

The portable target values above were checked against the supplied competitive cod26 reference. EqTune does not ship or import that entire file. It deliberately omits donor GPU/monitor identities, detected hardware/driver records, resolution/refresh rate, VRAM budgets, worker count, upscaling preference/quality, and match cap. It also leaves personal gameplay choices such as blood, brass, bullet impacts, and controls alone. The reference's menu limit of 120 is intentionally not copied: EqTune retains its 60 FPS menu target, effective only when the game's limiter is enabled.

This accommodates different PCs by **preserving the recipient's game-detected hardware settings**, not by claiming to auto-benchmark an optimal configuration. Apply to each PC's own game-generated file. Applying to a downloaded donor file would leave that donor's hardware fields intact; it does not convert it into the recipient's config. VRAM targets and worker counts need actual frame-time testing, especially on hybrid-core CPUs and power-limited laptops. GPU detection only gates the Reflex target; it does not change the GPU selection or identify which adapter is actively rendering.

Known NVIDIA/AMD/Intel frame-generation enable switches are disabled when present. Newer DLSS multi-frame-generation counters/mode keys are left unchanged and flagged: disabling the known parent enable switch is not proof that future switches or driver overrides are off. Verify frame generation is off in-game and in driver overrides. DLSS super resolution / FidelityFX CAS may remain enabled.

**Left unchanged:** resolution, refresh rate, monitor selection, fullscreen mode, render scale, anti-aliasing/upscaling selection and quality, texture quality/filtering, VRAM budgets, worker-thread count, in-match frame limit/limiter enable, FOV, controls, sensitivity, accessibility, audio, Windows/driver settings, overclocking, and power plans. No binary patching, anti-cheat bypass, registry/boot/timer tweaks, or hidden visibility commands.

For a **2560x1440 / 240 Hz** display, manually confirm both Windows and the game use that display mode. A laptop RTX 4060 cannot be promised 240 FPS in Warzone at 1440p; power limits, thermals, map/load, and CPU/GPU bottlenecks matter. **DLSS super resolution and FidelityFX CAS are allowed; frame generation is not part of this preset.** Choose your upscaling/sharpening setting in-game; EqTune leaves the existing choice unchanged rather than guessing preset enums. Super-resolution upscaling is not frame generation and can reduce GPU render time; its latency effect depends on the bottleneck.

**No G-SYNC enable:** EqTune never enables G-SYNC/VRR and does not change NVIDIA driver or monitor settings. For G-SYNC off, uncheck **Enable G-SYNC, G-SYNC Compatible** in NVIDIA Control Panel's **Set up G-SYNC** page where available, then Apply; check monitor/VRR controls separately if needed. V-Sync is disabled only in recognized game keys. This helper cannot certify that driver-level VRR, frame-generation overrides, or unfamiliar game options are off: verify them manually. Do not copy a fixed match cap or worker count from another PC.

NVIDIA's [system latency optimization guide](https://www.nvidia.com/en-us/geforce/guides/system-latency-optimization-guide/) explains Reflex and the V-Sync/VRR trade-offs. Higher generated FPS is not the same measurement as input latency. EqTune's graphics preset has synthetic file-edit tests, **not an in-game performance certification**. Config files can be inactive or overwritten by updates/cloud sync; successful writes do not establish that the game loaded them.

## Build and validate

Requirements: Windows x64, Visual Studio 2022 Build Tools with **Desktop development with C++**, the Windows SDK, CMake tools, and Git. No .NET SDK is required for the plugin. The release statically links the Microsoft C++ runtime.

```powershell
.\Build-Release.ps1
```

The build restores the MIT-licensed Steinberg VST3 SDK pinned to `v3.8.0_build_66`, compiles the plugin, runs Steinberg's validator plus DSP/editor/state checks, and creates `release\EqTune-VST3-win-x64.zip`.

Checks include:

- Steinberg's 47 validator checks for host interfaces, buses, processing formats, silence, automation, and bypass persistence.
- DSP tests at 44.1/48/96 kHz, mono/stereo, float/double, 0/50/100% intensity, exact dry bypass (including above-full-scale input), live bypass transitions, stereo isolation, in-place changes, and response agreement within 0.02 dB at 1 kHz.
- Loading the actual plugin binary in a test host, editing intensity, undo/restore/bypass and clarity preset, editor teardown/reopen, host edit gestures, state round-trip, and rejecting truncated state. UI edits are forwarded through the host parameter queue to actual audio: the clarity preset must cut a 120 Hz tone by over 5 dB and boost a 2.8 kHz tone by over 3 dB between 0% and 100%, and the preamp sweep must change audio by 18 dB within 0.05 dB.
- Extracting the release ZIP, checking required files and notices, installing into a temporary plugin directory, verifying the binary hash, refusing an overwrite, and uninstalling without removing unrelated files.
- Host-originated bypass parameter changes with a -18 dB preamp: disabled output must match input samples exactly after transition, and re-enabling must recover the previous processed level within 0.02 dB.
- All five headphone starting profiles, gain matching/Custom labels, one-step undo, editor reopen and state round-trip, preserved intensity/bypass/lower preamp, and steady-state response below preset headroom.
- Synthetic game-config checks for audio-only edits, both file generations, full backups, selective restore preserving unrelated later edits, conflict/stale-preview refusal, duplicate keys, game-running guard, multiple accounts, unsupported filenames, and UTF-8 BOM/LF round-trip. Tests do not alter your live game configs.
- Graphics config tests for exact preset targets, NVIDIA/non-NVIDIA gating, protected audio/gameplay/display/upscaling settings, original backups, selective restore, read-only/stale/conflict guards, malformed values, first-key BOM/LF round-trip, and legacy audio backup compatibility. Graphics tests also run against the installed helper in the temporary package test directory.
- Explicit single/multi-file selection under new filenames/extensions, no unselected sibling edits, version-2 backup path restrictions, selected-file audio restore, unsupported/binary/marker refusal, missing-key and unknown frame-generation warnings, unchanged DLSS/CAS selections, and no G-SYNC enable.
- Loudness DSP checks at 44.1/48/96 kHz in mono/stereo float/double: 0/50/100% quiet lift, loud-signal reduction, silence/low-noise behavior, preserved preamp attenuation, linked stereo ratios, transient sample ceiling, in-place changes, live toggles and exact full bypass. Actual-plugin host tests exercise loudness controls through the parameter queue into measured audio, undo, headphone-profile preservation, state round-trip, old-preset migration and truncated-state refusal.
- Strong-mode DSP/actual-plugin tests verify +9/+18 dB quiet lift at 50/100%, +6 dB over Standard at full intensity, loud-signal reduction, preserved stereo ratios/preamp, silence/noise floor, peak ceiling/bypass, checkbox undo, and version-1/version-2 preset migration with Strong off.

These are not a substitute for playback testing inside Wave Link 3 on your hardware. That final compatibility/listening check remains necessary.

The earlier C# standalone source and tests remain in `EqTune` and `EqTune.Tests` for reference, but are not built or shipped by the plugin release script. Older `EqTune-win-x64.zip` downloads are the obsolete cable-based prototype: use the **VST3** ZIP instead.

## Licensing

Free to use and share under the MIT license. The Steinberg SDK's MIT notice ships with the release and inside the plugin bundle. No ArtTuneDB code, presets, or assets are copied. VST is a trademark of Steinberg Media Technologies GmbH.
