using System.Diagnostics;
using System.Text.Json;
using NAudio.CoreAudioApi;
using NAudio.Wave;
using NAudio.Wave.SampleProviders;

namespace EqTune;

internal sealed class MainForm : Form
{
    private static readonly EqBand[] DefaultBands =
    [
        new("Low", 120, -4, 0.8f),
        new("Low-mid", 350, -2, 0.8f),
        new("Mid", 900, -1, 0.8f),
        new("Presence", 2800, 2, 0.8f),
        new("Clarity", 5000, 3, 0.8f)
    ];

    private readonly string _settingsPath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        "EqTune",
        "settings.json");
    private readonly MMDeviceEnumerator _deviceEnumerator = new();
    private readonly ComboBox _inputDevices = new();
    private readonly ComboBox _outputDevices = new();
    private readonly TrackBar[] _bandSliders = new TrackBar[DefaultBands.Length];
    private readonly TrackBar _preampSlider = new();
    private readonly TrackBar _intensitySlider = new();
    private readonly Label _intensityValue = new();
    private readonly Label _preampValue = new();
    private readonly Label _status = new();
    private readonly Button _startButton = new();
    private readonly Button _stopButton = new();
    private readonly Button _undoButton = new();
    private readonly Button _restoreButton = new();
    private readonly CheckBox _bypass = new();
    private readonly EqCurve _curve = new();
    private readonly Label _modeLabel = new();
    private readonly ToolTip _tips = new();
    private readonly Stack<int[]> _undoTunes = new();
    private int[] _savedTune = [];
    private int[] _currentTune = [];
    private bool _applyingTune;

    private WasapiCapture? _capture;
    private WasapiOut? _output;
    private BufferedWaveProvider? _buffer;
    private EqSampleProvider? _equalizer;
    private AppSettings _settings = new();

    public MainForm()
    {
        Text = "EqTune - Call of Duty";
        MinimumSize = new Size(850, 680);
        Size = new Size(1000, 950);
        StartPosition = FormStartPosition.CenterScreen;
        Font = new Font("Segoe UI", 10);
        BackColor = Color.FromArgb(13, 18, 26);
        ForeColor = Color.FromArgb(232, 239, 248);
        AutoScaleMode = AutoScaleMode.Dpi;

        BuildInterface();
        Load += (_, _) =>
        {
            _applyingTune = true;
            try
            {
                LoadUserSettings();
            }
            finally
            {
                _applyingTune = false;
                _savedTune = ReadTune();
                _currentTune = ReadTune();
                UpdateTuneButtons();
                UpdateEqualizer();
            }
        };
        FormClosing += OnFormClosing;
        FormClosed += (_, _) =>
        {
            SaveUserSettings();
            _deviceEnumerator.Dispose();
        };
        Disposed += (_, _) => _tips.Dispose();
        RefreshDevices();
        SetRunning(false);
    }

    private void BuildInterface()
    {
        var page = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            Padding = new Padding(24),
            ColumnCount = 1,
            RowCount = 5,
            AutoScroll = true
        };
        for (var row = 0; row < 5; row++)
        {
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        }
        Controls.Add(page);
        var header = CreateCard(2);
        header.BackColor = BackColor;
        header.Padding = Padding.Empty;
        header.Controls.Add(CreateText("E Q T U N E", 22, ForeColor));
        header.Controls.Add(CreateText("CALL OF DUTY", 10, Color.FromArgb(85, 224, 187)));
        page.Controls.Add(header, 0, 0);

        var deviceGroup = CreateCard(3);
        deviceGroup.Controls.Add(CreateText("01   CONNECT YOUR AUDIO", 11, ForeColor));
        var deviceLayout = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            ColumnCount = 2,
            RowCount = 2
        };
        deviceLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 145));
        deviceLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        deviceLayout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        deviceLayout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        deviceGroup.Controls.Add(deviceLayout);
        ConfigureCombo(_inputDevices);
        ConfigureCombo(_outputDevices);
        _inputDevices.AccessibleName = "Virtual cable recording device";
        _outputDevices.AccessibleName = "Wave Link channel or playback device";
        _tips.SetToolTip(_inputDevices, "Choose CABLE Output after installing VB-CABLE. Do not select your microphone.");
        _tips.SetToolTip(_outputDevices, "Choose a Wave Link virtual channel to mix processed audio in Wave Link, or select headphones for direct playback.");
        deviceLayout.Controls.Add(CreateFieldLabel("Virtual cable"), 0, 0);
        deviceLayout.Controls.Add(_inputDevices, 1, 0);
        deviceLayout.Controls.Add(CreateFieldLabel("Send audio to"), 0, 1);
        deviceLayout.Controls.Add(_outputDevices, 1, 1);
        var deviceButtons = new FlowLayoutPanel { Height = 56, Dock = DockStyle.Top, WrapContents = false };
        _startButton.Text = "Start EQ";
        StyleButton(_startButton, primary: true);
        _startButton.Click += (_, _) => StartAudio();
        _stopButton.Text = "Stop";
        StyleButton(_stopButton);
        _stopButton.Click += (_, _) => StopAudio(confirm: true);
        var refreshButton = new Button { Text = "Refresh devices" };
        StyleButton(refreshButton);
        refreshButton.Click += (_, _) => RefreshDevices();
        var soundSettingsButton = new Button { Text = "Sound settings" };
        StyleButton(soundSettingsButton);
        soundSettingsButton.Click += (_, _) => OpenSoundSettings();
        deviceButtons.Controls.AddRange([_startButton, _stopButton, refreshButton, soundSettingsButton]);
        deviceGroup.Controls.Add(deviceButtons);
        page.Controls.Add(deviceGroup, 0, 1);

        var eqGroup = CreateCard(7);
        eqGroup.Controls.Add(CreateText("02   SHAPE YOUR SOUND", 11, ForeColor));
        _modeLabel.Text = "EQ ACTIVE  /  Filter response in dB, before preamp";
        _modeLabel.AutoSize = true;
        _modeLabel.ForeColor = Color.FromArgb(85, 224, 187);
        _modeLabel.Margin = new Padding(0, 4, 0, 8);
        eqGroup.Controls.Add(_modeLabel);
        var intensityLayout = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            ColumnCount = 3,
            RowCount = 2,
            Margin = new Padding(0, 4, 0, 8)
        };
        intensityLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 100));
        intensityLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        intensityLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 65));
        intensityLayout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        intensityLayout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        intensityLayout.Controls.Add(CreateFieldLabel("Intensity"), 0, 0);
        _intensitySlider.Minimum = 0;
        _intensitySlider.Maximum = 100;
        _intensitySlider.Value = 100;
        _intensitySlider.TickFrequency = 25;
        _intensitySlider.LargeChange = 10;
        _intensitySlider.AutoSize = false;
        _intensitySlider.Height = 36;
        _intensitySlider.Dock = DockStyle.Fill;
        _intensitySlider.BackColor = eqGroup.BackColor;
        _intensitySlider.AccessibleName = "EQ intensity percent, zero is flat, one hundred is full tune";
        _intensityValue.Text = "100%";
        _intensityValue.AutoSize = true;
        _intensityValue.Anchor = AnchorStyles.Left;
        _intensityValue.ForeColor = Color.FromArgb(85, 224, 187);
        _intensitySlider.ValueChanged += (_, _) =>
        {
            _intensityValue.Text = $"{_intensitySlider.Value}%";
            OnTuneChanged();
        };
        _tips.SetToolTip(_intensitySlider,
            "0% = flat EQ. 100% = full selected boosts and cuts. Frequencies and preamp stay unchanged; this does not isolate footsteps.");
        intensityLayout.Controls.Add(_intensitySlider, 1, 0);
        intensityLayout.Controls.Add(_intensityValue, 2, 0);
        var intensityHelp = CreateText("0% Flat audio                         100% Full tune  /  Preamp stays unchanged", 9,
            Color.FromArgb(160, 176, 195));
        intensityLayout.Controls.Add(intensityHelp, 0, 1);
        intensityLayout.SetColumnSpan(intensityHelp, 3);
        eqGroup.Controls.Add(intensityLayout);
        eqGroup.Controls.Add(_curve);
        var bandsLayout = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            ColumnCount = DefaultBands.Length + 1,
            RowCount = 1,
            Margin = new Padding(0, 12, 0, 0)
        };
        for (var i = 0; i <= DefaultBands.Length; i++)
        {
            bandsLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f / (DefaultBands.Length + 1)));
        }
        eqGroup.Controls.Add(bandsLayout);
        for (var i = 0; i < DefaultBands.Length; i++)
        {
            var band = DefaultBands[i];
            var slider = CreateSlider(-6, 6, (int)band.GainDb, $"{band.Name} gain in decibels");
            var value = CreateText($"{slider.Value:+#;-#;0} dB", 12, Color.FromArgb(85, 224, 187));
            slider.ValueChanged += (_, _) =>
            {
                value.Text = $"{slider.Value:+#;-#;0} dB";
                OnTuneChanged();
            };
            _bandSliders[i] = slider;
            var frequency = band.Frequency < 1000 ? $"{band.Frequency:0} Hz" : $"{band.Frequency / 1000f:0.#} kHz";
            bandsLayout.Controls.Add(CreateBandColumn(band.Name, frequency, slider, value), i, 0);
            _tips.SetToolTip(slider, $"{frequency} | Up boosts, down cuts. Arrow keys fine-tune by 1 dB.");
        }
        _preampSlider.Minimum = -18;
        _preampSlider.Maximum = 0;
        _preampSlider.Value = -8;
        _preampSlider.TickFrequency = 3;
        ConfigureVerticalSlider(_preampSlider, "Preamp attenuation in decibels");
        _preampValue.AutoSize = true;
        _preampValue.Text = $"{_preampSlider.Value} dB";
        _preampValue.Font = new Font("Segoe UI Semibold", 12);
        _preampValue.ForeColor = Color.FromArgb(85, 224, 187);
        _preampSlider.ValueChanged += (_, _) =>
        {
            _preampValue.Text = $"{_preampSlider.Value} dB";
            OnTuneChanged();
        };
        bandsLayout.Controls.Add(CreateBandColumn("Preamp", "Headroom", _preampSlider, _preampValue), DefaultBands.Length, 0);
        _tips.SetToolTip(_preampSlider, "Overall level before EQ. Keep some headroom to reduce clipping when boosting bands.");
        eqGroup.Controls.Add(CreateText("Faders set the full tune  /  Intensity scales every band  /  Arrow keys = 1 dB steps", 9, Color.FromArgb(160, 176, 195)));
        var tuneButtons = new FlowLayoutPanel { Height = 56, Dock = DockStyle.Top, WrapContents = false };
        _undoButton.Text = "Undo change";
        StyleButton(_undoButton);
        _undoButton.Enabled = false;
        _undoButton.Click += (_, _) => UndoTune();
        _restoreButton.Text = "Restore saved tune";
        StyleButton(_restoreButton);
        _restoreButton.Enabled = false;
        _restoreButton.Click += (_, _) => RestoreSavedTune();
        tuneButtons.Controls.AddRange([_undoButton, _restoreButton]);
        _bypass.Text = "Bypass EQ";
        _bypass.AutoSize = true;
        _bypass.Padding = new Padding(12, 9, 0, 0);
        _tips.SetToolTip(_bypass, "Listen without EQ filters. Audio routing and preamp level stay unchanged.");
        _bypass.CheckedChanged += (_, _) => UpdateEqualizer();
        tuneButtons.Controls.Add(_bypass);
        eqGroup.Controls.Add(tuneButtons);
        page.Controls.Add(eqGroup, 0, 2);
        _status.Text = "Stopped";
        _status.AutoSize = true;
        _status.ForeColor = Color.FromArgb(85, 224, 187);
        _status.Margin = new Padding(0, 0, 0, 8);
        _status.MaximumSize = new Size(740, 0);
        page.Controls.Add(_status, 0, 3);
        var help = CreateCard(3);
        help.Controls.Add(CreateText("QUICK START & SAFE RETURN", 10, ForeColor));
        var instructions = CreateText(
            "1  Game output: CABLE Input     2  Virtual cable: CABLE Output     3  Send to a Wave Link channel or headphones\n\n" +
            "Return to normal: choose your original Wave Link channel or headphones in the game or Windows Volume mixer BEFORE stopping EqTune. " +
            "Bypass removes filters but keeps preamp and cable routing.\n\n" +
            "Start at a comfortable volume. This general tune is not calibrated to a specific headset.", 9, Color.FromArgb(160, 176, 195));
        instructions.MaximumSize = new Size(740, 0);
        help.Controls.Add(instructions);
        var waveLinkHelp = new Button { Text = "Wave Link 3 setup" };
        StyleButton(waveLinkHelp);
        waveLinkHelp.Click += (_, _) => MessageBox.Show(this,
            "1. In Wave Link 3, add a Virtual Channel and name it \"EqTune\". Note its Windows playback device name.\n\n" +
            "2. Route Call of Duty to CABLE Input (VB-CABLE), not directly to Wave Link.\n\n" +
            "3. In EqTune, select CABLE Output as the virtual cable. Under Send audio to, choose the playback device for the Wave Link virtual channel. Click Refresh devices if it is missing, then Start EQ.\n\n" +
            "4. In Wave Link, include that channel in your chosen mix and select your headphones/speakers as the mix output.\n\n" +
            "Avoid loops: never route a Wave Link mix back to CABLE Input. Do not also capture the raw game or CABLE Output in Wave Link, or you may hear doubled/unprocessed audio.\n\n" +
            "To revert: first send the game directly to your original Wave Link channel, then stop EqTune. EqTune does not change Wave Link settings automatically.",
            "Game -> VB-CABLE -> EqTune -> Wave Link 3 -> Headphones",
            MessageBoxButtons.OK, MessageBoxIcon.Information);
        help.Controls.Add(waveLinkHelp);
        page.Controls.Add(help, 0, 4);
        UpdateEqualizer();
    }

    private static TableLayoutPanel CreateCard(int rows)
    {
        var card = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            ColumnCount = 1,
            RowCount = rows,
            GrowStyle = TableLayoutPanelGrowStyle.AddRows,
            ColumnStyles = { new ColumnStyle(SizeType.Percent, 100) },
            BackColor = Color.FromArgb(21, 28, 38),
            Padding = new Padding(14),
            Margin = new Padding(0, 0, 0, 14)
        };
        for (var i = 0; i < rows; i++)
        {
            card.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        }
        return card;
    }

    private static Label CreateText(string text, float size, Color color) => new()
    {
        Text = text,
        AutoSize = true,
        Font = new Font("Segoe UI", size, size >= 11 ? FontStyle.Bold : FontStyle.Regular),
        ForeColor = color,
        Margin = new Padding(0, 0, 0, 8)
    };

    private static void StyleButton(Button button, bool primary = false)
    {
        button.AutoSize = true;
        button.FlatStyle = FlatStyle.Flat;
        button.FlatAppearance.BorderSize = 0;
        button.BackColor = primary ? Color.FromArgb(85, 224, 187) : Color.FromArgb(40, 52, 69);
        button.ForeColor = primary ? Color.FromArgb(13, 18, 26) : Color.FromArgb(232, 239, 248);
        button.Padding = new Padding(12, 6, 12, 6);
        button.Margin = new Padding(0, 8, 8, 0);
        button.Cursor = Cursors.Hand;
    }

    private static TrackBar CreateSlider(int minimum, int maximum, int value, string name)
    {
        var slider = new TrackBar { Minimum = minimum, Maximum = maximum, Value = value, TickFrequency = 3 };
        ConfigureVerticalSlider(slider, name);
        return slider;
    }

    private static void ConfigureVerticalSlider(TrackBar slider, string name)
    {
        slider.Orientation = Orientation.Vertical;
        slider.AutoSize = false;
        slider.Size = new Size(48, 110);
        slider.TickStyle = TickStyle.Both;
        slider.BackColor = Color.FromArgb(21, 28, 38);
        slider.AccessibleName = name;
        slider.Cursor = Cursors.Hand;
        slider.Anchor = AnchorStyles.None;
    }

    private static TableLayoutPanel CreateBandColumn(string name, string detail, TrackBar slider, Label value)
    {
        var column = new TableLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 4 };
        var title = CreateText(name, 10, Color.FromArgb(232, 239, 248));
        var subtitle = CreateText(detail, 9, Color.FromArgb(160, 176, 195));
        foreach (var control in new Control[] { title, subtitle, slider, value })
        {
            control.Anchor = AnchorStyles.None;
            column.Controls.Add(control);
        }
        return column;
    }

    private static Label CreateFieldLabel(string text) => new()
    {
        Text = text,
        AutoSize = true,
        Anchor = AnchorStyles.Left,
        Margin = new Padding(0, 8, 8, 8)
    };

    private static void ConfigureCombo(ComboBox combo)
    {
        combo.DropDownStyle = ComboBoxStyle.DropDownList;
        combo.Dock = DockStyle.Fill;
        combo.Margin = new Padding(0, 4, 0, 4);
        combo.BackColor = Color.FromArgb(40, 52, 69);
        combo.ForeColor = Color.FromArgb(232, 239, 248);
        combo.FlatStyle = FlatStyle.Flat;
        combo.DrawMode = DrawMode.OwnerDrawFixed;
        combo.ItemHeight = combo.Font.Height + 8;
        combo.DrawItem += (_, e) =>
        {
            var selected = (e.State & DrawItemState.Selected) != 0;
            using var background = new SolidBrush(selected
                ? Color.FromArgb(49, 82, 91)
                : combo.BackColor);
            e.Graphics.FillRectangle(background, e.Bounds);
            var text = e.Index >= 0
                ? combo.GetItemText(combo.Items[e.Index])
                : "Select a device...";
            TextRenderer.DrawText(e.Graphics, text, combo.Font, e.Bounds,
                combo.Enabled ? combo.ForeColor : Color.FromArgb(160, 176, 195),
                TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
            e.DrawFocusRectangle();
        };
    }

    private void RefreshDevices()
    {
        try
        {
            var selectedInputId = (_inputDevices.SelectedItem as DeviceOption)?.Id;
            var selectedOutputId = (_outputDevices.SelectedItem as DeviceOption)?.Id;
            _inputDevices.Items.Clear();
            _outputDevices.Items.Clear();

            foreach (var device in _deviceEnumerator.EnumerateAudioEndPoints(DataFlow.Capture, DeviceState.Active))
            {
                _inputDevices.Items.Add(new DeviceOption(device.ID, device.FriendlyName));
            }

            foreach (var device in _deviceEnumerator.EnumerateAudioEndPoints(DataFlow.Render, DeviceState.Active))
            {
                if (!device.FriendlyName.Contains("CABLE Input", StringComparison.OrdinalIgnoreCase))
                {
                    _outputDevices.Items.Add(new DeviceOption(device.ID, device.FriendlyName));
                }
            }

            var inputSelected = SelectDevice(_inputDevices, selectedInputId, "CABLE Output", selectFirst: false);
            SelectDevice(_outputDevices, selectedOutputId, null);
            if (!inputSelected)
            {
                _status.Text = "VB-CABLE input not found. Install the driver, then refresh devices.";
            }
            if (_inputDevices.Items.Count == 0 || _outputDevices.Items.Count == 0)
            {
                _status.Text = "No active audio devices found. Install/configure your virtual cable, then refresh.";
            }
        }
        catch (Exception ex)
        {
            _status.Text = "Could not enumerate audio devices.";
            MessageBox.Show(this, ex.Message, "Audio device error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private static bool SelectDevice(ComboBox combo, string? id, string? nameHint, bool selectFirst = true)
    {
        for (var i = 0; i < combo.Items.Count; i++)
        {
            var item = (DeviceOption)combo.Items[i]!;
            if (id is not null && item.Id == id)
            {
                combo.SelectedIndex = i;
                return true;
            }
        }

        if (nameHint is not null)
        {
            for (var i = 0; i < combo.Items.Count; i++)
            {
                var item = (DeviceOption)combo.Items[i]!;
                if (item.Name.Contains(nameHint, StringComparison.OrdinalIgnoreCase))
                {
                    combo.SelectedIndex = i;
                    return true;
                }
            }
        }

        if (selectFirst && combo.Items.Count > 0)
        {
            combo.SelectedIndex = 0;
            return true;
        }

        return false;
    }

    private void StartAudio()
    {
        if (_capture is not null)
        {
            return;
        }

        if (_inputDevices.SelectedItem is not DeviceOption input ||
            _outputDevices.SelectedItem is not DeviceOption output)
        {
            MessageBox.Show(this, "Select a virtual cable input and an output (Wave Link virtual channel or headphones).",
                "Choose audio devices", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }

        try
        {
            var inputDevice = _deviceEnumerator.GetDevice(input.Id);
            var outputDevice = _deviceEnumerator.GetDevice(output.Id);
            _capture = new WasapiCapture(inputDevice, true, 30);
            _buffer = new BufferedWaveProvider(_capture.WaveFormat)
            {
                BufferDuration = TimeSpan.FromSeconds(2),
                DiscardOnBufferOverflow = true,
                ReadFully = true
            };
            _equalizer = new EqSampleProvider(_buffer.ToSampleProvider(), ProcessingBands(), _preampSlider.Value);
            _output = new WasapiOut(outputDevice, AudioClientShareMode.Shared, true, 40);
            _output.Init(_equalizer);
            _capture.DataAvailable += OnDataAvailable;
            _capture.StartRecording();
            _output.Play();
            SetRunning(true);
            _status.Text = $"Running: {input.Name} -> {output.Name}";
        }
        catch (Exception ex)
        {
            StopAudio();
            _status.Text = "Could not start audio processing.";
            MessageBox.Show(this, ex.Message, "Audio startup error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void OnDataAvailable(object? sender, WaveInEventArgs e)
    {
        _buffer?.AddSamples(e.Buffer, 0, e.BytesRecorded);
    }

    private void OnFormClosing(object? sender, FormClosingEventArgs e)
    {
        if (_capture is not null &&
            MessageBox.Show(this,
                "Closing EqTune stops audio routed through it. First switch the game output back to your original Wave Link channel or headphones. Close anyway?",
                "Stop audio processing",
                MessageBoxButtons.YesNo,
                MessageBoxIcon.Warning) != DialogResult.Yes)
        {
            e.Cancel = true;
            return;
        }

        StopAudio();
    }

    private void StopAudio(bool confirm = false)
    {
        if (confirm && _capture is not null &&
            MessageBox.Show(this,
                "Stopping EqTune mutes audio routed through the virtual cable. First switch the game output back to your original Wave Link channel or headphones. Stop?",
                "Stop audio processing",
                MessageBoxButtons.YesNo,
                MessageBoxIcon.Warning) != DialogResult.Yes)
        {
            return;
        }

        try
        {
            if (_capture is not null)
            {
                _capture.DataAvailable -= OnDataAvailable;
                if (_capture.CaptureState != CaptureState.Stopped)
                {
                    _capture.StopRecording();
                }
                _capture.Dispose();
                _capture = null;
            }

            _output?.Stop();
            _output?.Dispose();
            _output = null;
            _buffer = null;
            _equalizer = null;
            SetRunning(false);
            if (!IsDisposed)
            {
                _status.Text = "Stopped";
            }
        }
        catch (Exception ex)
        {
            _status.Text = "Audio stopped with an error.";
            MessageBox.Show(this, ex.Message, "Audio shutdown error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void UpdateEqualizer()
    {
        _curve.SetBands(ProcessingBands());
        _modeLabel.Text = _bypass.Checked
            ? "BYPASSED  /  Preamp and audio routing are unchanged"
            : $"EQ {_intensitySlider.Value}%  /  Filter response in dB, before preamp";
        _intensitySlider.Enabled = !_bypass.Checked;
        _equalizer?.Update(ProcessingBands(), _preampSlider.Value);
    }

    private IReadOnlyList<EqBand> ProcessingBands() =>
        _bypass.Checked ? [] : CurrentBands()
            .Select(band => band with { GainDb = band.GainDb * _intensitySlider.Value / 100f })
            .ToArray();

    private int[] ReadTune() =>
        _bandSliders.Select(slider => slider.Value)
            .Append(_preampSlider.Value).Append(_intensitySlider.Value).ToArray();

    private void OnTuneChanged()
    {
        if (_applyingTune)
        {
            return;
        }

        var tune = ReadTune();
        if (_currentTune.Length > 0 && !tune.SequenceEqual(_currentTune))
        {
            _undoTunes.Push(_currentTune);
        }
        _currentTune = tune;
        UpdateTuneButtons();
        UpdateEqualizer();
    }

    private void ApplyTune(int[] tune)
    {
        _applyingTune = true;
        try
        {
            for (var i = 0; i < _bandSliders.Length; i++)
            {
                _bandSliders[i].Value = tune[i];
            }
            _preampSlider.Value = tune[_bandSliders.Length];
            _intensitySlider.Value = tune[^1];
        }
        finally
        {
            _applyingTune = false;
        }
        _currentTune = ReadTune();
        UpdateTuneButtons();
        UpdateEqualizer();
    }

    private void UndoTune()
    {
        if (_undoTunes.TryPop(out var tune))
        {
            ApplyTune(tune);
        }
    }

    private void RestoreSavedTune()
    {
        if (_savedTune.Length > 0 && !_currentTune.SequenceEqual(_savedTune))
        {
            _undoTunes.Push(_currentTune);
            ApplyTune(_savedTune);
        }
    }

    private void UpdateTuneButtons()
    {
        _undoButton.Enabled = _undoTunes.Count > 0;
        _restoreButton.Enabled = _savedTune.Length > 0 && !_currentTune.SequenceEqual(_savedTune);
    }

    private void OpenSoundSettings()
    {
        try
        {
            Process.Start(new ProcessStartInfo("ms-settings:sound") { UseShellExecute = true });
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, $"Could not open Windows Sound settings. Open Settings > System > Sound manually.\n{ex.Message}",
                "Sound settings error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private IReadOnlyList<EqBand> CurrentBands() => DefaultBands
        .Select((band, index) => band with { GainDb = _bandSliders[index].Value })
        .ToArray();

    private void SetRunning(bool running)
    {
        _startButton.Enabled = !running;
        _stopButton.Enabled = running;
        _inputDevices.Enabled = !running;
        _outputDevices.Enabled = !running;
    }

    private void LoadUserSettings()
    {
        if (!File.Exists(_settingsPath))
        {
            return;
        }

        try
        {
            _settings = JsonSerializer.Deserialize<AppSettings>(File.ReadAllText(_settingsPath)) ?? new AppSettings();
            for (var i = 0; i < _bandSliders.Length && i < _settings.BandGains.Length; i++)
            {
                _bandSliders[i].Value = Math.Clamp(_settings.BandGains[i], _bandSliders[i].Minimum, _bandSliders[i].Maximum);
            }

            _preampSlider.Value = Math.Clamp(_settings.Preamp, _preampSlider.Minimum, _preampSlider.Maximum);
            _intensitySlider.Value = Math.Clamp(_settings.Intensity, _intensitySlider.Minimum, _intensitySlider.Maximum);
            SelectDevice(_inputDevices, _settings.InputDeviceId, "CABLE Output", selectFirst: false);
            SelectDevice(_outputDevices, _settings.OutputDeviceId, null);
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, $"Your saved settings could not be loaded:\n{ex.Message}",
                "Settings error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private void SaveUserSettings()
    {
        try
        {
            _settings = new AppSettings
            {
                InputDeviceId = (_inputDevices.SelectedItem as DeviceOption)?.Id,
                OutputDeviceId = (_outputDevices.SelectedItem as DeviceOption)?.Id,
                BandGains = _bandSliders.Select(slider => slider.Value).ToArray(),
                Intensity = _intensitySlider.Value,
                Preamp = _preampSlider.Value
            };
            Directory.CreateDirectory(Path.GetDirectoryName(_settingsPath)!);
            File.WriteAllText(_settingsPath, JsonSerializer.Serialize(_settings, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, $"Your settings could not be saved:\n{ex.Message}",
                "Settings error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private sealed record DeviceOption(string Id, string Name)
    {
        public override string ToString() => Name;
    }

    private sealed class AppSettings
    {
        public string? InputDeviceId { get; set; }
        public string? OutputDeviceId { get; set; }
        public int[] BandGains { get; set; } = [-4, -2, -1, 2, 3];
        public int Preamp { get; set; } = -8;
        public int Intensity { get; set; } = 100;
    }
}
