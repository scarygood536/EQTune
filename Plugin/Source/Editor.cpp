#include "Editor.h"
#include "HeadphoneProfiles.h"
#include <commctrl.h>
#include <windowsx.h>
#include <cstdio>
#include <cstring>
#include <filesystem>

extern HINSTANCE ghInst;

namespace EqTune {
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
constexpr COLORREF bg = RGB(13, 18, 26), card = RGB(21, 28, 38);
constexpr COLORREF ink = RGB(232, 239, 248), muted = RGB(160, 176, 195), accent = RGB(85, 224, 187);
constexpr int width = 780, height = 780;
constexpr int undoId = 100, restoreId = 101, bypassButtonId = 102, profileId = 103;
constexpr int gameSetupId = 104, graphicsSetupId = 105;
constexpr int loudnessButtonId = 106;
constexpr int strongButtonId = 107;

bool horizontalSlider(int id) { return id == intensityId || id == loudnessIntensityId; }

void text(HDC dc, HFONT font, int x, int y, const wchar_t* value, COLORREF color)
{
    const auto old = SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    TextOutW(dc, x, y, value, static_cast<int>(wcslen(value)));
    SelectObject(dc, old);
}
void fill(HDC dc, RECT bounds, COLORREF color)
{
    const auto brush = CreateSolidBrush(color);
    FillRect(dc, &bounds, brush);
    DeleteObject(brush);
}
}

Editor::Editor(EditController* owner) : controller(owner)
{
    controller->addRef();
    rect = ViewRect(0, 0, width, height);
}

Editor::~Editor()
{
    removed();
    controller->release();
}

