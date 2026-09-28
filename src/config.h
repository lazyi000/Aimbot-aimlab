#pragma once

// Required for MOD_NOREPEAT (and several other Win32 hotkey flags) to be
// visible. _WIN32_WINNT 0x0601 == Windows 7.
#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cwchar>
#include <mutex>
#include <shared_mutex>

namespace aimlab {

enum class WindowScope : int { FullScreen = 0, SpecificWindow = 1 };

enum class Language : int { English = 0, Portuguese = 1 };

struct Settings {
    std::atomic<bool>  aimEnabled{false};

    // Capture scope
    WindowScope scope{WindowScope::FullScreen};
    wchar_t     targetWindowTitle[256]{};
    int         targetWindowPid{0};
    wchar_t     targetWindowClass[128]{};

    // Target color
    int  targetR{255}, targetG{0}, targetB{0};
    int  colorTolerance{25};        // per-channel distance for a pixel to be considered "target"
    int  sampleRadiusPx{15};        // box (2r+1) used by Sample at center
    int  minPixelNeighbors{4};      // 3x3 neighborhood count filter

    // Target cluster (the "centroid" of the color blob)
    int  clusterRadiusPx{10};       // radius used to score density around a match
    int  minClusterSize{20};        // minimum density (matches in that radius)

    // Target lock ("sticky aim"). When several blobs are on screen at once
    // (e.g. 3 dots), a high-power aimbot would oscillate between them. With
    // stickyTarget = true, the worker commits to the first one picked and
    // keeps aiming at it until it disappears or a new blob becomes
    // dramatically closer to the crosshair.
    bool stickyTarget{true};
    int  stickRadiusPx{60};         // a candidate within this distance of the
                                    // locked blob is treated as "the same one"
    int  stickLockMaxMisses{3};     // frames without a lock hit before we drop it
    int  stickSwitchRatio{50};      // 0..100: a new blob must be
                                    // (100/ratio)x closer to the anchor to
                                    // replace the lock (lower = stickier)

    // Aim movement
    int  aimMaxSpeed{40};           // max pixels moved per frame at long range
    int  aimPower{150};             // 100 = 1.0x, 150 = 1.5x, 200 = 2.0x — applied each frame
    int  aimDeadzonePx{0};          // if target is within this radius, no movement (perfect stop)
    bool aimSnapClose{true};        // if true, within ~2x aimMaxSpeed it snaps the full delta (no slow-down at end)

    // Auto-click
    bool autoClickEnabled{true};
    int  autoClickDistancePx{30};   // when target is within this many px of crosshair, fire a click

    // Performance: when a sticky lock is active, the worker only captures
    // and scans a (searchRegionPx x searchRegionPx) box around the lock
    // instead of the whole screen. Full screen is used when no lock exists.
    int  searchRegionPx{800};       // 200..2000; larger = finds new targets faster but slower per frame

    // Misc
    int  inputMethod{0};            // 0 = mouse_event, 1 = SendInput
    int  frameDelayMs{8};
    int  hotkeyMod{MOD_CONTROL};
    int  hotkeyVk{VK_PRIOR};

    // UI language (runtime only — the user picks on each launch).
    Language language{Language::Portuguese};
};

class ConfigStore {
public:
    Settings&       mutableView()       { return s_; }
    const Settings& view() const        { return s_; }

    void setTargetColor(int r, int g, int b) {
        std::unique_lock lk(mu_);
        s_.targetR = r; s_.targetG = g; s_.targetB = b;
    }

