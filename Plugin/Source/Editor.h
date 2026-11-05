#pragma once

#include "Dsp.h"
#include "public.sdk/source/common/pluginview.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include <windows.h>
#include <vector>

namespace EqTune {
class Editor final : public Steinberg::CPluginView {
public:
    explicit Editor(Steinberg::Vst::EditController* owner);
    ~Editor() override;
    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override;
    Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) override;
    Steinberg::tresult PLUGIN_API removed() override;

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT message, WPARAM wParam, LPARAM lParam);
    void paint(HDC dc);
    Values read() const;
    void apply(const Values& values);
    void update();
    void edit(int id, double value);
    Steinberg::Vst::EditController* controller;
    HWND window = nullptr;
    std::array<HWND, parameterCount> sliders {};
    HWND undoButton = nullptr, restoreButton = nullptr, bypassButton = nullptr, profileSelector = nullptr;
    HWND gameSetupButton = nullptr, graphicsSetupButton = nullptr, loudnessButton = nullptr, strongButton = nullptr;
    HFONT font = nullptr, headingFont = nullptr;
    HBRUSH background = nullptr;
    Values saved {}, shown {};
    std::vector<Values> history;
    int gesture = -1;
};
}