tresult PLUGIN_API Editor::isPlatformTypeSupported(FIDString type)
{
    return type && std::strcmp(type, kPlatformTypeHWND) == 0 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API Editor::attached(void* parent, FIDString type)
{
    if (!parent || window || isPlatformTypeSupported(type) != kResultTrue) return kResultFalse;
    INITCOMMONCONTROLSEX common {sizeof(common), ICC_BAR_CLASSES};
    if (!InitCommonControlsEx(&common)) return kResultFalse;
    WNDCLASSEXW klass {};
    klass.cbSize = sizeof(klass);
    klass.lpfnWndProc = windowProc;
    klass.hInstance = ghInst;
    klass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    klass.lpszClassName = L"EqTune.PluginEditor.020";
    if (!RegisterClassExW(&klass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return kResultFalse;
    background = CreateSolidBrush(card);
    font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    headingFont = CreateFontW(-28, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    saved = shown = read();
    window = CreateWindowExW(0, klass.lpszClassName, L"EqTune", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, width, height, static_cast<HWND>(parent), nullptr, ghInst, this);
    if (!window) {
        removed();
        return kResultFalse;
    }
    for (int id = 0; id < parameterCount; ++id) {
        if (id == bypassId || id == loudnessEnabledId || id == loudnessStrongId) continue;
        const auto intensity = horizontalSlider(id);
        const auto loudness = id == loudnessIntensityId;
        const auto style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | (intensity ? TBS_HORZ : TBS_VERT | TBS_BOTH);
        sliders[id] = CreateWindowExW(0, TRACKBAR_CLASSW,
            loudness ? L"Loudness intensity percent" : (intensity ? L"EQ intensity percent" : (id == preampId ? L"Preamp dB" : L"EQ band gain dB")),
            style, loudness ? 240 : (intensity ? 150 : 66 + id * 112), loudness ? 550 : (intensity ? 120 : 368),
            loudness ? 410 : (intensity ? 500 : 50), intensity ? 36 : 138,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id + 1)), ghInst, nullptr);
        if (!sliders[id]) { removed(); return kResultFalse; }
        SendMessageW(sliders[id], TBM_SETRANGE, TRUE, MAKELPARAM(0, intensity ? 100 : (id == preampId ? 180 : 120)));
        SendMessageW(sliders[id], TBM_SETTICFREQ, intensity ? 25 : 30, 0);
        SendMessageW(sliders[id], TBM_SETPAGESIZE, 0, intensity ? 10 : 10);
    }
    auto button = [&](int id, const wchar_t* title, int x, int buttonWidth, bool check = false) {
        auto handle = CreateWindowExW(0, L"BUTTON", title, WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            (check ? BS_AUTOCHECKBOX : BS_OWNERDRAW), x, 634, buttonWidth, 36,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), ghInst, nullptr);
        SendMessageW(handle, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return handle;
    };
    undoButton = button(undoId, L"Undo change", 40, 155);
    restoreButton = button(restoreId, L"Restore on-open tune", 205, 210);
    bypassButton = button(bypassButtonId, L"Bypass EQ", 445, 190, true);
    profileSelector = CreateWindowExW(0, L"COMBOBOX", L"Headphone starting preset",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        414, 46, 334, 240, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(profileId)), ghInst, nullptr);
    if (!undoButton || !restoreButton || !bypassButton || !profileSelector) { removed(); return kResultFalse; }
    SendMessageW(profileSelector, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(profileSelector, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Custom - current EQ"));
    for (const auto& profile : headphoneProfiles)
        SendMessageW(profileSelector, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(profile.name));
    gameSetupButton = button(gameSetupId, L"Game audio setup", 594, 155);
    graphicsSetupButton = button(graphicsSetupId, L"Graphics setup", 594, 155);
    loudnessButton = button(loudnessButtonId, L"Loudness EQ", 40, 190, true);
    strongButton = button(strongButtonId, L"Strong", 40, 150, true);
    if (!gameSetupButton || !graphicsSetupButton || !loudnessButton || !strongButton) { removed(); return kResultFalse; }
    SetWindowPos(loudnessButton, nullptr, 40, 550, 190, 36, SWP_NOZORDER);
    SetWindowPos(strongButton, nullptr, 40, 588, 150, 36, SWP_NOZORDER);
    SetWindowPos(graphicsSetupButton, nullptr, 594, 684, 155, 36, SWP_NOZORDER);
    SetWindowPos(bypassButton, nullptr, 430, 634, 158, 36, SWP_NOZORDER);
    SetTimer(window, 1, 50, nullptr);
    update();
    return CPluginView::attached(parent, type);
}

tresult PLUGIN_API Editor::removed()
{
    if (gesture >= 0) { controller->endEdit(gesture); gesture = -1; }
    if (window) {
        KillTimer(window, 1);
        DestroyWindow(window);
        window = nullptr;
        UnregisterClassW(L"EqTune.PluginEditor.020", ghInst);
    }
    sliders = {};
    undoButton = restoreButton = bypassButton = profileSelector = gameSetupButton = graphicsSetupButton = loudnessButton = strongButton = nullptr;
    if (font) { DeleteObject(font); font = nullptr; }
    if (headingFont) { DeleteObject(headingFont); headingFont = nullptr; }
    if (background) { DeleteObject(background); background = nullptr; }
    return CPluginView::removed();
}

Values Editor::read() const
{
    Values values {};
    for (int id = 0; id < parameterCount; ++id) values[id] = controller->getParamNormalized(id);
    return values;
}

void Editor::edit(int id, double value)
{
    controller->setParamNormalized(id, value);
    controller->performEdit(id, value);
}

void Editor::apply(const Values& values)
{
    for (int id = 0; id < parameterCount; ++id) {
        controller->beginEdit(id);
        edit(id, values[id]);
        controller->endEdit(id);
    }
    update();
}

void Editor::update()
{
    const auto values = read();
    for (int id = 0; id < parameterCount; ++id) {
        if (!sliders[id] || gesture == id) continue;
        const auto range = horizontalSlider(id) ? 100 : (id == preampId ? 180 : 120);
        const auto position = std::lround((horizontalSlider(id) ? values[id] : 1 - values[id]) * range);
        SendMessageW(sliders[id], TBM_SETPOS, TRUE, position);
    }
    SendMessageW(bypassButton, BM_SETCHECK, values[bypassId] >= 0.5 ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(sliders[intensityId], values[bypassId] < 0.5);
    SendMessageW(loudnessButton, BM_SETCHECK, values[loudnessEnabledId] >= 0.5 ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(loudnessButton, values[bypassId] < 0.5);
    EnableWindow(sliders[loudnessIntensityId], values[bypassId] < 0.5 && values[loudnessEnabledId] >= 0.5);
    SendMessageW(strongButton, BM_SETCHECK, values[loudnessStrongId] >= 0.5 ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(strongButton, values[bypassId] < 0.5 && values[loudnessEnabledId] >= 0.5);
    EnableWindow(undoButton, !history.empty());
    EnableWindow(restoreButton, values != saved);
    SendMessageW(profileSelector, CB_SETCURSEL, matchingHeadphoneProfile(values), 0);
    shown = values;
    InvalidateRect(window, nullptr, FALSE);
}

LRESULT CALLBACK Editor::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        auto* editor = static_cast<Editor*>(create->lpCreateParams);
        editor->window = hwnd;
    }
    auto* editor = reinterpret_cast<Editor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return editor ? editor->handle(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT Editor::handle(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
        SetTextColor(reinterpret_cast<HDC>(wParam), ink);
        SetBkColor(reinterpret_cast<HDC>(wParam), card);
        return reinterpret_cast<LRESULT>(background);
    case WM_HSCROLL:
    case WM_VSCROLL: {
        const auto id = GetDlgCtrlID(reinterpret_cast<HWND>(lParam)) - 1;
        if (id < 0 || id >= parameterCount || !sliders[id]) break;
        const auto code = LOWORD(wParam);
        if (code == TB_ENDTRACK) {
            if (gesture >= 0) { controller->endEdit(gesture); gesture = -1; }
            update();
            return 0;
        }
        const auto position = SendMessageW(sliders[id], TBM_GETPOS, 0, 0);
        const auto range = horizontalSlider(id) ? 100.0 : (id == preampId ? 180.0 : 120.0);
        const auto value = horizontalSlider(id) ? position / range : 1 - position / range;
        if (value == controller->getParamNormalized(id)) return 0;
        if (gesture != id) {
            if (gesture >= 0) controller->endEdit(gesture);
            if (history.size() >= 100) history.erase(history.begin());
            history.push_back(read());
            gesture = id;
            controller->beginEdit(id);
        }
        edit(id, value);
        update();
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == profileId && HIWORD(wParam) == CBN_SELCHANGE) {
            if (gesture >= 0) { controller->endEdit(gesture); gesture = -1; }
            const auto selection = SendMessageW(profileSelector, CB_GETCURSEL, 0, 0);
            if (selection > 0 && selection <= static_cast<LRESULT>(headphoneProfiles.size())) {
                const auto values = read();
                const auto preset = applyHeadphoneProfile(values, headphoneProfiles[selection - 1]);
                if (preset != values) {
                    if (history.size() >= 100) history.erase(history.begin());
                    history.push_back(values);
                    apply(preset);
                }
            }
            update();
            return 0;
        }
        if (HIWORD(wParam) != BN_CLICKED) break;
        if (gesture >= 0) { controller->endEdit(gesture); gesture = -1; }
        if (LOWORD(wParam) == undoId && !history.empty()) {
            const auto prior = history.back();
            history.pop_back();
            apply(prior);
        } else if (LOWORD(wParam) == restoreId) {
            history.push_back(read());
            apply(saved);
        } else if (LOWORD(wParam) == bypassButtonId || LOWORD(wParam) == loudnessButtonId || LOWORD(wParam) == strongButtonId) {
            history.push_back(read());
            const auto id = LOWORD(wParam) == bypassButtonId ? bypassId :
                (LOWORD(wParam) == strongButtonId ? loudnessStrongId : loudnessEnabledId);
            const auto control = id == bypassId ? bypassButton : (id == loudnessStrongId ? strongButton : loudnessButton);
            controller->beginEdit(id);
            edit(id, SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
            controller->endEdit(id);
            update();
        } else if (LOWORD(wParam) == gameSetupId || LOWORD(wParam) == graphicsSetupId) {
            wchar_t modulePath[32768] {};
            const auto length = GetModuleFileNameW(ghInst, modulePath, 32768);
            wchar_t systemDirectory[MAX_PATH] {};
            const auto systemLength = GetSystemDirectoryW(systemDirectory, MAX_PATH);
            if (!length || length >= 32768 || !systemLength || systemLength >= MAX_PATH) {
                MessageBoxW(window, L"Could not locate the installed setup helper or Windows PowerShell.", L"EqTune setup error", MB_OK | MB_ICONERROR);
                return 0;
            }
            const auto script = std::filesystem::path(modulePath).parent_path().parent_path()
                / L"Resources" / L"WarzoneAudioSetup.ps1";
            if (GetFileAttributesW(script.c_str()) == INVALID_FILE_ATTRIBUTES) {
                MessageBoxW(window, L"The setup helper is missing. Reinstall the complete EqTune.vst3 bundle.", L"EqTune setup error", MB_OK | MB_ICONERROR);
                return 0;
            }
            const auto executable = std::filesystem::path(systemDirectory) / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";
            auto command = L"\"" + executable.wstring() + L"\" -NoProfile -STA -ExecutionPolicy Bypass -File \"" + script.wstring() + L"\"";
            command += LOWORD(wParam) == graphicsSetupId ? L" -Mode Graphics" : L" -Mode Audio";
            STARTUPINFOW startup {};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process {};
            if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
                MessageBoxW(window, L"Could not start the game setup helper. Your game settings were not changed.", L"EqTune setup error", MB_OK | MB_ICONERROR);
            } else {
                CloseHandle(process.hThread);
                CloseHandle(process.hProcess);
            }
        }
        return 0;
    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        const auto disabled = (item->itemState & ODS_DISABLED) != 0;
        fill(item->hDC, item->rcItem, RGB(40, 52, 69));
        wchar_t label[80] {};
        GetWindowTextW(item->hwndItem, label, 80);
        text(item->hDC, font, 12, 8, label, disabled ? muted : ink);
        if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &item->rcItem);
        return TRUE;
    }
    case WM_TIMER:
        if (read() != shown) update();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps {};
        const auto dc = BeginPaint(window, &ps);
        const auto memory = CreateCompatibleDC(dc);
        const auto bitmap = CreateCompatibleBitmap(dc, width, height);
        const auto prior = SelectObject(memory, bitmap);
        paint(memory);
        BitBlt(dc, 0, 0, width, height, memory, 0, 0, SRCCOPY);
        SelectObject(memory, prior);
        DeleteObject(bitmap);
        DeleteDC(memory);
        EndPaint(window, &ps);
        return 0;
    }
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void Editor::paint(HDC dc)
{
    fill(dc, {0, 0, width, height}, bg);
    fill(dc, {24, 90, 756, 732}, card);
    text(dc, headingFont, 32, 20, L"E Q T U N E", ink);
    text(dc, font, 32, 57, L"CALL OF DUTY  /  WAVE LINK EFFECT", accent);
    text(dc, font, 414, 20, L"HEADPHONE  /  Starting presets, not calibration", muted);
    text(dc, font, 40, 124, L"Intensity", ink);
    wchar_t label[160] {};
    swprintf_s(label, L"%.0f%%", shown[intensityId] * 100);
    text(dc, font, 674, 124, label, accent);
    text(dc, font, 150, 158, L"0% Flat EQ                  100% Full tune  /  Preamp stays unchanged", muted);
    const auto bypass = shown[bypassId] >= 0.5;
    text(dc, font, 40, 191, bypass ? L"BYPASSED  /  Original audio, no EQ or preamp" : L"EQ RESPONSE  /  Before preamp, 48 kHz reference", accent);
    const auto gridPen = CreatePen(PS_SOLID, 1, RGB(42, 53, 68));
    const auto oldPen = SelectObject(dc, gridPen);
    for (int db = -12; db <= 12; db += 6) {
        const auto y = 228 + (12 - db) * 4;
        MoveToEx(dc, 72, y, nullptr); LineTo(dc, 712, y);
        swprintf_s(label, L"%+d", db);
        text(dc, font, 34, y - 9, label, muted);
    }
    for (const int hz : {20, 100, 1000, 10000, 20000}) {
        const auto x = 72 + static_cast<int>(std::log10(hz / 20.0) / 3 * 640);
        MoveToEx(dc, x, 228, nullptr); LineTo(dc, x, 324);
        if (hz >= 1000) swprintf_s(label, L"%dk", hz / 1000);
        else swprintf_s(label, L"%d", hz);
        text(dc, font, x - 10, 328, label, muted);
    }
    const auto curvePen = CreatePen(PS_SOLID, 2, accent);
    SelectObject(dc, curvePen);
    for (int i = 0; i <= 640; ++i) {
        const auto frequency = 20 * std::pow(1000.0, i / 640.0);
        const auto db = std::clamp(responseDb(shown, frequency), -12.0, 12.0);
        const auto y = 228 + static_cast<int>((12 - db) * 4);
        if (i == 0) MoveToEx(dc, 72, y, nullptr);
        else LineTo(dc, 72 + i, y);
    }
    SelectObject(dc, oldPen);
    DeleteObject(gridPen);
    DeleteObject(curvePen);
    const wchar_t* names[] = {L"120 Hz", L"350 Hz", L"900 Hz", L"2.8 kHz", L"5 kHz", L"Preamp"};
    for (int id = 0; id < 6; ++id) {
        text(dc, font, 62 + id * 112, 351, names[id], muted);
        swprintf_s(label, L"%+.1f dB", id == preampId ? preampDb(shown[id]) : gainDb(shown[id]));
        text(dc, font, 54 + id * 112, 513, label, accent);
    }
    swprintf_s(label, L"%.0f%%", shown[loudnessIntensityId] * 100);
    text(dc, font, 674, 554, label, accent);
    text(dc, font, 210, 596, L"Quiet lift / peak control. Strong raises ambience too. Start low.", muted);
    text(dc, font, 40, 693, L"Optional game setup: preview, back up, apply or restore.", muted);
    text(dc, font, 32, 745, L"Wave Link handles routing. Disable/remove this effect to return to normal audio.", muted);
}
}
