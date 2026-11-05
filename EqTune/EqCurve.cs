using System.Drawing.Drawing2D;

namespace EqTune;

internal sealed class EqCurve : Control
{
    private IReadOnlyList<EqBand> _bands = [];

    public EqCurve()
    {
        DoubleBuffered = true;
        SetStyle(ControlStyles.ResizeRedraw, true);
        Height = 130;
        Dock = DockStyle.Top;
        AccessibleName = "Equalizer frequency response, excluding preamp";
        BackColor = Color.FromArgb(21, 28, 38);
        ForeColor = Color.FromArgb(85, 224, 187);
    }

    public void SetBands(IReadOnlyList<EqBand> bands)
    {
        _bands = bands;
        Invalidate();
    }

    internal static double GainAt(double frequency, IReadOnlyList<EqBand> bands)
    {
        const double sampleRate = 48000;
        var gain = 0d;
        var w = 2 * Math.PI * frequency / sampleRate;
        foreach (var band in bands)
        {
            var a = Math.Pow(10, band.GainDb / 40);
            var w0 = 2 * Math.PI * band.Frequency / sampleRate;
            var alpha = Math.Sin(w0) / (2 * band.Q);
            var b0 = 1 + alpha * a;
            var b1 = -2 * Math.Cos(w0);
            var b2 = 1 - alpha * a;
            var a0 = 1 + alpha / a;
            var a1 = b1;
            var a2 = 1 - alpha / a;
            var numerator = Math.Pow(b0 + b1 * Math.Cos(w) + b2 * Math.Cos(2 * w), 2)
                + Math.Pow(b1 * Math.Sin(w) + b2 * Math.Sin(2 * w), 2);
            var denominator = Math.Pow(a0 + a1 * Math.Cos(w) + a2 * Math.Cos(2 * w), 2)
                + Math.Pow(a1 * Math.Sin(w) + a2 * Math.Sin(2 * w), 2);
            gain += 10 * Math.Log10(numerator / denominator);
        }
        return gain;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        var plot = new RectangleF(42, 16, ClientSize.Width - 62, ClientSize.Height - 44);
        if (plot.Width <= 0 || plot.Height <= 0)
        {
            return;
        }

        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        using var grid = new Pen(Color.FromArgb(42, 53, 68));
        using var text = new SolidBrush(Color.FromArgb(160, 176, 195));
        using var line = new Pen(ForeColor, 2.5f);
        float X(double frequency) => plot.Left + (float)(Math.Log10(frequency / 20) / 3) * plot.Width;
        float Y(double db) => plot.Top + (float)((12 - Math.Clamp(db, -12, 12)) / 24) * plot.Height;
        foreach (var db in new[] { -12, -6, 0, 6, 12 })
        {
            e.Graphics.DrawLine(grid, plot.Left, Y(db), plot.Right, Y(db));
            e.Graphics.DrawString($"{db:+#;-#;0}", Font, text, 4, Y(db) - 7);
        }
        foreach (var hz in new[] { 20, 100, 1000, 10000, 20000 })
        {
            e.Graphics.DrawLine(grid, X(hz), plot.Top, X(hz), plot.Bottom);
            var label = hz >= 1000 ? $"{hz / 1000}k" : hz.ToString();
            var width = e.Graphics.MeasureString(label, Font).Width;
            e.Graphics.DrawString(label, Font, text, X(hz) - width / 2, plot.Bottom + 8);
        }
        var points = Enumerable.Range(0, 400)
            .Select(i =>
            {
                var frequency = 20 * Math.Pow(1000, i / 399d);
                return new PointF(X(frequency), Y(GainAt(frequency, _bands)));
            }).ToArray();
        e.Graphics.DrawLines(line, points);
    }
}
