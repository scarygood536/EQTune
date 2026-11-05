#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "base/source/fobject.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"
#include "../Source/Dsp.h"
#include "../Source/HeadphoneProfiles.h"
#include <windows.h>
#include <commctrl.h>
#include <iostream>
#include <stdexcept>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
void check(bool result, const char* message)
{
    if (!result) throw std::runtime_error(message);
}

void selectProfile(HWND editor, int selection)
{
    const auto selector = GetDlgItem(editor, 103);
    check(selector != nullptr, "Headphone selector missing");
    SendMessageW(selector, CB_SETCURSEL, selection, 0);
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(103, CBN_SELCHANGE), reinterpret_cast<LPARAM>(selector));
}

class Handler final : public FObject, public IComponentHandler {
public:
    int begins = 0, edits = 0, ends = 0;
    ParameterChanges pending;
    tresult PLUGIN_API beginEdit(ParamID) override { ++begins; return kResultOk; }
    tresult PLUGIN_API performEdit(ParamID id, ParamValue value) override
    {
        ++edits;
        int32 index = 0, point = 0;
        pending.addParameterData(id, index)->addPoint(0, value, point);
        return kResultOk;
    }
    tresult PLUGIN_API endEdit(ParamID) override { ++ends; return kResultOk; }
    tresult PLUGIN_API restartComponent(int32) override { return kResultOk; }
    OBJ_METHODS(Handler, FObject)
    DEFINE_INTERFACES
        DEF_INTERFACE(IComponentHandler)
    END_DEFINE_INTERFACES(FObject)
    REFCOUNT_METHODS(FObject)
};

