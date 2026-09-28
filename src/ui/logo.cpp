#include "logo.h"

#include <cstring>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#include <GL/gl.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace {

struct WicGuard {
    IWICImagingFactory* factory = nullptr;
    bool coInit = false;

    bool init() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        coInit = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                              CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&factory));
        return SUCCEEDED(hr) && factory != nullptr;
    }

    ~WicGuard() {
        if (factory) factory->Release();
        if (coInit)  CoUninitialize();
    }
};

}  // namespace

bool LoadLogo(const wchar_t* path, LogoImage& out) {
    WicGuard wic;
    if (!wic.init()) return false;

    IWICBitmapDecoder* decoder = nullptr;
    HRESULT hr = wic.factory->CreateDecoderFromFilename(
        path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (!SUCCEEDED(hr) || !decoder) return false;

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (!SUCCEEDED(hr) || !frame) { decoder->Release(); return false; }

    IWICFormatConverter* conv = nullptr;
    hr = wic.factory->CreateFormatConverter(&conv);
    if (!SUCCEEDED(hr) || !conv) { frame->Release(); decoder->Release(); return false; }

    hr = conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
                          WICBitmapDitherTypeNone, nullptr, 0.0f,
                          WICBitmapPaletteTypeMedianCut);
    if (!SUCCEEDED(hr)) { conv->Release(); frame->Release(); decoder->Release(); return false; }

    UINT w = 0, h = 0;
    conv->GetSize(&w, &h);
    if (w == 0 || h == 0) { conv->Release(); frame->Release(); decoder->Release(); return false; }

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h * 4);
    hr = conv->CopyPixels(nullptr, w * 4,
                          static_cast<UINT>(pixels.size()), pixels.data());
    conv->Release();
    frame->Release();
    decoder->Release();
    if (!SUCCEEDED(hr)) return false;

    glGenTextures(1, &out.glId);
    glBindTexture(GL_TEXTURE_2D, out.glId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    // WIC gave us RGBA, upload as RGBA, premultiply later if needed.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 static_cast<GLsizei>(w), static_cast<GLsizei>(h),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    out.width  = static_cast<int>(w);
    out.height = static_cast<int>(h);
    return true;
}

void FreeLogo(LogoImage& img) {
    if (img.glId) {
        glDeleteTextures(1, &img.glId);
        img.glId = 0;
    }
    img.width = img.height = 0;
}

HICON CreateHIconFromPng(const wchar_t* path, int size) {
    WicGuard wic;
    if (!wic.init()) return nullptr;

    IWICBitmapDecoder* decoder = nullptr;
    HRESULT hr = wic.factory->CreateDecoderFromFilename(
        path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (!SUCCEEDED(hr) || !decoder) return nullptr;

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (!SUCCEEDED(hr) || !frame) { decoder->Release(); return nullptr; }

    // Resize to the requested icon size (cubic = high quality)
    IWICBitmapScaler* scaler = nullptr;
    hr = wic.factory->CreateBitmapScaler(&scaler);
    if (!SUCCEEDED(hr)) { frame->Release(); decoder->Release(); return nullptr; }
    hr = scaler->Initialize(frame, (UINT)size, (UINT)size,
                            WICBitmapInterpolationModeCubic);
    if (!SUCCEEDED(hr)) {
        scaler->Release(); frame->Release(); decoder->Release();
        return nullptr;
    }

    // Convert to 32bpp BGRA (what CreateIcon expects for the XOR data)
    IWICFormatConverter* conv = nullptr;
    hr = wic.factory->CreateFormatConverter(&conv);
    if (!SUCCEEDED(hr)) {
        scaler->Release(); frame->Release(); decoder->Release();
        return nullptr;
    }
    hr = conv->Initialize(scaler, GUID_WICPixelFormat32bppBGRA,
                          WICBitmapDitherTypeNone, nullptr, 0.0f,
                          WICBitmapPaletteTypeMedianCut);
    if (!SUCCEEDED(hr)) {
        conv->Release(); scaler->Release(); frame->Release(); decoder->Release();
        return nullptr;
    }

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size) * size * 4);
    hr = conv->CopyPixels(nullptr, size * 4,
                          static_cast<UINT>(pixels.size()), pixels.data());
    conv->Release(); scaler->Release(); frame->Release(); decoder->Release();
    if (!SUCCEEDED(hr)) return nullptr;

    // AND mask: all zero means "use the alpha channel" (32-bit color icon)
    std::vector<std::uint8_t> andMask((static_cast<std::size_t>(size) * size) / 8, 0);

    HICON hIcon = CreateIcon(nullptr, size, size, 1, 32,
                             andMask.data(), pixels.data());
    return hIcon;
}