    void setColorTolerance(int v)        { std::unique_lock lk(mu_); s_.colorTolerance = v; }
    void setSampleRadius(int v)          { std::unique_lock lk(mu_); s_.sampleRadiusPx = v; }
    void setMinPixelNeighbors(int v)     { std::unique_lock lk(mu_); s_.minPixelNeighbors = v; }
    void setClusterRadius(int v)         { std::unique_lock lk(mu_); s_.clusterRadiusPx = v; }
    void setMinClusterSize(int v)        { std::unique_lock lk(mu_); s_.minClusterSize = v; }
    void setStickyTarget(bool v)         { std::unique_lock lk(mu_); s_.stickyTarget = v; }
    void setStickRadius(int v)           { std::unique_lock lk(mu_); s_.stickRadiusPx = v; }
    void setStickMaxMisses(int v)        { std::unique_lock lk(mu_); s_.stickLockMaxMisses = v; }
    void setStickSwitchRatio(int v)      { std::unique_lock lk(mu_); s_.stickSwitchRatio = v; }
    void setAimMaxSpeed(int v)           { std::unique_lock lk(mu_); s_.aimMaxSpeed = v; }
    void setAimPower(int v)              { std::unique_lock lk(mu_); s_.aimPower = v; }
    void setAimDeadzone(int v)           { std::unique_lock lk(mu_); s_.aimDeadzonePx = v; }
    void setAimSnapClose(bool v)         { std::unique_lock lk(mu_); s_.aimSnapClose = v; }
    void setAutoClick(bool v)            { std::unique_lock lk(mu_); s_.autoClickEnabled = v; }
    void setAutoClickDistance(int v)     { std::unique_lock lk(mu_); s_.autoClickDistancePx = v; }
    void setSearchRegion(int v)          { std::unique_lock lk(mu_); s_.searchRegionPx = v; }
    void setInputMethod(int v)           { std::unique_lock lk(mu_); s_.inputMethod = v; }
    void setFrameDelay(int v)            { std::unique_lock lk(mu_); s_.frameDelayMs = v; }
    void setHotkey(int mod, int vk)      { std::unique_lock lk(mu_); s_.hotkeyMod = mod; s_.hotkeyVk = vk; }
    void setLanguage(Language v)         { std::unique_lock lk(mu_); s_.language = v; }
    void setAimEnabled(bool v)           { s_.aimEnabled.store(v); }
    bool aimEnabled() const              { return s_.aimEnabled.load(); }

    void setScope(WindowScope v)         { std::unique_lock lk(mu_); s_.scope = v; }
    void setTargetWindow(const wchar_t* title, const wchar_t* cls, int pid) {
        std::unique_lock lk(mu_);
        wcsncpy(s_.targetWindowTitle, title ? title : L"", 255);
        s_.targetWindowTitle[255] = L'\0';
        wcsncpy(s_.targetWindowClass, cls   ? cls   : L"", 127);
        s_.targetWindowClass[127] = L'\0';
        s_.targetWindowPid = pid;
    }

    void copyInto(Settings& dst) const {
        std::shared_lock lk(mu_);
        dst.scope              = s_.scope;
        wcsncpy(dst.targetWindowTitle, s_.targetWindowTitle, 255);
        dst.targetWindowTitle[255] = L'\0';
        wcsncpy(dst.targetWindowClass, s_.targetWindowClass, 127);
        dst.targetWindowClass[127] = L'\0';
        dst.targetWindowPid    = s_.targetWindowPid;
        dst.targetR = s_.targetR;
        dst.targetG = s_.targetG;
        dst.targetB = s_.targetB;
        dst.colorTolerance = s_.colorTolerance;
        dst.sampleRadiusPx = s_.sampleRadiusPx;
        dst.minPixelNeighbors = s_.minPixelNeighbors;
        dst.clusterRadiusPx = s_.clusterRadiusPx;
        dst.minClusterSize = s_.minClusterSize;
        dst.stickyTarget = s_.stickyTarget;
        dst.stickRadiusPx = s_.stickRadiusPx;
        dst.stickLockMaxMisses = s_.stickLockMaxMisses;
        dst.stickSwitchRatio = s_.stickSwitchRatio;
        dst.aimMaxSpeed = s_.aimMaxSpeed;
        dst.aimPower = s_.aimPower;
        dst.aimDeadzonePx = s_.aimDeadzonePx;
        dst.aimSnapClose = s_.aimSnapClose;
        dst.autoClickEnabled = s_.autoClickEnabled;
        dst.autoClickDistancePx = s_.autoClickDistancePx;
        dst.searchRegionPx = s_.searchRegionPx;
        dst.inputMethod = s_.inputMethod;
        dst.frameDelayMs = s_.frameDelayMs;
        dst.hotkeyMod = s_.hotkeyMod;
        dst.hotkeyVk = s_.hotkeyVk;
    }

private:
    Settings            s_;
    mutable std::shared_mutex mu_;
};

}  // namespace aimlab
