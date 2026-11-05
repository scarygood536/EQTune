using NAudio.Dsp;
using NAudio.Wave;

namespace EqTune;

internal readonly record struct EqBand(string Name, float Frequency, float GainDb, float Q);

internal sealed class EqSampleProvider : ISampleProvider
{
    private readonly ISampleProvider _source;
    private readonly object _filterLock = new();
    private BiQuadFilter[][] _filters = [];
    private float _preamp = 1f;

    public EqSampleProvider(ISampleProvider source, IReadOnlyList<EqBand> bands, float preampDb)
    {
        _source = source;
        Update(bands, preampDb);
    }

    public WaveFormat WaveFormat => _source.WaveFormat;

    public void Update(IReadOnlyList<EqBand> bands, float preampDb)
    {
        lock (_filterLock)
        {
            _filters = new BiQuadFilter[WaveFormat.Channels][];
            for (var channel = 0; channel < WaveFormat.Channels; channel++)
            {
                _filters[channel] = bands
                    .Select(band => BiQuadFilter.PeakingEQ(
                        WaveFormat.SampleRate,
                        band.Frequency,
                        band.Q,
                        band.GainDb))
                    .ToArray();
            }

            _preamp = MathF.Pow(10f, preampDb / 20f);
        }
    }

    public int Read(float[] buffer, int offset, int count)
    {
        var samplesRead = _source.Read(buffer, offset, count);
        lock (_filterLock)
        {
            for (var i = 0; i < samplesRead; i++)
            {
                var sample = buffer[offset + i] * _preamp;
                var filters = _filters[i % WaveFormat.Channels];
                for (var band = 0; band < filters.Length; band++)
                {
                    sample = filters[band].Transform(sample);
                }

                buffer[offset + i] = Math.Clamp(sample, -1f, 1f);
            }
        }

        return samplesRead;
    }
}
