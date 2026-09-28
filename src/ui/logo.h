#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

struct LogoImage {
    int          width  = 0;
    int          height = 0;
    unsigned int glId   = 0;
};

// Loads an image from disk (PNG/JPG/etc via WIC) into an OpenGL texture.
// Returns false on failure.
bool LoadLogo(const wchar_t* path, LogoImage& out);

// Frees the GL texture.
void FreeLogo(LogoImage& img);

// Creates an HICON from a PNG file (resized to size x size). The returned
// HICON must be destroyed with DestroyIcon(). Returns NULL on failure.
HICON CreateHIconFromPng(const wchar_t* path, int size = 32);
