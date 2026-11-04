#include "app/display.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>

#include "app/game_settings.h"
#include "core/log.h"
#include "dsound/dsound.h"

namespace app {

namespace {

constexpr DWORD kPollMs = 20;
constexpr float kSameShape = 0.01f;  // 1280x960 and the like are 4:3

HMONITOR MonitorHandle(const std::string& device) {
    struct Find {
        std::wstring device;
        HMONITOR found;
    } find{std::wstring(device.begin(), device.end()), nullptr};
    auto match = [](HMONITOR mon, HDC, LPRECT, LPARAM p) -> BOOL {
        auto* f = reinterpret_cast<Find*>(p);
        MONITORINFOEXW mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(mon, &mi) && f->device == mi.szDevice) f->found = mon;
        return !f->found;
    };
    if (!device.empty()) EnumDisplayMonitors(nullptr, nullptr, match, LPARAM(&find));
    return find.found ? find.found : MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
}

// a hidden window, behind the splash or for a handover, never pauses: it opens from the game thread
bool ShouldPause() {
    if (!Rdr().pause_on_focus_loss) return false;
    HWND hwnd = static_cast<HWND>(Window());
    if (!hwnd || !IsWindowVisible(hwnd)) return false;
    return GetForegroundWindow() != hwnd || IsIconic(hwnd);
}

}  // namespace

std::vector<Display> Displays() {
    std::vector<Display> out;
    auto add = [](HMONITOR mon, HDC, LPRECT, LPARAM list) -> BOOL {
        MONITORINFOEXW mi{};
        mi.cbSize = sizeof(mi);
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        if (!GetMonitorInfoW(mon, &mi) || !EnumDisplaySettingsW(mi.szDevice, ENUM_REGISTRY_SETTINGS, &dm)) return TRUE;
        Display d{std::string(), 0, {dm.dmPelsWidth, dm.dmPelsHeight}, (mi.dwFlags & MONITORINFOF_PRIMARY) != 0};
        for (const wchar_t* c = mi.szDevice; *c; c++) d.device += char(*c);
        size_t digits = d.device.find_last_not_of("0123456789") + 1;
        d.number = digits < d.device.size() ? std::atoi(d.device.c_str() + digits) : 0;
        reinterpret_cast<std::vector<Display>*>(list)->push_back(d);
        return TRUE;
    };
    EnumDisplayMonitors(nullptr, nullptr, add, LPARAM(&out));
    std::sort(out.begin(), out.end(), [](const Display& a, const Display& b) { return a.number < b.number; });
    return out;
}

void ShowOnMonitor() {
    HWND hwnd = static_cast<HWND>(Window());
    if (!hwnd) return;
    HMONITOR mon = MonitorHandle(Rdr().monitor);
    if (MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL) != mon) {
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(mon, &mi);
        const RECT& r = mi.rcMonitor;
        SetWindowPos(hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    ApplyDisplay();
}

float OutputAspect() {
    // asked per draw: the mode lookup runs only when the resolution setting changes
    static std::atomic<uint64_t> key{~0ull};
    static std::atomic<float> aspect{16.0f / 9.0f};
    const Settings& s = Get();
    uint64_t k = uint64_t(s.width) << 32 | s.height;
    if (key.exchange(k) != k) {
        Mode m = Output();
        float shape = m.height ? float(m.width) / float(m.height) : 16.0f / 9.0f;
        aspect = shape < kConsoleAspect + kSameShape ? kConsoleAspect : shape;
    }
    return aspect;
}

void WaitWhileUnfocused() {
    if (!ShouldPause()) return;
    LOGR("focus lost: paused");
    dsound::SetPaused(true);
    while (ShouldPause()) Sleep(kPollMs);
    dsound::SetPaused(false);
    LOGR("focus back: resumed");
}

}  // namespace app
