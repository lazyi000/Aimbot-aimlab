#include "hotkey.h"

namespace aimlab {

bool RegisterAimHotkey(HWND hwnd, int mod, int vk, int hotkeyId) {
    if (!hwnd) return false;
    // MOD_NOREPEAT prevents repeated firings while the key is held down.
    return RegisterHotKey(hwnd, hotkeyId, mod | MOD_NOREPEAT, vk) != 0;
}

void UnregisterAimHotkey(HWND hwnd, int hotkeyId) {
    if (!hwnd) return;
    UnregisterHotKey(hwnd, hotkeyId);
}

}  // namespace aimlab
