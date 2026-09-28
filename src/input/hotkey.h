#pragma once

#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace aimlab {

// Registers a system-wide hotkey (modifiers + virtual-key). The hotkey
// fires WM_HOTKEY to the given window with wParam == hotkeyId.
bool RegisterAimHotkey(HWND hwnd, int mod, int vk, int hotkeyId = 1);

void UnregisterAimHotkey(HWND hwnd, int hotkeyId = 1);

}  // namespace aimlab
