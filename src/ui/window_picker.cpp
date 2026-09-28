#include "window_picker.h"

#include <algorithm>

namespace aimlab {

namespace {

struct EnumCtx {
    std::vector<WindowInfo>* out;
    HWND skip;
};

BOOL CALLBACK EnumProc(HWND h, LPARAM lParam) {
    auto* ctx = reinterpret_cast<EnumCtx*>(lParam);
    if (h == ctx->skip) return TRUE;
    if (!IsWindowVisible(h)) return TRUE;

    wchar_t title[256]{};
    GetWindowTextW(h, title, 256);
    if (title[0] == L'\0') return TRUE;  // skip unnamed windows

    wchar_t cls[128]{};
    GetClassNameW(h, cls, 128);

    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == 0) return TRUE;

    RECT r{};
    GetWindowRect(h, &r);

    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    bool minimized = false;
    if (GetWindowPlacement(h, &wp)) {
        minimized = (wp.showCmd == SW_SHOWMINIMIZED);
    }

    WindowInfo info;
    info.hwnd       = h;
    info.title      = title;
    info.className  = cls;
    info.pid        = pid;
    info.rect       = r;
    info.visible    = true;
    info.minimized  = minimized;
    ctx->out->push_back(std::move(info));
    return TRUE;
}

}  // namespace

std::vector<WindowInfo> EnumerateVisibleWindows(HWND skipHwnd) {
    std::vector<WindowInfo> out;
    EnumCtx ctx{&out, skipHwnd};
    EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&ctx));

    std::sort(out.begin(), out.end(), [](const WindowInfo& a, const WindowInfo& b) {
        return a.title < b.title;
    });
    return out;
}

HWND ResolveWindowHandle(DWORD pid, const wchar_t* title, const wchar_t* className) {
    if (pid == 0) return nullptr;

    struct Ctx {
        DWORD pid;
        const wchar_t* title;
        const wchar_t* className;
        HWND exact;
        HWND fallback;
    } ctx{pid, title, className, nullptr, nullptr};

    EnumWindows([](HWND h, LPARAM lParam) -> BOOL {
        auto* c = reinterpret_cast<Ctx*>(lParam);
        if (!IsWindowVisible(h)) return TRUE;
        DWORD p = 0;
        GetWindowThreadProcessId(h, &p);
        if (p != c->pid) return TRUE;

        wchar_t cls[128]{};
        GetClassNameW(h, cls, 128);
        if (c->className && c->className[0] && wcscmp(cls, c->className) != 0)
            return TRUE;

        wchar_t title[256]{};
        GetWindowTextW(h, title, 256);

        if (c->title && c->title[0] && wcscmp(title, c->title) == 0) {
            c->exact = h;
            return FALSE;  // found the best match
        }
        if (c->fallback == nullptr) c->fallback = h;
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    return ctx.exact ? ctx.exact : ctx.fallback;
}

}  // namespace aimlab
