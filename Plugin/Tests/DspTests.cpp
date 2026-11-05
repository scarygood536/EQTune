#include "../Source/Dsp.h"
#include "../Source/HeadphoneProfiles.h"
#include <iostream>
#include <stdexcept>
#include <vector>

void check(bool result, const char* message)
{
    if (!result) throw std::runtime_error(message);
}

template <class Sample>
void testLoudness(double rate, int channels)
{
    const auto count = static_cast<int>(rate);
    auto values = EqTune::defaults;
    values[EqTune::intensityId] = 0;
    values[EqTune::preampId] = 1;
    values[EqTune::loudnessEnabledId] = 1;
    auto measure = [&](double amplitude, double intensity, bool enabled = true) {
        std::vector<Sample> left(count, static_cast<Sample>(amplitude));
        std::vector<Sample> right(count, static_cast<Sample>(amplitude * 0.25));
        std::vector<Sample> outLeft(count), outRight(count);
        Sample* input[] {left.data(), right.data()};
        Sample* output[] {outLeft.data(), outRight.data()};
        values[EqTune::loudnessIntensityId] = intensity;
        values[EqTune::loudnessEnabledId] = enabled ? 1 : 0;
        EqTune::Engine engine;
        engine.prepare(rate, values);
        engine.process(input, output, channels, count, values);
        for (int i = count / 2; i < count; ++i) {
            check(std::isfinite(outLeft[i]), "Loudness output is not finite");
            if (channels == 2)
                check(std::abs(outLeft[i] * 0.25 - outRight[i]) < 1e-7, "Loudness changed stereo level ratio");
        }
        return static_cast<double>(outLeft.back());
    };
    const auto quiet = measure(0.005, 0);
    const auto half = measure(0.005, 0.5);
    const auto full = measure(0.005, 1);
    check(std::abs(20 * std::log10(full / quiet) - 12) < 0.05, "Quiet loudness lift must reach 12 dB");
    check(std::abs(20 * std::log10(half / quiet) - 6) < 0.05, "Half loudness must give 6 dB quiet lift");
    check(std::abs(measure(0.005, 1, false) - quiet) < 1e-9, "Loudness off must preserve original processing");
    check(measure(0.5, 1) < measure(0.5, 0) * 0.55, "Loudness must reduce loud sustained audio by over 5 dB");
    check(measure(0, 1) == 0, "Loudness generated audio during silence");
    check(std::abs(measure(1e-5, 1) / 1e-5 - 1) < 1e-5, "Loudness must not lift very low noise");
    values[EqTune::preampId] = 0;
    check(std::abs(20 * std::log10(measure(0.005, 1) / full) + 18) < 0.05, "Loudness must preserve preamp attenuation");
    values[EqTune::preampId] = 1;
    values[EqTune::loudnessStrongId] = 1;
    const auto strong = measure(0.005, 1);
    check(std::abs(20 * std::log10(strong / quiet) - 18) < 0.05, "Strong quiet lift must reach 18 dB");
    check(std::abs(20 * std::log10(measure(0.005, 0.5) / quiet) - 9) < 0.05,
        "Strong half intensity must give 9 dB quiet lift");
    check(std::abs(measure(0.005, 0) - quiet) < 1e-9, "Strong at 0% must leave processing unchanged");
    check(std::abs(measure(0.005, 1, false) - quiet) < 1e-9, "Disabled loudness must ignore Strong");
    check(measure(0.5, 1) < measure(0.5, 0) * 0.5, "Strong must reduce loud sustained audio by over 6 dB");
    check(measure(0, 1) == 0 && std::abs(measure(1e-5, 1) / 1e-5 - 1) < 1e-5,
        "Strong must preserve silence and the low-noise floor");
    values[EqTune::preampId] = 0;
    check(std::abs(20 * std::log10(measure(0.005, 1) / strong) + 18) < 0.05,
        "Strong must preserve preamp attenuation");
    values[EqTune::preampId] = 1;
    values[EqTune::loudnessEnabledId] = 1;
    values[EqTune::loudnessIntensityId] = 1;
    std::vector<Sample> left(count, 0), right(count, 0);
    Sample* inPlace[] {left.data(), right.data()};
    left[count / 2] = static_cast<Sample>(2);
    right[count / 2] = static_cast<Sample>(0.5);
    EqTune::Engine engine;
    engine.prepare(rate, values);
    engine.process(inPlace, inPlace, channels, count, values);
    check(std::abs(left[count / 2]) <= std::pow(10.0, -1.0 / 20) + 1e-7, "Transient exceeded loudness sample ceiling");
    if (channels == 2)
        check(std::abs(left[count / 2] * 0.25 - right[count / 2]) < 1e-7, "Transient protection must be stereo linked");
    values[EqTune::bypassId] = 1;
    std::fill(left.begin(), left.end(), static_cast<Sample>(1.25));
    engine.process(inPlace, inPlace, channels, count, values);
    check(left.back() == static_cast<Sample>(1.25), "Full bypass must skip loudness and ceiling");
    values[EqTune::bypassId] = 0;
    values[EqTune::loudnessEnabledId] = 0;
    engine.process(inPlace, inPlace, channels, count, values);
    values[EqTune::loudnessEnabledId] = 1;
    values[EqTune::loudnessIntensityId] = 0.5;
    values[EqTune::loudnessStrongId] = 0;
    engine.process(inPlace, inPlace, channels, count, values);
    for (auto sample : left) check(std::isfinite(sample), "Live loudness toggle produced non-finite output");
}

