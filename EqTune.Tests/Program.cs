using System.Reflection;
using System.Text.Json;
using NAudio.Wave;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        var testDirectory = Path.Combine(Path.GetTempPath(), $"EqTune-revert-test-{Guid.NewGuid():N}");
        Directory.CreateDirectory(testDirectory);
        try
        {
            RunTests(Path.Combine(testDirectory, "settings.json"));
            RunTests(Path.Combine(testDirectory, "first-launch.json"), hasSavedSettings: false);
            RunTests(Path.Combine(testDirectory, "saved-intensity.json"), savedIntensity: 35);
            Console.WriteLine("PASS: intensity 0/50/100%, undo, restore, bypass, persistence, response graph accuracy, and dashboard layout.");
        }
        finally
        {
            Directory.Delete(testDirectory, recursive: true);
        }
    }

    private static void RunTests(string settingsPath, bool hasSavedSettings = true, int savedIntensity = 100)
    {
        var assembly = Assembly.Load("EqTune");
        var formType = assembly.GetType("EqTune.MainForm", throwOnError: true)!;
        using var form = (Form)Activator.CreateInstance(formType, nonPublic: true)!;
        FieldInfo Field(string name) => formType.GetField(name, BindingFlags.Instance | BindingFlags.NonPublic)!;
        T Get<T>(string name) => (T)Field(name).GetValue(form)!;
        object? Invoke(string name, params object[] args) =>
            formType.GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(form, args);
        Field("_settingsPath").SetValue(form, settingsPath);
        var baseline = hasSavedSettings ? new[] { -3, -2, -1, 1, 2, -9 } : [-4, -2, -1, 2, 3, -8];
        if (hasSavedSettings)
        {
            File.WriteAllText(settingsPath, savedIntensity == 100
                ? """{"BandGains":[-3,-2,-1,1,2],"Preamp":-9}"""
                : JsonSerializer.Serialize(new { BandGains = baseline[..5], Preamp = baseline[^1], Intensity = savedIntensity }));
        }
        typeof(Form).GetMethod("OnLoad", BindingFlags.Instance | BindingFlags.NonPublic)!
            .Invoke(form, [EventArgs.Empty]);
        var sliders = Get<TrackBar[]>("_bandSliders");
        var preamp = Get<TrackBar>("_preampSlider");
        var intensity = Get<TrackBar>("_intensitySlider");
        var undo = Get<Button>("_undoButton");
        var restore = Get<Button>("_restoreButton");
        int[] Tune() => sliders.Select(slider => slider.Value).Append(preamp.Value).ToArray();

        Check(Tune().SequenceEqual(baseline), "Legacy saved settings should load.");
        Check(intensity.Value == savedIntensity, "Intensity should load, defaulting to 100 for legacy/first launch.");
        Check(!undo.Enabled && !restore.Enabled, "Loading should not create undo history.");
        sliders[0].Value = -5;
        preamp.Value = -12;
        Check(undo.Enabled && restore.Enabled, "Edits should enable undo and restore.");
        Invoke("UndoTune");
        Check(preamp.Value == baseline[^1] && sliders[0].Value == -5, "Undo should revert only the last edit.");
        Invoke("UndoTune");
        Check(Tune().SequenceEqual(baseline) && !undo.Enabled && !restore.Enabled, "Repeated undo should restore baseline.");

        sliders[2].Value = 4;
        preamp.Value = -14;
        var editedIntensity = intensity.Value;
        var edited = Tune();
        Invoke("RestoreSavedTune");
        Check(Tune().SequenceEqual(baseline), "Restore should revert every EQ value and preamp.");
        Invoke("UndoTune");
        Check(Tune().SequenceEqual(edited), "Restoration should itself be undoable.");
        intensity.Value = editedIntensity == 35 ? 45 : 35;
        Invoke("UndoTune");
        Check(intensity.Value == editedIntensity, "Intensity changes should be undoable.");
        intensity.Value = 25;
        Invoke("RestoreSavedTune");
        Check(intensity.Value == savedIntensity && Tune().SequenceEqual(baseline),
            "Restore should include saved intensity and every fader.");
        Invoke("UndoTune");
        Check(intensity.Value == 25 && Tune().SequenceEqual(edited),
            "Undoing restore should bring back intensity and faders together.");
        intensity.Value = 100;

        var bypass = Get<CheckBox>("_bypass");
        bypass.Checked = true;
        Check(Tune().SequenceEqual(edited), "Bypass must not erase the tune.");
        var processingBands = Invoke("ProcessingBands")!;
        var providerType = assembly.GetType("EqTune.EqSampleProvider", throwOnError: true)!;
        var provider = (ISampleProvider)Activator.CreateInstance(providerType,
            [new ConstantSource(), processingBands, (float)preamp.Value])!;
        var samples = new float[128];
        Check(provider.Read(samples, 0, samples.Length) == samples.Length, "Bypass should keep producing audio.");
        var expected = 0.25f * MathF.Pow(10f, preamp.Value / 20f);
        Check(samples.All(sample => MathF.Abs(sample - expected) < 0.000001f),
            "Bypass must remove filters while preserving preamp attenuation.");
        Check(!intensity.Enabled, "Bypass should disable the intensity control.");
        bypass.Checked = false;
        Check(Tune().SequenceEqual(edited), "Leaving bypass should retain all slider values.");
        Check(((System.Collections.IEnumerable)Invoke("ProcessingBands")!).Cast<object>().Count() == 5,
            "Leaving bypass should restore all five filters.");
        var curveType = assembly.GetType("EqTune.EqCurve", throwOnError: true)!;
        var gainAt = curveType.GetMethod("GainAt", BindingFlags.Static | BindingFlags.NonPublic)!;
        foreach (var percent in new[] { 0, 50, 100 })
        {
            intensity.Value = percent;
            var activeBands = Invoke("ProcessingBands")!;
            var bands = ((System.Collections.IEnumerable)activeBands).Cast<object>().ToArray();
            for (var i = 0; i < bands.Length; i++)
            {
                var gain = (float)bands[i].GetType().GetProperty("GainDb")!.GetValue(bands[i])!;
                Check(Math.Abs(gain - sliders[i].Value * percent / 100f) < 0.00001f,
                    "Intensity must scale every band's gain, including cuts.");
            }
            Check(preamp.Value == -14 && Tune().SequenceEqual(edited),
                "Intensity must not move full-tune faders or preamp.");
            var predictedDb = (double)gainAt.Invoke(null, [1000d, activeBands])!;
            var eqProvider = (ISampleProvider)Activator.CreateInstance(providerType,
                [new SineSource(), activeBands, (float)preamp.Value])!;
            var tone = new float[48000];
            eqProvider.Read(tone, 0, tone.Length);
            var rms = Math.Sqrt(tone.Skip(24000).Average(sample => (double)sample * sample));
            var measuredDb = 20 * Math.Log10(rms / (0.25 / Math.Sqrt(2)));
            Check(Math.Abs(predictedDb + preamp.Value - measuredDb) < 0.02,
                "The graphed response plus preamp should match audio within 0.02 dB at each intensity.");
            if (percent == 0)
            {
                Check(Math.Abs(predictedDb) < 0.00001, "Zero intensity must produce flat EQ.");
            }
            var curveBands = Get<Control>("_curve").GetType()
                .GetField("_bands", BindingFlags.Instance | BindingFlags.NonPublic)!
                .GetValue(Get<Control>("_curve"))!;
            Check(Math.Abs((double)gainAt.Invoke(null, [1000d, curveBands])! - predictedDb) < 0.00001,
                "The displayed graph should track the intensity-scaled filters.");
        }
        intensity.Value = 50;

        Invoke("SaveUserSettings");
        using var saved = JsonDocument.Parse(File.ReadAllText(settingsPath));
        Check(saved.RootElement.GetProperty("Preamp").GetInt32() == -14, "The selected preamp should persist.");
        Check(saved.RootElement.GetProperty("BandGains")[2].GetInt32() == 4, "The selected tune should persist.");
        Check(saved.RootElement.GetProperty("Intensity").GetInt32() == 50, "Intensity should persist.");
        intensity.Value = 10;
        Field("_applyingTune").SetValue(form, true);
        try
        {
            Invoke("LoadUserSettings");
        }
        finally
        {
            Field("_applyingTune").SetValue(form, false);
        }
        Check(intensity.Value == 50, "Saved intensity should reload.");
        Invoke("UpdateEqualizer");

        form.Show();
        Application.DoEvents();
        var curve = Get<Control>("_curve");
        Check(curve.Width > 600 && curve.Height >= 120, "The response graph should fill the dashboard.");
        Check(sliders.All(slider => slider.Orientation == Orientation.Vertical && slider.Height >= 100),
            "EQ faders should be vertical and comfortably sized.");
        Check(!string.IsNullOrEmpty(preamp.AccessibleName), "The preamp should have an accessible label.");
        var screenshotPath = Environment.GetEnvironmentVariable("EQTUNE_SCREENSHOT_PATH");
        if (hasSavedSettings && !string.IsNullOrEmpty(screenshotPath))
        {
            using var bitmap = new Bitmap(form.Width, form.Height);
            form.DrawToBitmap(bitmap, new Rectangle(Point.Empty, form.Size));
            bitmap.Save(screenshotPath);
        }
        form.Size = form.MinimumSize;
        Application.DoEvents();
        Check(curve.Width > 600, "The graph should remain usable at the minimum window width.");
        Check(sliders.Append(preamp).All(slider =>
            slider.Parent!.ClientRectangle.Contains(slider.Bounds)),
            "All faders should fit inside their columns at minimum size.");
        form.Hide();
    }

    private static void Check(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidOperationException(message);
        }
    }

    private sealed class ConstantSource : ISampleProvider
    {
        public WaveFormat WaveFormat { get; } = WaveFormat.CreateIeeeFloatWaveFormat(48000, 2);

        public int Read(float[] buffer, int offset, int count)
        {
            Array.Fill(buffer, 0.25f, offset, count);
            return count;
        }

    }

    private sealed class SineSource : ISampleProvider
    {
        private int _position;
        public WaveFormat WaveFormat { get; } = WaveFormat.CreateIeeeFloatWaveFormat(48000, 1);

        public int Read(float[] buffer, int offset, int count)
        {
            for (var i = 0; i < count; i++)
            {
                buffer[offset + i] = 0.25f * MathF.Sin(2 * MathF.PI * 1000 * _position++ / 48000f);
            }
            return count;
        }
    }
}
