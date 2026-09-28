#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>

namespace aimlab {

struct WindowInfo {
    HWND    hwnd{};
    std::wstring title;
    std::wstring className;
    DWORD   pid{};
    RECT    rect{};
    bool    visible{false};
    bool    minimized{false};
};

// Enumerate all top-level visible windows with non-empty titles.
// `skipHwnd` is excluded (e.g. the MacroBot window itself).
std::vector<WindowInfo> EnumerateVisibleWindows(HWND skipHwnd = nullptr);

// Re-resolve a previously-selected window by (pid, title, class).
// Returns nullptr if no match. The match must be visible, top-level
// and share the same class + pid. The title is compared exactly first
// and, failing that, the first window with the same pid + class wins.
HWND ResolveWindowHandle(DWORD pid, const wchar_t* title, const wchar_t* className);

}  // namespace aimlab