template <class Sample>
void test(double rate, int channels)
{
    constexpr double pi = 3.14159265358979323846;
    const auto count = static_cast<int>(rate);
    std::vector<Sample> left(count), right(count), outLeft(count), outRight(count);
    Sample* input[] {left.data(), right.data()};
    Sample* output[] {outLeft.data(), outRight.data()};
    for (int i = 0; i < count; ++i) {
        left[i] = static_cast<Sample>(0.1 * std::sin(2 * pi * 1000 * i / rate));
        right[i] = 0;
    }
    for (const auto percent : {0.0, 0.5, 1.0}) {
        auto values = EqTune::defaults;
        values[EqTune::intensityId] = percent;
        EqTune::Engine engine;
        engine.prepare(rate, values);
        engine.process(input, output, channels, count, values);
        double energy = 0;
        for (int i = count / 2; i < count; ++i) {
            check(std::isfinite(outLeft[i]), "Non-finite audio output");
            energy += static_cast<double>(outLeft[i]) * outLeft[i];
            if (channels == 2) check(outRight[i] == 0, "Stereo crosstalk");
        }
        const auto measured = 20 * std::log10(std::sqrt(energy / (count - count / 2)) / (0.1 / std::sqrt(2.0)));
        const auto expected = EqTune::responseDb(values, 1000, rate) + EqTune::preampDb(values[5]);
        check(std::abs(measured - expected) < 0.02, "Audio differs from graph by more than 0.02 dB");
        if (percent == 0) check(std::abs(EqTune::responseDb(values, 1000, rate)) < 1e-8, "0% is not flat");
    }
    auto values = EqTune::defaults;
    values[7] = 1;
    EqTune::Engine engine;
    engine.prepare(rate, values);
    engine.process(input, output, channels, count, values);
    for (int i = 0; i < count; ++i)
        check(outLeft[i] == left[i], "Bypass must return exact dry audio, without preamp");
    left[0] = static_cast<Sample>(1.25);
    engine.process(input, output, channels, count, values);
    check(outLeft[0] == left[0], "Bypass must not clip host audio");
    left[0] = 0;
    values[7] = 0;
    engine.process(input, input, channels, count, values);
    for (auto sample : left) check(std::isfinite(sample) && std::abs(sample) <= 1, "In-place automation failed");
    values[7] = 1;
    engine.process(input, output, channels, count, values);
    for (int i = static_cast<int>(rate * 0.03); i < count; ++i)
        check(outLeft[i] == left[i], "Live bypass must settle to exact dry audio");
}

int main()
{
    try {
        for (int profile = 0; profile < static_cast<int>(EqTune::headphoneProfiles.size()); ++profile) {
            auto prior = EqTune::defaults;
            prior[EqTune::preampId] = 0.1;
            prior[EqTune::intensityId] = 0.35;
            prior[EqTune::bypassId] = 1;
            const auto values = EqTune::applyHeadphoneProfile(prior, EqTune::headphoneProfiles[profile]);
            check(values[5] == prior[5] && values[6] == prior[6] && values[7] == prior[7],
                "Profiles must preserve lower preamp, intensity, and bypass");
            check(EqTune::matchingHeadphoneProfile(values) == profile + 1, "Profile matching failed");
            auto highPreamp = prior;
            highPreamp[5] = 1;
            check(EqTune::preampDb(EqTune::applyHeadphoneProfile(highPreamp,
                EqTune::headphoneProfiles[profile])[5]) <= -8, "Profile must provide at least 8 dB headroom");
            auto active = values;
            active[7] = 0;
            active[6] = 1;
            double peak = -100;
            for (int i = 0; i <= 1000; ++i) {
                const auto db = EqTune::responseDb(active, 20 * std::pow(1000.0, i / 1000.0));
                check(std::isfinite(db), "Profile response must be finite");
                peak = std::max(peak, db);
            }
            check(peak < 8, "Profile steady-state response must fit the preset headroom");
            active[6] = 0;
            check(std::abs(EqTune::responseDb(active, 2800)) < 1e-8, "Every profile must become flat at 0%");
        }
        for (const auto rate : {44100.0, 48000.0, 96000.0})
            for (const auto channels : {1, 2}) {
                test<float>(rate, channels);
                test<double>(rate, channels);
                testLoudness<float>(rate, channels);
                testLoudness<double>(rate, channels);
            }
        std::cout << "PASS: mono/stereo, float/double, 44.1/48/96 kHz, intensity 0/50/100%, graph accuracy, bypass, in-place changes.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
