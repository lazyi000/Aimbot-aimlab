#include "screen_capture.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// MinGW's <windows.h> is older than Win 8.1 and does not define
// PW_RENDERFULLCONTENT — supply the value ourselves.
#ifndef PW_RENDERFULLCONTENT
#  define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace aimlab {

ScreenSize GetPrimaryScreenSize() {
    ScreenSize s{0, 0};
    s.width  = GetSystemMetrics(SM_CXSCREEN);
    s.height = GetSystemMetrics(SM_CYSCREEN);
    return s;
}

// ===== Internal: BitBlt the screen and pull the bits =====
namespace {

struct GdiCtx {
    HDC      hdcSrc = nullptr;
    HDC      hdcMem = nullptr;
    HBITMAP  hbm    = nullptr;
    HGDIOBJ  oldObj = nullptr;
    bool     ok     = false;
};

GdiCtx BeginScreenBlt(HDC hdcSrc, int w, int h) {
    GdiCtx c;
    c.hdcSrc = hdcSrc;
    c.hdcMem = CreateCompatibleDC(hdcSrc);
    if (!c.hdcMem) return c;
    c.hbm = CreateCompatibleBitmap(hdcSrc, w, h);
    if (!c.hbm) {
        DeleteDC(c.hdcMem);
        c.hdcMem = nullptr;
        return c;
    }
    c.oldObj = SelectObject(c.hdcMem, c.hbm);
    c.ok = true;
    return c;
}

void EndScreenBlt(GdiCtx& c) {
    if (c.hdcMem) {
        if (c.oldObj) SelectObject(c.hdcMem, c.oldObj);
        if (c.hbm)    DeleteObject(c.hbm);
        DeleteDC(c.hdcMem);
    }
    c.hdcMem = nullptr;
    c.hbm    = nullptr;
    c.oldObj = nullptr;
    c.ok     = false;
}

// Pull the 32-bpp BGRA pixels from a memory DC + bitmap into `out`.
// `out` is resized (not reallocated) to exactly w*h*4 bytes.
void ReadDIBitsBGRA(HDC hdcMem, HBITMAP hbm, int w, int h, ScreenFrame& out) {
    BITMAPINFOHEADER bih{};
    bih.biSize        = sizeof(BITMAPINFOHEADER);
    bih.biWidth       = w;
    bih.biHeight      = -h;            // top-down
    bih.biPlanes      = 1;
    bih.biBitCount    = 32;
    bih.biCompression = BI_RGB;

    out.width  = w;
    out.height = h;
    // Use resize (preserves capacity) instead of assign (always reallocates).
    // After the first call the vector's capacity is the max size we've ever
    // seen, so subsequent calls just shrink/grow within it.
    out.pixels.resize(static_cast<std::size_t>(w) * h * 4, 0);

    int lines = GetDIBits(hdcMem, hbm, 0, static_cast<UINT>(h),
                          out.pixels.data(),
                          reinterpret_cast<BITMAPINFO*>(&bih),
                          DIB_RGB_COLORS);
    if (lines == 0) {
        out.pixels.clear();
        out.width = out.height = 0;
    }
}

}  // namespace

ScreenFrame CapturePrimaryScreen() {
    ScreenFrame out;

    const int w = GetSystemMetrics(SM_CXSCREEN);
    const int h = GetSystemMetrics(SM_CYSCREEN);
    if (w <= 0 || h <= 0) return out;

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return out;

    GdiCtx c = BeginScreenBlt(hdcScreen, w, h);
    if (!c.ok) { ReleaseDC(nullptr, hdcScreen); return out; }

    if (!BitBlt(c.hdcMem, 0, 0, w, h, hdcScreen, 0, 0, SRCCOPY)) {
        EndScreenBlt(c);
        ReleaseDC(nullptr, hdcScreen);
        return out;
    }

    out.originX = 0;
    out.originY = 0;
    ReadDIBitsBGRA(c.hdcMem, c.hbm, w, h, out);

    EndScreenBlt(c);
    ReleaseDC(nullptr, hdcScreen);
    return out;
}

ScreenFrame CaptureRegion(int x, int y, int w, int h) {
    ScreenFrame out;
    if (w <= 0 || h <= 0) return out;

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return out;

    GdiCtx c = BeginScreenBlt(hdcScreen, w, h);
    if (!c.ok) { ReleaseDC(nullptr, hdcScreen); return out; }

    // BitBlt from screen (x, y) into the memory DC at (0, 0).
    if (!BitBlt(c.hdcMem, 0, 0, w, h, hdcScreen, x, y, SRCCOPY)) {
        EndScreenBlt(c);
        ReleaseDC(nullptr, hdcScreen);
        return out;
    }

    out.originX = x;
    out.originY = y;
    ReadDIBitsBGRA(c.hdcMem, c.hbm, w, h, out);

    EndScreenBlt(c);
    ReleaseDC(nullptr, hdcScreen);
    return out;
}

ScreenFrame CaptureWindow(HWND hwnd, int& outOriginX, int& outOriginY) {
    outOriginX = 0;
    outOriginY = 0;
    ScreenFrame out;

    if (!hwnd || !IsWindow(hwnd)) return out;

    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) return out;
    const int w = rc.right  - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return out;

    HDC hdcSrc = GetWindowDC(hwnd);
    if (!hdcSrc) return out;

    GdiCtx c = BeginScreenBlt(hdcSrc, w, h);
    if (!c.ok) { ReleaseDC(hwnd, hdcSrc); return out; }

    // PW_RENDERFULLCONTENT (Win 8.1+) forces DWM-composed rendering,
    // which is what we want for DirectX / OpenGL / UWP / browser windows.
    BOOL ok = PrintWindow(hwnd, c.hdcMem, PW_RENDERFULLCONTENT);
    if (!ok) {
        // Fall back to plain PW_CLIENTONLY (no flag) which works for
        // regular Win32 GDI windows.
        ok = PrintWindow(hwnd, c.hdcMem, 0);
    }

    if (!ok) {
        EndScreenBlt(c);
        ReleaseDC(hwnd, hdcSrc);
        return out;
    }

    out.originX = rc.left;
    out.originY = rc.top;
    ReadDIBitsBGRA(c.hdcMem, c.hbm, w, h, out);

    EndScreenBlt(c);
    ReleaseDC(hwnd, hdcSrc);
    return out;
}

}  // namespace aimlab
