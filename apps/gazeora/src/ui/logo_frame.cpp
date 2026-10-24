#include "ui/logo_frame.h"

#include <windows.h>

namespace gazeora::app {

std::vector<uint8_t> RenderLogoFrame(int width, int height) {
    std::vector<uint8_t> pixels;
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dc || !bitmap) {
        if (dc) DeleteDC(dc);
        return pixels;
    }
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

    RECT all = {0, 0, width, height};
    HBRUSH background = CreateSolidBrush(RGB(18, 20, 30));  // kWindowBg
    FillRect(dc, &all, background);
    DeleteObject(background);

    int logo = height * 3 / 8;
    int top = (height - logo) / 2 - height / 12;
    if (HICON icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), L"IDI_GAZEORA",
                                                   IMAGE_ICON, logo, logo, LR_DEFAULTCOLOR))) {
        DrawIconEx(dc, (width - logo) / 2, top, icon, logo, logo, 0, nullptr, DI_NORMAL);
        DestroyIcon(icon);
    }

    SetBkMode(dc, TRANSPARENT);
    auto drawLine = [&](const wchar_t* text, int size, int weight, COLORREF color, int y) {
        HFONT font = CreateFontW(-size, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                 DEFAULT_PITCH, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetTextColor(dc, color);
        RECT line = {0, y, width, height};
        DrawTextW(dc, text, -1, &line, DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, oldFont);
        DeleteObject(font);
    };
    int nameY = top + logo + height / 24;
    drawLine(L"Gazeora", height / 12, FW_SEMIBOLD, RGB(236, 238, 246), nameY);   // kText
    drawLine(L"Waiting for a game", height / 30, FW_NORMAL, RGB(152, 158, 182),  // kTextDim
             nameY + height / 9);

    GdiFlush();
    const uint8_t* src = static_cast<const uint8_t*>(bits);
    pixels.assign(src, src + size_t(width) * height * 4);
    for (size_t i = 3; i < pixels.size(); i += 4) {
        pixels[i] = 255;  // GDI leaves alpha at 0
    }
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return pixels;
}

}  // namespace gazeora::app