void savePreview(HWND editor)
{
    wchar_t path[32768] {};
    if (!GetEnvironmentVariableW(L"EQTUNE_PLUGIN_SCREENSHOT", path, 32768)) return;
    RECT rect {};
    GetClientRect(editor, &rect);
    const auto dc = GetDC(editor);
    const auto memory = CreateCompatibleDC(dc);
    const auto bitmap = CreateCompatibleBitmap(dc, rect.right, rect.bottom);
    const auto prior = SelectObject(memory, bitmap);
    SendMessageW(editor, WM_PRINT, reinterpret_cast<WPARAM>(memory), PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    // WM_PRINT is not guaranteed for every native control; capture the displayed editor as well.
    BitBlt(memory, 0, 0, rect.right, rect.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(memory, prior);
    BITMAPINFO info {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = rect.right;
    info.bmiHeader.biHeight = -rect.bottom;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    std::vector<unsigned char> pixels(rect.right * rect.bottom * 4);
    check(GetDIBits(memory, bitmap, 0, rect.bottom, pixels.data(), &info, DIB_RGB_COLORS) != 0, "Could not capture editor");
    BITMAPFILEHEADER header {};
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + static_cast<DWORD>(pixels.size());
    const auto file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(file != INVALID_HANDLE_VALUE, "Could not write editor preview");
    DWORD written = 0;
    const auto wrote = WriteFile(file, &header, sizeof(header), &written, nullptr)
        && WriteFile(file, &info.bmiHeader, sizeof(info.bmiHeader), &written, nullptr)
        && WriteFile(file, pixels.data(), static_cast<DWORD>(pixels.size()), &written, nullptr);
    CloseHandle(file);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(editor, dc);
    check(wrote, "Could not save preview bytes");
}

void run(IPluginFactory* factory)
{
    PClassInfo info {};
    bool found = false;
    for (int32 i = 0; i < factory->countClasses(); ++i) {
        factory->getClassInfo(i, &info);
        if (std::strcmp(info.category, kVstAudioEffectClass) == 0) { found = true; break; }
    }
    check(found, "No VST3 audio effect exported");
    IComponent* rawComponent = nullptr;
    check(factory->createInstance(info.cid, IComponent::iid, reinterpret_cast<void**>(&rawComponent)) == kResultOk,
        "Cannot instantiate processor");
    auto component = owned(rawComponent);
    auto host = owned(new HostApplication);
    check(component->initialize(host) == kResultOk, "Cannot initialize processor");
    TUID controllerId {};
    check(component->getControllerClassId(controllerId) == kResultOk, "Missing controller ID");
    IEditController* rawController = nullptr;
    check(factory->createInstance(controllerId, IEditController::iid, reinterpret_cast<void**>(&rawController)) == kResultOk,
        "Cannot instantiate controller");
    auto controller = owned(rawController);
    check(controller->initialize(host) == kResultOk, "Cannot initialize controller");
    auto handler = owned(new Handler);
    controller->setComponentHandler(handler);
    check(controller->getParameterCount() == EqTune::parameterCount, "Unexpected parameter count");
    auto state = owned(new MemoryStream);
    check(component->getState(state) == kResultOk, "Cannot save state");
    state->seek(0, IBStream::kIBSeekSet, nullptr);
    check(controller->setComponentState(state) == kResultOk, "Cannot restore controller from processor");
    for (int id = 0; id < EqTune::parameterCount; ++id)
        check(std::abs(controller->getParamNormalized(id) - EqTune::defaults[id]) < 1e-8, "Defaults differ");

    auto view = owned(controller->createView(ViewType::kEditor));
    check(view != nullptr, "No custom editor");
    check(view->isPlatformTypeSupported(kPlatformTypeHWND) == kResultTrue, "Editor does not support Windows");
    const auto parent = CreateWindowExW(0, L"STATIC", L"EqTune plugin UI test",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 80, 60, 810, 830, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    check(parent != nullptr, "Could not create test host window");
    check(view->attached(parent, kPlatformTypeHWND) == kResultOk, "Could not attach plugin editor");
    const auto editor = FindWindowExW(parent, nullptr, L"EqTune.PluginEditor.020", nullptr);
    check(editor != nullptr, "Native editor window missing");
    check(GetDlgItem(editor, 104) != nullptr, "Game audio setup button missing");
    check(GetDlgItem(editor, 105) != nullptr, "Graphics setup button missing");
    const auto loudnessButton = GetDlgItem(editor, 106);
    const auto strongButton = GetDlgItem(editor, 107);
    check(strongButton && !IsWindowEnabled(strongButton), "Strong must be off and unavailable until loudness enabled");
    const auto loudnessSlider = GetDlgItem(editor, EqTune::loudnessIntensityId + 1);
    check(loudnessButton && loudnessSlider && !IsWindowEnabled(loudnessSlider), "Optional loudness controls/default off missing");
    SendMessageW(loudnessButton, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(106, BN_CLICKED), 0);
    check(controller->getParamNormalized(EqTune::loudnessEnabledId) == 1 && IsWindowEnabled(loudnessSlider),
        "Loudness enable did not reach host");
    SendMessageW(strongButton, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(107, BN_CLICKED), 0);
    check(controller->getParamNormalized(EqTune::loudnessStrongId) == 1, "Strong checkbox did not reach host");
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(100, BN_CLICKED), 0);
    check(controller->getParamNormalized(EqTune::loudnessStrongId) == 0, "Strong undo failed");
    SendMessageW(loudnessSlider, TBM_SETPOS, TRUE, 75);
    SendMessageW(editor, WM_HSCROLL, TB_THUMBTRACK, reinterpret_cast<LPARAM>(loudnessSlider));
    SendMessageW(editor, WM_HSCROLL, TB_ENDTRACK, reinterpret_cast<LPARAM>(loudnessSlider));
    check(controller->getParamNormalized(EqTune::loudnessIntensityId) == 0.75, "Loudness slider did not reach host");
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(100, BN_CLICKED), 0);
    check(controller->getParamNormalized(EqTune::loudnessIntensityId) == 0.5, "Loudness intensity undo failed");
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(100, BN_CLICKED), 0);
    check(controller->getParamNormalized(EqTune::loudnessEnabledId) == 0, "Loudness enable undo failed");
    auto intensity = GetDlgItem(editor, 7);
    check(intensity != nullptr, "Intensity control missing");
    SendMessageW(intensity, TBM_SETPOS, TRUE, 50);
    SendMessageW(editor, WM_HSCROLL, TB_THUMBTRACK, reinterpret_cast<LPARAM>(intensity));
    SendMessageW(editor, WM_HSCROLL, TB_ENDTRACK, reinterpret_cast<LPARAM>(intensity));
    check(controller->getParamNormalized(6) == 0.5, "Intensity did not reach host parameter");
    check(handler->edits > 0 && handler->begins == handler->ends, "Unbalanced host edit gestures");
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(100, BN_CLICKED), 0);
    check(controller->getParamNormalized(6) == 1, "Undo intensity failed");
    SendMessageW(intensity, TBM_SETPOS, TRUE, 25);
    SendMessageW(editor, WM_HSCROLL, TB_THUMBTRACK, reinterpret_cast<LPARAM>(intensity));
    SendMessageW(editor, WM_HSCROLL, TB_ENDTRACK, reinterpret_cast<LPARAM>(intensity));
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(101, BN_CLICKED), 0);
    check(controller->getParamNormalized(6) == 1, "Restore-on-open failed");
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(100, BN_CLICKED), 0);
    check(controller->getParamNormalized(6) == 0.25, "Undo restoration failed");
    SendMessageW(GetDlgItem(editor, 102), BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(102, BN_CLICKED), 0);
    check(controller->getParamNormalized(7) == 1 && !IsWindowEnabled(intensity), "Bypass UI failed");
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(100, BN_CLICKED), 0);
    check(controller->getParamNormalized(7) == 0 && IsWindowEnabled(intensity), "Undo bypass failed");
    const auto previousPreamp = controller->getParamNormalized(5);
    selectProfile(editor, 1);
    check(controller->getParamNormalized(6) == 0.25 && controller->getParamNormalized(7) == 0,
        "Headphone selection must preserve intensity and bypass");
    check(std::abs(EqTune::gainDb(controller->getParamNormalized(3)) - 4) < 1e-8,
        "Clarity preset presence gain is wrong");
    check(controller->getParamNormalized(5) <= previousPreamp, "Clarity preset must not raise preamp");
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(100, BN_CLICKED), 0);
    check(controller->getParamNormalized(6) == 0.25 && controller->getParamNormalized(5) == previousPreamp,
        "Clarity preset must be undoable");
    auto readController = [&] {
        EqTune::Values values {};
        for (int id = 0; id < EqTune::parameterCount; ++id) values[id] = controller->getParamNormalized(id);
        return values;
    };
    const auto beforeProfiles = readController();
    for (int profile = 1; profile <= 5; ++profile) {
        selectProfile(editor, profile);
        check(EqTune::matchingHeadphoneProfile(readController()) == profile,
            "Selector did not apply the requested profile gains");
        check(SendMessageW(GetDlgItem(editor, 103), CB_GETCURSEL, 0, 0) == profile,
            "Selector label does not match applied gains");
        SendMessageW(editor, WM_COMMAND, MAKEWPARAM(100, BN_CLICKED), 0);
        check(readController() == beforeProfiles, "Undo should restore every pre-profile value");
    }
    selectProfile(editor, 4);
    const auto iemValues = readController();
    const auto firstBand = GetDlgItem(editor, 1);
    SendMessageW(firstBand, TBM_SETPOS, TRUE, 100);
    SendMessageW(editor, WM_VSCROLL, TB_THUMBTRACK, reinterpret_cast<LPARAM>(firstBand));
    SendMessageW(editor, WM_VSCROLL, TB_ENDTRACK, reinterpret_cast<LPARAM>(firstBand));
    check(SendMessageW(GetDlgItem(editor, 103), CB_GETCURSEL, 0, 0) == 0,
        "Manual band edits should display Custom");
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(100, BN_CLICKED), 0);
    check(readController() == iemValues &&
        SendMessageW(GetDlgItem(editor, 103), CB_GETCURSEL, 0, 0) == 4,
        "Undoing a manual edit should restore the IEM label");
    selectProfile(editor, 0);
    check(readController() == iemValues, "Selecting Custom must not overwrite current gains");
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(101, BN_CLICKED), 0);
    check(SendMessageW(GetDlgItem(editor, 103), CB_GETCURSEL, 0, 0) == 0,
        "Restore should recompute the profile label for the opening tune");
    SendMessageW(editor, WM_COMMAND, MAKEWPARAM(100, BN_CLICKED), 0);
    selectProfile(editor, 4);
    for (const int id : {1, 2, 3, 4, 5, 6, 7, 10, 100, 101, 102, 103, 104, 105, 106, 107}) {
        RECT bounds {}, client {};
        GetWindowRect(GetDlgItem(editor, id), &bounds);
        MapWindowPoints(nullptr, editor, reinterpret_cast<POINT*>(&bounds), 2);
        GetClientRect(editor, &client);
        check(bounds.left >= 0 && bounds.top >= 0 && bounds.right <= client.right && bounds.bottom <= client.bottom,
            "Control is outside editor");
    }
    RECT setupBounds {}, bypassBounds {};
    GetWindowRect(GetDlgItem(editor, 104), &setupBounds);
    GetWindowRect(GetDlgItem(editor, 102), &bypassBounds);
    check(setupBounds.left > bypassBounds.right, "Game setup button overlaps bypass");
    RECT graphicsBounds {};
    GetWindowRect(GetDlgItem(editor, 105), &graphicsBounds);
    check(graphicsBounds.top > setupBounds.bottom, "Graphics setup overlaps audio setup");
    UpdateWindow(parent);
    RedrawWindow(editor, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    savePreview(editor);
    check(view->removed() == kResultOk, "Editor teardown failed");
    check(view->attached(parent, kPlatformTypeHWND) == kResultOk, "Editor re-open failed");
    const auto reopenedEditor = FindWindowExW(parent, nullptr, L"EqTune.PluginEditor.020", nullptr);
    check(SendMessageW(GetDlgItem(reopenedEditor, 103), CB_GETCURSEL, 0, 0) == 4,
        "Reopening should retain the profile inferred from saved gains");
    view->removed();
    DestroyWindow(parent);
    view = nullptr;

    FUnknownPtr<IAudioProcessor> processor(component);
    check(processor != nullptr, "Missing processor interface");
    ProcessSetup setup {};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample32;
    setup.maxSamplesPerBlock = 256;
    setup.sampleRate = 48000;
    check(processor->setupProcessing(setup) == kResultOk, "Cannot set up audio");
    component->setActive(true);
    ParameterChanges changes;
    for (int id = 0; id < EqTune::parameterCount; ++id) {
        int32 index = 0, point = 0;
        changes.addParameterData(id, index)->addPoint(0, controller->getParamNormalized(id), point);
    }
    ProcessData data {};
    data.symbolicSampleSize = kSample32;
    data.inputParameterChanges = &changes;
    check(processor->process(data) == kResultOk, "Host parameter flush failed");
    auto editedState = owned(new MemoryStream);
    check(component->getState(editedState) == kResultOk, "Saving edited state failed");
    controller->setParamNormalized(6, 0.9);
    editedState->seek(0, IBStream::kIBSeekSet, nullptr);
    check(component->setState(editedState) == kResultOk, "Processor state restoration failed");
    editedState->seek(0, IBStream::kIBSeekSet, nullptr);
    check(controller->setComponentState(editedState) == kResultOk &&
        controller->getParamNormalized(6) == 0.25, "Intensity state round-trip failed");
    check(EqTune::matchingHeadphoneProfile(readController()) == 4,
        "State round-trip must retain headphone profile gains");
    auto invalidState = owned(new MemoryStream);
    check(component->setState(invalidState) != kResultOk, "Truncated state was accepted");
    auto audioView = owned(controller->createView(ViewType::kEditor));
    const auto audioParent = CreateWindowExW(0, L"STATIC", L"EqTune audio test", WS_OVERLAPPEDWINDOW,
        0, 0, 810, 830, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    check(audioParent != nullptr && audioView->attached(audioParent, kPlatformTypeHWND) == kResultOk,
        "Could not open editor for UI-to-audio checks");
    const auto audioEditor = FindWindowExW(audioParent, nullptr, L"EqTune.PluginEditor.020", nullptr);
    selectProfile(audioEditor, 1);
    auto moveSlider = [&](int id, int position) {
        const auto slider = GetDlgItem(audioEditor, id + 1);
        SendMessageW(slider, TBM_SETPOS, TRUE, position);
        const auto message = id == 6 || id == EqTune::loudnessIntensityId ? WM_HSCROLL : WM_VSCROLL;
        SendMessageW(audioEditor, message, TB_THUMBTRACK, reinterpret_cast<LPARAM>(slider));
        SendMessageW(audioEditor, message, TB_ENDTRACK, reinterpret_cast<LPARAM>(slider));
    };
    auto measure = [&](double frequency, bool expectDry = false, double amplitude = 0.1) {
        std::array<float, 256> left {}, right {}, outputLeft {}, outputRight {};
        float* input[] {left.data(), right.data()};
        float* output[] {outputLeft.data(), outputRight.data()};
        AudioBusBuffers inputBus {}, outputBus {};
        inputBus.numChannels = outputBus.numChannels = 2;
        inputBus.channelBuffers32 = input;
        outputBus.channelBuffers32 = output;
        ProcessData audio {};
        audio.symbolicSampleSize = kSample32;
        audio.numInputs = audio.numOutputs = 1;
        audio.inputs = &inputBus;
        audio.outputs = &outputBus;
        audio.numSamples = 256;
        double energy = 0;
        int measuredSamples = 0;
        for (int block = 0; block < 400; ++block) {
            for (int i = 0; i < 256; ++i)
                left[i] = right[i] = static_cast<float>(amplitude * std::sin(
                    2 * 3.14159265358979323846 * frequency * (block * 256 + i) / 48000));
            audio.inputParameterChanges = block == 0 ? &handler->pending : nullptr;
            check(processor->process(audio) == kResultOk, "UI-driven audio processing failed");
            if (block == 0) handler->pending.clearQueue();
            if (block >= 300) {
                for (int i = 0; i < 256; ++i) {
                    check(std::abs(outputLeft[i] - outputRight[i]) < 1e-7,
                        "EQ changed the balance of identical stereo inputs");
                    if (expectDry) check(outputLeft[i] == left[i] && outputRight[i] == right[i],
                        "Host bypass must return exact unprocessed samples");
                    energy += outputLeft[i] * outputLeft[i];
                }
                measuredSamples += 256;
            }
        }
        return 20 * std::log10(std::sqrt(energy / measuredSamples));
    };
    moveSlider(6, 0);
    const auto flatBass = measure(120);
    const auto flatPresence = measure(2800);
    moveSlider(6, 100);
    const auto fullBass = measure(120);
    const auto fullPresence = measure(2800);
    check(fullBass - flatBass < -5 && fullPresence - flatPresence > 3,
        "Actual UI intensity must measurably change processed bass and presence");
    moveSlider(5, 0);
    const auto loud = measure(1000);
    moveSlider(5, 180);
    const auto quiet = measure(1000);
    check(std::abs((quiet - loud) + 18) < 0.05, "UI preamp must change actual audio by 18 dB");
    auto hostBypass = [&](double value) {
        int32 index = 0, point = 0;
        handler->pending.addParameterData(EqTune::bypassId, index)->addPoint(0, value, point);
    };
    hostBypass(1);
    const auto dry = measure(1000, true);
    check(dry - quiet > 15, "Host disable must remove the -18 dB preamp as well as EQ");
    hostBypass(0);
    const auto enabledAgain = measure(1000);
    check(std::abs(enabledAgain - quiet) < 0.02, "Host enable must restore the previous processed sound");
    const auto audioLoudness = GetDlgItem(audioEditor, 106);
    auto enableLoudness = [&](bool enabled) {
        SendMessageW(audioLoudness, BM_SETCHECK, enabled ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(audioEditor, WM_COMMAND, MAKEWPARAM(106, BN_CLICKED), 0);
    };
    moveSlider(6, 0);
    moveSlider(5, 0);
    const auto quietOriginal = measure(1000, false, 0.005);
    const auto peakOriginal = measure(1000, false, 0.5);
    enableLoudness(true);
    moveSlider(EqTune::loudnessIntensityId, 50);
    const auto quietHalf = measure(1000, false, 0.005);
    moveSlider(EqTune::loudnessIntensityId, 100);
    const auto quietLifted = measure(1000, false, 0.005);
    const auto peakControlled = measure(1000, false, 0.5);
    check(quietHalf - quietOriginal > 5.5 && quietHalf - quietOriginal < 6.5,
        "Actual loudness slider at 50% must lift quiet audio about 6 dB");
    check(quietLifted - quietOriginal > 11.5 && quietLifted - quietOriginal < 12.5,
        "Actual loudness slider at 100% must lift quiet audio about 12 dB");
    check(peakControlled - peakOriginal < -5, "Actual loudness stage must reduce loud audio");
    const auto audioStrong = GetDlgItem(audioEditor, 107);
    SendMessageW(audioStrong, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(audioEditor, WM_COMMAND, MAKEWPARAM(107, BN_CLICKED), 0);
    const auto quietStrong = measure(1000, false, 0.005);
    check(std::abs(quietStrong - quietOriginal - 18) < 0.1 &&
        quietStrong - quietLifted > 5.9, "Strong must measurably add 6 dB over standard quiet lift");
    moveSlider(EqTune::loudnessIntensityId, 50);
    check(std::abs(measure(1000, false, 0.005) - quietOriginal - 9) < 0.1,
        "Strong 50% slider must produce about 9 dB lift through actual host");
    moveSlider(EqTune::loudnessIntensityId, 0);
    check(std::abs(measure(1000, false, 0.005) - quietOriginal) < 0.02, "0% loudness must preserve original audio");
    moveSlider(EqTune::loudnessIntensityId, 75);
    measure(1000);
    selectProfile(audioEditor, 2);
    check(controller->getParamNormalized(EqTune::loudnessEnabledId) == 1 &&
        controller->getParamNormalized(EqTune::loudnessIntensityId) == 0.75 &&
        controller->getParamNormalized(EqTune::loudnessStrongId) == 1,
        "Headphone selection must preserve loudness controls");
    auto loudnessState = owned(new MemoryStream);
    measure(1000);
    check(component->getState(loudnessState) == kResultOk, "Cannot save loudness state");
    enableLoudness(false);
    loudnessState->seek(0, IBStream::kIBSeekSet, nullptr);
    check(component->setState(loudnessState) == kResultOk, "Cannot reload loudness processor state");
    loudnessState->seek(0, IBStream::kIBSeekSet, nullptr);
    check(controller->setComponentState(loudnessState) == kResultOk &&
        controller->getParamNormalized(EqTune::loudnessEnabledId) == 1 &&
        controller->getParamNormalized(EqTune::loudnessIntensityId) == 0.75 &&
        controller->getParamNormalized(EqTune::loudnessStrongId) == 1, "Loudness state round-trip failed");
    handler->pending.clearQueue();
    hostBypass(1);
    measure(1000, true);
    const auto priorValues = readController();
    auto legacyState = owned(new MemoryStream);
    IBStreamer legacyWriter(legacyState, kLittleEndian);
    check(legacyWriter.writeInt32(1), "Cannot create old preset fixture");
    for (int id = 0; id < 8; ++id) check(legacyWriter.writeDouble(priorValues[id]), "Cannot write old parameter");
    legacyState->seek(0, IBStream::kIBSeekSet, nullptr);
    check(component->setState(legacyState) == kResultOk, "Old processor presets must still load");
    legacyState->seek(0, IBStream::kIBSeekSet, nullptr);
    check(controller->setComponentState(legacyState) == kResultOk, "Old controller presets must still load");
    const auto migrated = readController();
    for (int id = 0; id < 8; ++id) check(migrated[id] == priorValues[id], "Migration changed an existing parameter");
    check(migrated[EqTune::loudnessEnabledId] == 0 && migrated[EqTune::loudnessIntensityId] == 0.5,
        "Old presets must load with loudness off and 50% stored intensity");
    check(migrated[EqTune::loudnessStrongId] == 0, "Old presets must not enable Strong");
    auto v2State = owned(new MemoryStream);
    IBStreamer v2Writer(v2State, kLittleEndian);
    v2Writer.writeInt32(2);
    for (int id = 0; id < 10; ++id) v2Writer.writeDouble(priorValues[id]);
    v2State->seek(0, IBStream::kIBSeekSet, nullptr);
    check(component->setState(v2State) == kResultOk, "Version-2 processor preset migration failed");
    v2State->seek(0, IBStream::kIBSeekSet, nullptr);
    check(controller->setComponentState(v2State) == kResultOk, "Version-2 controller migration failed");
    const auto migratedV2 = readController();
    for (int id = 0; id < 10; ++id) check(migratedV2[id] == priorValues[id], "Migration changed existing loudness settings");
    check(migratedV2[EqTune::loudnessStrongId] == 0, "Version-2 presets must retain standard loudness");
    auto truncatedV2 = owned(new MemoryStream);
    IBStreamer truncatedWriter(truncatedV2, kLittleEndian);
    truncatedWriter.writeInt32(2);
    for (int id = 0; id < 8; ++id) truncatedWriter.writeDouble(priorValues[id]);
    truncatedV2->seek(0, IBStream::kIBSeekSet, nullptr);
    check(component->setState(truncatedV2) != kResultOk, "Truncated new presets must be rejected");
    truncatedV2->seek(0, IBStream::kIBSeekSet, nullptr);
    check(controller->setComponentState(truncatedV2) != kResultOk && readController() == migratedV2,
        "Invalid preset must not partially change controller state");
    auto truncatedV3 = owned(new MemoryStream);
    IBStreamer v3Writer(truncatedV3, kLittleEndian);
    v3Writer.writeInt32(3);
    for (int id = 0; id < 10; ++id) v3Writer.writeDouble(priorValues[id]);
    truncatedV3->seek(0, IBStream::kIBSeekSet, nullptr);
    check(component->setState(truncatedV3) != kResultOk, "Truncated Strong preset accepted");
    audioView->removed();
    audioView = nullptr;
    DestroyWindow(audioParent);
    component->setActive(false);
    controller->setComponentHandler(nullptr);
    controller->terminate();
    component->terminate();
}
}

int wmain(int argc, wchar_t** argv)
{
    try {
        check(argc == 2, "Supply the plugin binary path");
        const auto module = LoadLibraryW(argv[1]);
        check(module != nullptr, "Cannot load actual plugin binary");
        const auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
        const auto exit = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
        const auto factoryFunction = reinterpret_cast<IPluginFactory* (*)()>(GetProcAddress(module, "GetPluginFactory"));
        check(init && exit && factoryFunction && init(), "Plugin exports/init failed");
        {
            auto factory = owned(factoryFunction());
            run(factory);
        }
        check(exit(), "Plugin deinit failed");
        FreeLibrary(module);
        std::cout << "PASS: real VST3, UI-to-audio intensity/preamp, stereo balance, preset/undo/restore/bypass, state and editor lifecycle.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
