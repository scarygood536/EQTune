#pragma once

#include "Dsp.h"

namespace EqTune {
struct HeadphoneProfile {
    const wchar_t* name;
    std::array<double, 5> gains;
};

inline constexpr std::array<HeadphoneProfile, 5> headphoneProfiles {{
    {L"DT 990 PRO X - starting preset", {-6, -3, 1, 4, 1}},
    {L"Open-back - generic", {-4, -2, 0, 2, 1}},
    {L"Closed-back - generic", {-5, -3, 0, 3, 1}},
    {L"IEM - generic", {-2, -1, 0, 1, 0}},
    {L"Gaming headset - generic", {-4, -3, -1, 3, 2}}
}};

inline Values applyHeadphoneProfile(Values values, const HeadphoneProfile& profile)
{
    for (int i = 0; i < 5; ++i) values[i] = (profile.gains[i] + 6) / 12;
    values[preampId] = std::min(values[preampId], 10.0 / 18);
    return values;
}

inline int matchingHeadphoneProfile(const Values& values)
{
    for (int profile = 0; profile < static_cast<int>(headphoneProfiles.size()); ++profile) {
        bool matches = true;
        for (int band = 0; band < 5; ++band)
            matches = matches && std::abs(gainDb(values[band]) - headphoneProfiles[profile].gains[band]) < 1e-6;
        if (matches) return profile + 1;
    }
    return 0;
}
}
