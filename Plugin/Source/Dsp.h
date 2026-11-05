#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace EqTune {
constexpr int parameterCount = 11;
constexpr int preampId = 5;
constexpr int intensityId = 6;
constexpr int bypassId = 7;
constexpr int loudnessEnabledId = 8;
constexpr int loudnessIntensityId = 9;
constexpr int loudnessStrongId = 10;
constexpr std::array<double, 5> frequencies {120, 350, 900, 2800, 5000};
constexpr std::array<double, parameterCount> defaults {
    2.0 / 12, 4.0 / 12, 5.0 / 12, 8.0 / 12, 9.0 / 12, 10.0 / 18, 1, 0, 0, 0.5, 0
};
using Values = std::array<double, parameterCount>;

inline double gainDb(double normalized) { return normalized * 12 - 6; }
inline double preampDb(double normalized) { return normalized * 18 - 18; }
inline bool validValue(double value) { return std::isfinite(value) && value >= 0 && value <= 1; }

struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void configure(double sampleRate, double frequency, double db)
    {
        constexpr double pi = 3.14159265358979323846;
        const auto a = std::pow(10.0, db / 40);
        const auto w = 2 * pi * std::min(frequency, sampleRate * 0.45) / sampleRate;
        const auto alpha = std::sin(w) / (2 * 0.8);
        const auto a0 = 1 + alpha / a;
        b0 = (1 + alpha * a) / a0;
        b1 = -2 * std::cos(w) / a0;
        b2 = (1 - alpha * a) / a0;
        a1 = b1;
        a2 = (1 - alpha / a) / a0;
    }

    double process(double input)
    {
        const auto output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        if (std::abs(z1) < 1e-25) z1 = 0;
        if (std::abs(z2) < 1e-25) z2 = 0;
        return output;
    }

    double response(double frequency, double sampleRate) const
    {
        constexpr double pi = 3.14159265358979323846;
        const auto z = std::polar(1.0, -2 * pi * frequency / sampleRate);
        return std::abs((b0 + b1 * z + b2 * z * z) / (1.0 + a1 * z + a2 * z * z));
    }
};

inline double responseDb(const Values& values, double frequency, double sampleRate = 48000)
{
    const auto intensity = values[bypassId] >= 0.5 ? 0 : values[intensityId];
    double result = 0;
    for (int band = 0; band < 5; ++band) {
        Biquad filter;
        filter.configure(sampleRate, frequencies[band], gainDb(values[band]) * intensity);
        result += 20 * std::log10(filter.response(frequency, sampleRate));
    }
    return result;
}

class Engine {
public:
    void prepare(double rate, const Values& values)
    {
        sampleRate = rate;
        filters = {};
        current = targets(values);
        wetMix = values[bypassId] >= 0.5 ? 0 : 1;
        tick = 0;
        envelope = 0;
        loudnessAmount = values[loudnessEnabledId] >= 0.5 ? values[loudnessIntensityId] : 0;
        strongMix = values[loudnessStrongId] >= 0.5 ? 1 : 0;
        attack = std::exp(-1.0 / (0.005 * sampleRate));
        release = std::exp(-1.0 / (0.2 * sampleRate));
        configure();
    }

    template <class Sample>
    void process(Sample** input, Sample** output, int channels, int count, const Values& values)
    {
        const auto target = targets(values);
        const auto smoothing = 1 - std::exp(-32.0 / (0.02 * sampleRate));
        const auto targetMix = values[bypassId] >= 0.5 ? 0.0 : 1.0;
        const auto mixStep = 1.0 / (0.02 * sampleRate);
        const auto targetLoudness = values[loudnessEnabledId] >= 0.5 ? values[loudnessIntensityId] : 0;
        const auto targetStrong = values[loudnessStrongId] >= 0.5 ? 1.0 : 0.0;
        for (int frame = 0; frame < count; ++frame) {
            wetMix += std::clamp(targetMix - wetMix, -mixStep, mixStep);
            loudnessAmount += std::clamp(targetLoudness - loudnessAmount, -mixStep, mixStep);
            strongMix += std::clamp(targetStrong - strongMix, -mixStep, mixStep);
            if (tick == 0) {
                for (int i = 0; i < 6; ++i) current[i] += (target[i] - current[i]) * smoothing;
                configure();
            }
            tick = (tick + 1) % 32;
            std::array<double, 2> samples {};
            double peak = 0;
            for (int channel = 0; channel < channels; ++channel) {
                double sample = input[channel][frame] * level;
                for (auto& filter : filters[channel]) sample = filter.process(sample);
                samples[channel] = sample;
                peak = std::max(peak, std::abs(sample));
            }
            // Detect before preamp so compression does not undo the user's headroom choice.
            const auto detected = peak / level;
            const auto coefficient = detected > envelope ? attack : release;
            envelope = coefficient * envelope + (1 - coefficient) * detected;
            if (envelope < 1e-25) envelope = 0;
            double dynamicsGain = 1;
            if (loudnessAmount > 0) {
                const auto db = 20 * std::log10(std::max(envelope, 1e-12));
                const auto noiseFade = std::clamp((db + 70) / 15, 0.0, 1.0);
                const auto standardDb = 12 * noiseFade - std::max(0.0, db + 30) * 0.75;
                const auto strongDb = 18 * noiseFade - std::max(0.0, db + 36) * (5.0 / 6);
                const auto adjustment = standardDb + strongMix * (strongDb - standardDb);
                dynamicsGain = std::pow(10.0, loudnessAmount * adjustment / 20);
                const auto ceiling = 1 - loudnessAmount * (1 - std::pow(10.0, -1.0 / 20));
                if (peak * dynamicsGain > ceiling) dynamicsGain = ceiling / peak;
            }
            for (int channel = 0; channel < channels; ++channel) {
                const double dry = input[channel][frame];
                const auto wet = std::clamp(samples[channel] * dynamicsGain, -1.0, 1.0);
                output[channel][frame] = wetMix == 0 ? input[channel][frame]
                    : static_cast<Sample>(dry + wetMix * (wet - dry));
            }
        }
    }

private:
    static std::array<double, 6> targets(const Values& values)
    {
        std::array<double, 6> result {};
        const auto intensity = values[intensityId];
        for (int i = 0; i < 5; ++i) result[i] = gainDb(values[i]) * intensity;
        result[5] = preampDb(values[preampId]);
        return result;
    }
    void configure()
    {
        level = std::pow(10.0, current[5] / 20);
        for (auto& channel : filters)
            for (int i = 0; i < 5; ++i) channel[i].configure(sampleRate, frequencies[i], current[i]);
    }
    double sampleRate = 48000;
    double level = 1;
    double wetMix = 1;
    double envelope = 0, loudnessAmount = 0, attack = 0, release = 0;
    double strongMix = 0;
    int tick = 0;
    std::array<double, 6> current {};
    std::array<std::array<Biquad, 5>, 2> filters {};
};
}
