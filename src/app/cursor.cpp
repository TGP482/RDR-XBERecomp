// The menus' cursor: cursor.png or cursor.dds beside the exe, untinted, its top-left pixel on the pointer. Its visible
// part is sized, not the image, so padding and resolution don't matter: the longer side is 17 units of the 480 line
// frame. Without a file the system arrow stays.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "app/app.h"
#include "app/cursor.h"
#include "core/log.h"

using Microsoft::WRL::ComPtr;

namespace app {

namespace {

constexpr float kFrameHeight = 480.0f;
constexpr float kVisibleSize = 17.0f;
constexpr uint32_t kVisibleAlpha = 32;  // a fainter pixel is edge fringe, not part of the shape
constexpr const wchar_t* kFiles[] = {L"cursor.png", L"cursor.dds"};

ComPtr<IWICImagingFactory> g_wic;
ComPtr<IWICBitmap> g_image;  // premultiplied BGRA
UINT g_extent = 0;  // the longer side of the visible part, image pixels
bool g_read = false;
HCURSOR g_cursor = nullptr;
HCURSOR g_previous = nullptr;
int g_height = 0;  // the frame height g_cursor is sized for

// premultiplied BGRA of a file, null when it can't be read
ComPtr<IWICBitmap> Decode(const std::filesystem::path& path) {
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    ComPtr<IWICBitmap> image;
    HRESULT hr = S_OK;
    if (!g_wic) hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_wic));
    if (SUCCEEDED(hr)) {
        hr = g_wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                              &decoder);
    }
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(hr)) hr = g_wic->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                                   WICBitmapPaletteTypeCustom);
    }
    if (SUCCEEDED(hr)) hr = g_wic->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnLoad, &image);
    if (FAILED(hr)) LOGE("cursor: %s could not be read (%08X)", path.string().c_str(), unsigned(hr));
    return SUCCEEDED(hr) ? image : nullptr;
}

// the longer side of the part of the image a pixel of which is at least faintly visible
UINT VisibleExtent(IWICBitmap* image) {
    UINT w = 0;
    UINT h = 0;
    image->GetSize(&w, &h);
    std::vector<uint32_t> pixels(size_t(w) * h);
    image->CopyPixels(nullptr, w * 4, UINT(pixels.size() * 4), reinterpret_cast<BYTE*>(pixels.data()));
    UINT x0 = w;
    UINT y0 = h;
    UINT x1 = 0;
    UINT y1 = 0;
    for (UINT y = 0; y < h; y++) {
        for (UINT x = 0; x < w; x++) {
            if (pixels[size_t(y) * w + x] >> 24 < kVisibleAlpha) continue;
            x0 = std::min(x0, x);
            x1 = std::max(x1, x + 1);
            y0 = std::min(y0, y);
            y1 = std::max(y1, y + 1);
        }
    }
    return x1 > x0 ? std::max(x1 - x0, y1 - y0) : std::max(w, h);
}

void ReadCursorFile() {
    g_read = true;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    for (const wchar_t* name : kFiles) {
        auto path = ExeDir() / name;
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) continue;
        g_image = Decode(path);
        if (!g_image) continue;
        g_extent = VisibleExtent(g_image.Get());
        return;
    }
}

// straight alpha, as cursor bitmaps want it
void Unpremultiply(std::vector<uint32_t>& pixels) {
    for (uint32_t& p : pixels) {
        uint32_t a = p >> 24;
        if (!a || a == 255) continue;
        uint32_t c = 0;
        for (int s = 0; s < 24; s += 8) c |= std::min(255u, ((p >> s & 0xFF) * 255 + a / 2) / a) << s;
        p = a << 24 | c;
    }
}

HCURSOR MakeCursor(const std::vector<uint32_t>& pixels, UINT w, UINT h) {
    BITMAPV5HEADER header{};
    header.bV5Size = sizeof(header);
    header.bV5Width = LONG(w);
    header.bV5Height = -LONG(h);
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00FF0000;
    header.bV5GreenMask = 0x0000FF00;
    header.bV5BlueMask = 0x000000FF;
    header.bV5AlphaMask = 0xFF000000;
    void* bits = nullptr;
    HDC dc = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&header), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    if (!color || !bits) return nullptr;
    std::copy(pixels.begin(), pixels.end(), static_cast<uint32_t*>(bits));
    HBITMAP mask = CreateBitmap(int(w), int(h), 1, 1, nullptr);
    ICONINFO info{FALSE, 0, 0, mask, color};
    HCURSOR cursor = CreateIconIndirect(&info);
    DeleteObject(color);
    if (mask) DeleteObject(mask);
    return cursor;
}

// the image scaled to the frame
HCURSOR Build(int frame_height) {
    UINT w = 0;
    UINT h = 0;
    g_image->GetSize(&w, &h);
    float scale = frame_height * kVisibleSize / kFrameHeight / float(g_extent);
    UINT cw = std::max(1u, UINT(std::lround(w * scale)));
    UINT ch = std::max(1u, UINT(std::lround(h * scale)));
    ComPtr<IWICBitmapScaler> scaler;
    std::vector<uint32_t> pixels(size_t(cw) * ch);
    BYTE* out = reinterpret_cast<BYTE*>(pixels.data());
    bool scaled = SUCCEEDED(g_wic->CreateBitmapScaler(&scaler)) &&
                  SUCCEEDED(scaler->Initialize(g_image.Get(), cw, ch, WICBitmapInterpolationModeHighQualityCubic)) &&
                  SUCCEEDED(scaler->CopyPixels(nullptr, cw * 4, UINT(pixels.size() * 4), out));
    if (!scaled) return nullptr;
    Unpremultiply(pixels);
    return MakeCursor(pixels, cw, ch);
}

}  // namespace

void* MenuCursor() {
    static HCURSOR arrow = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
    if (!g_read) ReadCursorFile();
    RECT r;
    HWND hwnd = static_cast<HWND>(Window());
    if (!g_image || !hwnd || !GetClientRect(hwnd, &r)) return arrow;
    // the frame is shown 16:9 or 4:3 in the middle of the window
    float aspect = Widescreen() ? 16.0f / 9.0f : 4.0f / 3.0f;
    int frame_height = int(std::min(float(r.bottom), r.right / aspect));
    if (frame_height <= 0) return arrow;
    if (frame_height != g_height || !g_cursor) {
        HCURSOR cursor = Build(frame_height);
        if (!cursor) return arrow;
        // the one on screen goes a rebuild later, never while it is shown
        if (g_previous) DestroyCursor(g_previous);
        g_previous = g_cursor;
        g_cursor = cursor;
        g_height = frame_height;
    }
    return g_cursor;
}

}  // namespace app
