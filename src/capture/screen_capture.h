#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <vector>

namespace aimlab {

struct ScreenSize {
    int width;
    int height;
};

ScreenSize GetPrimaryScreenSize();

// BGRA (top-down, 32 bpp). Pixels laid out row-major: row * width + col.
struct ScreenFrame {
    int             width  = 0;
    int             height = 0;
    int             originX = 0;   // screen X of pixel (0,0)
    int             originY = 0;   // screen Y of pixel (0,0)
    std::vector<std::uint8_t> pixels;  // size = width * height * 4

    bool valid() const { return !pixels.empty() && width > 0 && height > 0; }
};

// Captures the entire primary monitor. Returns an empty frame on failure.
ScreenFrame CapturePrimaryScreen();

// Captures a rectangular region of the primary monitor. The returned
// frame's origin is the screen (x, y) of its top-left pixel. Used by the
// worker when a target lock is active so we don't have to scan the whole
// screen every frame. Returns an empty frame on failure.
ScreenFrame CaptureRegion(int x, int y, int w, int h);

// Captures a specific window. Returns an empty frame on failure.
// `outOriginX/Y` receives the screen coordinates of the frame's top-left
// (== window's GetWindowRect top-left). On failure, origin is left at 0.
ScreenFrame CaptureWindow(HWND hwnd, int& outOriginX, int& outOriginY);

}  // namespace aimlab
