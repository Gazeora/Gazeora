#define IMGUI_DEFINE_MATH_OPERATORS
#include "app.h"

#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "gazeora/common/shared.h"
#include "gazeora/host/capture/frame_dump.h"
#include "gazeora/host/launch/injector.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::app {

namespace {

// RTSP port for the frame stream (8554 is the usual alternative to 554). If it's taken,
// any free port is used.
constexpr uint16_t kStreamPort = 8554;
// Size of the logo frame streamed while no game is capturing.
constexpr int kLogoFrameWidth = 1280;
constexpr int kLogoFrameHeight = 720;

// Colors taken from the app icon: navy surfaces, and a violet accent that fades to cyan
// on primary actions.
const ImU32 kAccent = IM_COL32(124, 92, 255, 255);
const ImU32 kAccentHover = IM_COL32(146, 120, 255, 255);
const ImU32 kAccentEnd = IM_COL32(56, 189, 248, 255);  // far end of the accent gradient
const ImU32 kOnAccent = IM_COL32(255, 255, 255, 255);
const ImU32 kWindowBg = IM_COL32(18, 20, 30, 255);
const ImU32 kSidebarBg = IM_COL32(13, 14, 22, 255);
const ImU32 kCardBg = IM_COL32(27, 30, 44, 255);
const ImU32 kCardHover = IM_COL32(35, 39, 56, 255);
const ImU32 kInset = IM_COL32(20, 22, 33, 255);  // tiles inside cards
const ImU32 kSecondary = IM_COL32(42, 46, 66, 255);
const ImU32 kSecondaryHover = IM_COL32(54, 59, 82, 255);
const ImU32 kBorder = IM_COL32(52, 57, 80, 255);
const ImU32 kDisabled = IM_COL32(40, 43, 60, 255);
const ImU32 kText = IM_COL32(236, 238, 246, 255);
const ImU32 kTextDim = IM_COL32(152, 158, 182, 255);
const ImU32 kTextFaint = IM_COL32(100, 106, 130, 255);
const ImU32 kDanger = IM_COL32(244, 86, 112, 255);
const ImU32 kDangerHover = IM_COL32(250, 112, 136, 255);
const ImU32 kWarning = IM_COL32(250, 190, 70, 255);

// Segoe Fluent Icons / Segoe MDL2 Assets code points, merged into every text font.
#define ICON_HOME u8""
#define ICON_LIBRARY u8""
#define ICON_SETTINGS u8""
#define ICON_ADD u8""
#define ICON_PLAY u8""
#define ICON_BACK u8""
#define ICON_DELETE u8""
#define ICON_FOLDER u8""
#define ICON_PICTURE u8""
#define ICON_CAMERA u8""
#define ICON_SEARCH u8""
#define ICON_GAME u8""
#define ICON_REFRESH u8""
#define ICON_CAPTURE u8""
#define ICON_CLOCK u8""
#define ICON_INFO u8""
#define ICON_CHECK u8""
#define ICON_WARNING u8""
#define ICON_EDIT u8""

const char kAllIcons[] = ICON_HOME ICON_LIBRARY ICON_SETTINGS ICON_ADD ICON_PLAY ICON_BACK
    ICON_DELETE ICON_FOLDER ICON_PICTURE ICON_CAMERA ICON_SEARCH ICON_GAME ICON_REFRESH
        ICON_CAPTURE ICON_CLOCK ICON_INFO ICON_CHECK ICON_WARNING ICON_EDIT;

std::string U8(const std::wstring& text) { return WideToUtf8(text); }

ImU32 WithAlpha(ImU32 color, float alpha) {
    return (color & ~IM_COL32_A_MASK) | (ImU32(alpha * 255.0f + 0.5f) << IM_COL32_A_SHIFT);
}

ImU32 Shade(const float rgb[3], float brightness, float alpha = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(
        ImVec4(rgb[0] * brightness, rgb[1] * brightness, rgb[2] * brightness, alpha));
}

// A stable color per name, for games whose exe has no icon.
void NameColor(const std::wstring& name, float rgb[3]) {
    uint32_t hash = 2166136261u;
    for (wchar_t c : name) {
        hash = (hash ^ uint32_t(c)) * 16777619u;
    }
    ImGui::ColorConvertHSVtoRGB(float(hash % 360) / 360.0f, 0.55f, 0.75f, rgb[0], rgb[1], rgb[2]);
}

const char* LabelEnd(const char* label) {
    const char* hidden = strstr(label, "##");
    return hidden ? hidden : label + strlen(label);
}

// UV rectangle that fills a rect of `rectAspect` with a texture of `texAspect`, cropping
// the overflow (CSS object-fit: cover).
void CoverUv(float texAspect, float rectAspect, ImVec2& uv0, ImVec2& uv1) {
    if (texAspect > rectAspect) {
        float w = rectAspect / texAspect;
        uv0 = ImVec2((1.0f - w) * 0.5f, 0.0f);
        uv1 = ImVec2((1.0f + w) * 0.5f, 1.0f);
    } else {
        float h = texAspect / rectAspect;
        uv0 = ImVec2(0.0f, (1.0f - h) * 0.5f);
        uv1 = ImVec2(1.0f, (1.0f + h) * 0.5f);
    }
}

// Draws `text` clipped to `maxWidth`, ending in "..." if it doesn't fit.
void TextEllipsis(ImDrawList* draw, ImFont* font, ImVec2 pos, ImU32 color, const std::string& text,
                  float maxWidth) {
    float size = font->FontSize;
    if (font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str()).x <= maxWidth) {
        draw->AddText(font, size, pos, color, text.c_str());
        return;
    }
    float dots = font->CalcTextSizeA(size, FLT_MAX, 0.0f, "...").x;
    const char* end = text.c_str() + text.size();
    while (end > text.c_str() &&
           font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str(), end).x + dots > maxWidth) {
        // Step back one UTF-8 code point.
        do {
            --end;
        } while (end > text.c_str() && (static_cast<unsigned char>(*end) & 0xC0) == 0x80);
    }
    std::string clipped(text.c_str(), end);
    clipped += "...";
    draw->AddText(font, size, pos, color, clipped.c_str());
}

bool FilledButton(const char* label, ImVec2 size, ImU32 bg, ImU32 bgHover, ImU32 fg,
                  ImFont* font, float rounding, bool enabled = true) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(label, size) && enabled;
    bool hovered = enabled && ImGui::IsItemHovered();
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(pos, pos + size,
                        enabled ? (hovered ? bgHover : bg) : kDisabled, rounding);
    const char* end = LabelEnd(label);
    ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, label, end);
    draw->AddText(font, font->FontSize, pos + (size - textSize) * 0.5f,
                  enabled ? fg : kTextFaint, label, end);
    return pressed;
}

// Text-only button, e.g. "Back" or "View all".
bool LinkButton(const char* label, ImFont* font, ImU32 color, ImU32 hoverColor) {
    const char* end = LabelEnd(label);
    ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, label, end);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(label, size);
    bool hovered = ImGui::IsItemHovered();
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImGui::GetWindowDrawList()->AddText(font, font->FontSize, pos, hovered ? hoverColor : color,
                                        label, end);
    return pressed;
}

// Primary action: violet fading into cyan, brighter on hover. `gradient` is the
// horizontal TextureCache gradient.
bool AccentButton(const char* label, ImVec2 size, ImFont* font, float rounding,
                  ImTextureID gradient, bool enabled = true) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(label, size) && enabled;
    bool hovered = enabled && ImGui::IsItemHovered();
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (enabled) {
        ImU32 start = hovered ? kAccentHover : kAccent;
        // The gradient texture fades out over its width; start it a third of the way in
        // so the left side stays solid violet.
        draw->AddRectFilled(pos, pos + size, kAccentEnd, rounding);
        draw->AddRectFilled(pos, pos + ImVec2(size.x * 0.35f, size.y), start, rounding,
                            ImDrawFlags_RoundCornersLeft);
        draw->AddImage(gradient, pos + ImVec2(size.x * 0.35f, 0), pos + size, ImVec2(0, 0),
                       ImVec2(1, 1), start);
    } else {
        draw->AddRectFilled(pos, pos + size, kDisabled, rounding);
    }
    const char* end = LabelEnd(label);
    ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, label, end);
    draw->AddText(font, font->FontSize, pos + (size - textSize) * 0.5f,
                  enabled ? kOnAccent : kTextFaint, label, end);
    return pressed;
}

bool Toggle(const char* id, bool* value, float scale) {
    ImVec2 size(40.0f * scale, 22.0f * scale);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(id, size);
    if (pressed) {
        *value = !*value;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float radius = size.y * 0.5f;
    draw->AddRectFilled(pos, pos + size, *value ? kAccent : kSecondaryHover, radius);
    float knobX = *value ? pos.x + size.x - radius : pos.x + radius;
    draw->AddCircleFilled(ImVec2(knobX, pos.y + radius), radius - 3.0f * scale,
                          *value ? kOnAccent : kTextDim);
    return pressed;
}

std::string RelativeTime(int64_t when) {
    if (when <= 0) {
        return "Never played";
    }
    int64_t ago = UnixNow() - when;
    char text[64];
    if (ago < 60) {
        return "Just now";
    } else if (ago < 3600) {
        snprintf(text, sizeof(text), "%lld min ago", ago / 60);
    } else if (ago < 86400) {
        snprintf(text, sizeof(text), "%lld h ago", ago / 3600);
    } else if (ago < 2 * 86400) {
        return "Yesterday";
    } else if (ago < 30 * 86400) {
        snprintf(text, sizeof(text), "%lld days ago", ago / 86400);
    } else {
        time_t t = time_t(when);
        tm local;
        localtime_s(&local, &t);
        strftime(text, sizeof(text), "%d %b %Y", &local);
    }
    return text;
}

std::string DateText(int64_t when) {
    if (when <= 0) {
        return "-";
    }
    time_t t = time_t(when);
    tm local;
    localtime_s(&local, &t);
    char text[64];
    strftime(text, sizeof(text), "%d %b %Y, %H:%M", &local);
    return text;
}

std::string PlaytimeText(int64_t seconds) {
    char text[32];
    if (seconds <= 0) {
        return "No playtime yet";
    } else if (seconds < 60) {
        return "< 1 min played";
    } else if (seconds < 3600) {
        snprintf(text, sizeof(text), "%lld min played", seconds / 60);
    } else {
        snprintf(text, sizeof(text), "%.1f h played", double(seconds) / 3600.0);
    }
    return text;
}

std::string ClockText(int64_t seconds) {
    char text[32];
    snprintf(text, sizeof(text), "%02lld:%02lld:%02lld", seconds / 3600, (seconds / 60) % 60,
             seconds % 60);
    return text;
}

// Badge text for a session that hasn't finished.
const char* SessionLabel(GameSession::State state) {
    switch (state) {
        case GameSession::State::Launching:
            return "LAUNCHING";
        case GameSession::State::WaitingForGame:
            return "STARTING";
        default:
            return "RUNNING";
    }
}

const char* HookStateText(LONG state) {
    switch (state) {
        case kHookNotLoaded:
            return "Waiting for hook";
        case kHookInitializing:
            return "Hook initializing";
        case kHookInstalled:
            return "Hooked, waiting for the first frame";
        case kHookCapturing:
            return "Capturing";
        case kHookError:
            return "Hook error";
        default:
            return "Unknown";
    }
}

std::wstring FolderOf(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(0, slash);
}

void ShowInExplorer(const std::wstring& path) {
    if (GetFileAttributesW(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY) {
        ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else {
        std::wstring args = L"/select,\"" + path + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }
}

// The picture streamed while no game is capturing: the app icon and name on the window
// background, as BGRA8.
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
    drawLine(L"Gazeora", height / 12, FW_SEMIBOLD, RGB(236, 238, 246), nameY);  // kText
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

// VR settings live in stream.ini next to the library file, so they're easy to edit by hand.
std::wstring StreamSettingsPath() {
    std::wstring library = GameLibrary::DefaultPath();
    return library.substr(0, library.find_last_of(L"\\/") + 1) + L"stream.ini";
}

// What players did, rewritten at every start: for working out why a stream stalled.
std::wstring StreamLogPath() {
    std::wstring library = GameLibrary::DefaultPath();
    return library.substr(0, library.find_last_of(L"\\/") + 1) + L"stream.log";
}

float ReadSetting(const std::wstring& path, const wchar_t* key, float fallback) {
    wchar_t text[64];
    GetPrivateProfileStringW(L"vr", key, L"", text, 64, path.c_str());
    return text[0] ? float(_wtof(text)) : fallback;
}

void WriteSetting(const std::wstring& path, const wchar_t* key, float value) {
    wchar_t text[64];
    swprintf(text, 64, L"%.4g", value);
    WritePrivateProfileStringW(L"vr", key, text, path.c_str());
}

VrLayout LoadVrLayout() {
    std::wstring path = StreamSettingsPath();
    VrLayout vr;
    vr.enabled = ReadSetting(path, L"enabled", 1) != 0;  // on unless turned off
    vr.screenAspect = ReadSetting(path, L"screen_aspect", vr.screenAspect);
    vr.k1 = ReadSetting(path, L"k1", vr.k1);
    vr.k2 = ReadSetting(path, L"k2", vr.k2);
    vr.imageScale = ReadSetting(path, L"image_scale", vr.imageScale);
    vr.lensSeparation = ReadSetting(path, L"lens_separation", vr.lensSeparation);
    vr.chromaCorrection = ReadSetting(path, L"chroma_correction", vr.chromaCorrection);
    return vr;
}

void SaveVrLayout(const VrLayout& vr) {
    std::wstring path = StreamSettingsPath();
    SHCreateDirectoryExW(nullptr, FolderOf(path).c_str(), nullptr);
    WriteSetting(path, L"enabled", vr.enabled ? 1.0f : 0.0f);
    WriteSetting(path, L"screen_aspect", vr.screenAspect);
    WriteSetting(path, L"k1", vr.k1);
    WriteSetting(path, L"k2", vr.k2);
    WriteSetting(path, L"image_scale", vr.imageScale);
    WriteSetting(path, L"lens_separation", vr.lensSeparation);
    WriteSetting(path, L"chroma_correction", vr.chromaCorrection);
}

// The stream codec, in the [stream] section: "h264" (the default) or "h265".
VideoCodec LoadStreamCodec() {
    wchar_t text[16];
    GetPrivateProfileStringW(L"stream", L"codec", L"h264", text, 16, StreamSettingsPath().c_str());
    return _wcsicmp(text, L"h265") == 0 ? VideoCodec::H265 : VideoCodec::H264;
}

void SaveStreamCodec(VideoCodec codec) {
    std::wstring path = StreamSettingsPath();
    SHCreateDirectoryExW(nullptr, FolderOf(path).c_str(), nullptr);
    WritePrivateProfileStringW(L"stream", L"codec", codec == VideoCodec::H265 ? L"h265" : L"h264",
                               path.c_str());
}

// Distortion of common phone viewers. Most cheap viewers are close to one of the two
// Cardboard versions; the sliders take it from there.
struct LensPreset {
    const char* name;
    float k1, k2;
};
const LensPreset kLensPresets[] = {
    {"Google Cardboard (2015)", 0.34f, 0.55f},
    {"Google Cardboard (2014)", 0.441f, 0.156f},
    {"No lens correction", 0.0f, 0.0f},
};

struct ScreenPreset {
    const char* name;
    float aspect;
};
const ScreenPreset kScreenPresets[] = {
    {"16:9", 16.0f / 9}, {"18:9", 18.0f / 9},   {"19.5:9", 19.5f / 9},
    {"20:9", 20.0f / 9}, {"21:9", 21.0f / 9},
};

// "3% lost, 3.0 Mbit/s" while players report loss or the stream has backed off because of
// it; empty on a clean link.
std::string StreamHealth(const FrameStreamServer& stream) {
    float loss = stream.PacketLoss();
    uint32_t bitrate = stream.Bitrate();
    if (loss < 0.005f && bitrate >= stream.FullBitrate()) {
        return {};
    }
    char text[96];
    snprintf(text, sizeof(text), "%.0f%% lost, bitrate lowered to %.1f Mbit/s", loss * 100,
             bitrate / 1e6);
    return text;
}

bool EndsWithI(const std::wstring& text, const wchar_t* suffix) {
    size_t n = wcslen(suffix);
    return text.size() >= n && _wcsicmp(text.c_str() + text.size() - n, suffix) == 0;
}

std::string Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return char(tolower(c)); });
    return text;
}

enum class PickKind { Games, Image, Folder };

std::vector<std::wstring> PickFiles(HWND owner, PickKind kind) {
    std::vector<std::wstring> paths;
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog)))) {
        return paths;
    }
    FILEOPENDIALOGOPTIONS options = 0;
    dialog->GetOptions(&options);
    options |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST;
    if (kind == PickKind::Games) {
        // Keep .lnk files as they are, so their arguments come along.
        options |= FOS_ALLOWMULTISELECT | FOS_NODEREFERENCELINKS;
        COMDLG_FILTERSPEC filters[] = {{L"Games and shortcuts", L"*.exe;*.lnk"},
                                       {L"All files", L"*.*"}};
        dialog->SetFileTypes(ARRAYSIZE(filters), filters);
        dialog->SetTitle(L"Add games to Gazeora");
    } else if (kind == PickKind::Image) {
        COMDLG_FILTERSPEC filters[] = {
            {L"Images", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.webp"},
            {L"All files", L"*.*"}};
        dialog->SetFileTypes(ARRAYSIZE(filters), filters);
        dialog->SetTitle(L"Choose cover art");
    } else {
        options |= FOS_PICKFOLDERS;
        dialog->SetTitle(L"Choose working directory");
    }
    dialog->SetOptions(options);
    if (FAILED(dialog->Show(owner))) {
        return paths;  // cancelled
    }

    ComPtr<IShellItemArray> items;
    DWORD count = 0;
    if (FAILED(dialog->GetResults(&items)) || FAILED(items->GetCount(&count))) {
        return paths;
    }
    for (DWORD i = 0; i < count; ++i) {
        ComPtr<IShellItem> item;
        PWSTR path = nullptr;
        if (SUCCEEDED(items->GetItemAt(i, &item)) &&
            SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
            paths.push_back(path);
            CoTaskMemFree(path);
        }
    }
    return paths;
}

// Section container with a rounded background. Pair with EndCard().
void BeginCard(const char* id, float scale, float width = 0.0f) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f * scale, 20.0f * scale));
    ImGui::BeginChild(id, ImVec2(width, 0.0f),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f * scale, 8.0f * scale));
}

void EndCard() {
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void CardTitle(ImFont* font, const char* text) {
    ImGui::PushFont(font);
    ImGui::TextUnformatted(text);
    ImGui::PopFont();
    ImGui::Spacing();
}

void FieldLabel(ImFont* font, const char* text) {
    ImGui::PushFont(font);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

}  // namespace

App::App(HWND hwnd, ID3D11Device* device, float dpiScale)
    : m_hwnd(hwnd), m_scale(dpiScale), m_device(device), m_textures(device) {
    std::wstring error;
    if (!m_library.Load(error)) {
        ShowToast(U8(error), true);
    }
    // The stream is always on and open to the whole network. It shows the logo frame until a
    // game is captured.
    m_vr = LoadVrLayout();
    m_stream.SetVrLayout(m_vr);
    m_stream.SetLogFile(StreamLogPath());
    m_stream.SetCodec(LoadStreamCodec());
    if (!m_stream.Start(kStreamPort, true, m_streamError) &&
        !m_stream.Start(0, true, m_streamError)) {
        ShowToast("Frame streaming unavailable: " + U8(m_streamError), true);
    }
}

App::~App() {
    // Count the time of games still running; the games themselves keep running.
    bool changed = false;
    for (const auto& session : m_sessions) {
        GameSession::Stats stats = session->Snapshot();
        if (stats.state == GameSession::State::Running) {
            if (Game* game = m_library.Find(session->GameId())) {
                game->playtimeSeconds += stats.elapsedSeconds;
                changed = true;
            }
        }
    }
    m_sessions.clear();
    if (changed) {
        std::wstring error;
        m_library.Save(error);
    }
}

void App::SetupImGui() {
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    wchar_t windows[MAX_PATH];
    GetWindowsDirectoryW(windows, MAX_PATH);
    std::string fonts = U8(windows) + "\\Fonts\\";
    auto exists = [](const std::string& path) {
        return GetFileAttributesW(Utf8ToWide(path).c_str()) != INVALID_FILE_ATTRIBUTES;
    };
    // Windows 11 ships Segoe Fluent Icons; Windows 10 has the MDL2 assets with the same codes.
    std::string iconFont = fonts + "SegoeIcons.ttf";
    if (!exists(iconFont)) {
        iconFont = fonts + "segmdl2.ttf";
    }

    static ImVector<ImWchar> iconRanges;
    if (iconRanges.empty()) {
        ImFontGlyphRangesBuilder builder;
        builder.AddText(kAllIcons);
        builder.BuildRanges(&iconRanges);
    }
    static const ImWchar textRanges[] = {0x0020, 0x00FF, 0x0100, 0x024F, 0x2010, 0x205E, 0};

    auto addFont = [&](const char* file, float size, bool withIcons) {
        float px = std::round(size * m_scale);
        std::string path = fonts + file;
        ImFont* font = exists(path)
                           ? io.Fonts->AddFontFromFileTTF(path.c_str(), px, nullptr, textRanges)
                           : nullptr;
        if (!font) {
            ImFontConfig config;
            config.SizePixels = px;
            font = io.Fonts->AddFontDefault(&config);
        }
        if (withIcons && exists(iconFont)) {
            ImFontConfig config;
            config.MergeMode = true;
            config.PixelSnapH = true;
            config.GlyphOffset.y = std::round(px * 0.12f);
            io.Fonts->AddFontFromFileTTF(iconFont.c_str(), px * 0.92f, &config, iconRanges.Data);
        }
        return font;
    };
    m_fonts.body = addFont("segoeui.ttf", 15.0f, true);  // first font = default
    m_fonts.bold = addFont("seguisb.ttf", 15.0f, true);
    m_fonts.small = addFont("segoeui.ttf", 13.0f, true);
    m_fonts.heading = addFont("seguisb.ttf", 22.0f, true);
    m_fonts.title = addFont("segoeuib.ttf", 36.0f, true);
    m_fonts.hugeIcon = addFont("segoeui.ttf", 56.0f, true);

    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);
    style.WindowPadding = ImVec2(10, 10);  // popups and menus; the panes set their own
    style.FramePadding = ImVec2(10, 7);
    style.ItemSpacing = ImVec2(10, 10);
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.ScrollbarSize = 10.0f;
    style.WindowBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.ScaleAllSizes(m_scale);

    ImVec4* colors = style.Colors;
    auto c = [](ImU32 color) { return ImGui::ColorConvertU32ToFloat4(color); };
    colors[ImGuiCol_Text] = c(kText);
    colors[ImGuiCol_TextDisabled] = c(kTextFaint);
    colors[ImGuiCol_WindowBg] = c(kWindowBg);
    colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_PopupBg] = c(WithAlpha(kCardBg, 0.98f));
    colors[ImGuiCol_Border] = c(kBorder);
    colors[ImGuiCol_FrameBg] = c(kSecondary);
    colors[ImGuiCol_FrameBgHovered] = c(kSecondaryHover);
    colors[ImGuiCol_FrameBgActive] = c(kSecondaryHover);
    colors[ImGuiCol_Button] = c(kSecondary);
    colors[ImGuiCol_ButtonHovered] = c(kSecondaryHover);
    colors[ImGuiCol_ButtonActive] = c(kBorder);
    colors[ImGuiCol_Header] = c(WithAlpha(kAccent, 0.22f));
    colors[ImGuiCol_HeaderHovered] = c(WithAlpha(kAccent, 0.32f));
    colors[ImGuiCol_HeaderActive] = c(WithAlpha(kAccent, 0.42f));
    colors[ImGuiCol_CheckMark] = c(kAccent);
    colors[ImGuiCol_SliderGrab] = c(kAccent);
    colors[ImGuiCol_SliderGrabActive] = c(kAccentHover);
    colors[ImGuiCol_TextSelectedBg] = c(WithAlpha(kAccent, 0.35f));
    colors[ImGuiCol_NavHighlight] = c(kAccent);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrab] = c(kSecondary);
    colors[ImGuiCol_ScrollbarGrabHovered] = c(kSecondaryHover);
    colors[ImGuiCol_ScrollbarGrabActive] = c(kBorder);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.6f);
}

void App::Tick() {
    if (m_logoDirty) {
        UpdateLogoFrame();  // here rather than in Draw: it uses the UI's device context
        m_logoDirty = false;
    }
    for (auto it = m_sessions.begin(); it != m_sessions.end();) {
        GameSession& session = **it;
        if (!session.Finished()) {
            ++it;
            continue;
        }
        GameSession::Stats stats = session.Snapshot();
        Game* game = m_library.Find(session.GameId());
        std::string name = game ? U8(game->name) : "Game";
        if (stats.state == GameSession::State::Failed) {
            ShowToast("Couldn't launch " + name + ": " + U8(stats.error), true);
        } else if (stats.state == GameSession::State::Exited && game) {
            game->playtimeSeconds += stats.elapsedSeconds;
            SaveLibrary();
        }
        it = m_sessions.erase(it);
    }
}

bool App::WantsContinuousFrames() const {
    if (!m_toasts.empty() || ImGui::GetIO().WantTextInput) {
        return true;
    }
    for (const auto& session : m_sessions) {
        if (session->Snapshot().state == GameSession::State::Launching) {
            return true;
        }
    }
    return false;
}

void App::OnFilesDropped(const std::vector<std::wstring>& paths) {
    m_deferred.push_back([this, paths] { AddGamePaths(paths); });
}

void App::OnActivated() {
    if (!m_library.ChangedOnDisk()) {
        return;
    }
    m_deferred.push_back([this] {
        std::wstring error;
        if (!m_library.Load(error)) {
            ShowToast(U8(error), true);
        }
        m_editId = 0;
        if (m_page == Page::Details && !m_library.Find(m_detailsId)) {
            m_page = Page::Library;
        }
    });
}

void App::Draw() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(3);

    DrawSidebar();
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(40), S(32)));
    ImGui::BeginChild("##content", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar(2);
    if (m_page != m_shownPage || m_detailsId != m_shownDetailsId) {
        m_shownPage = m_page;
        m_shownDetailsId = m_detailsId;
        ImGui::SetScrollY(0.0f);
    }
    switch (m_page) {
        case Page::Home:
            DrawHome();
            break;
        case Page::Library:
            DrawLibrary();
            break;
        case Page::Details:
            DrawDetails();
            break;
        case Page::Settings:
            DrawSettings();
            break;
    }
    ImGui::Dummy(ImVec2(0, S(8)));
    ImGui::EndChild();
    ImGui::End();

    DrawToasts();

    std::vector<std::function<void()>> deferred;
    deferred.swap(m_deferred);
    for (auto& action : deferred) {
        action();
    }
}

void App::DrawSidebar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kSidebarBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(14), S(24)));
    ImGui::BeginChild("##sidebar", ImVec2(S(236), 0), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(2);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, S(4)));
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Logo: the exe's own icon, then the name.
    ImVec2 pos = ImGui::GetCursorScreenPos() + ImVec2(S(8), -S(4));
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    const Texture& logo = m_textures.Icon(exePath);
    if (logo) {
        draw->AddImage(ImTextureID(logo.Id()), pos, pos + ImVec2(S(32), S(32)));
    }
    draw->AddText(m_fonts.heading, m_fonts.heading->FontSize, pos + ImVec2(S(42), S(3)), kText,
                  "GAZEORA");
    ImGui::Dummy(ImVec2(0, S(52)));

    struct NavItem {
        const char* icon;
        const char* label;
        Page page;
    };
    const NavItem items[] = {
        {ICON_HOME, "Home", Page::Home},
        {ICON_LIBRARY, "Library", Page::Library},
        {ICON_SETTINGS, "Settings", Page::Settings},
    };
    float width = ImGui::GetContentRegionAvail().x;
    for (const NavItem& item : items) {
        bool selected = m_page == item.page || (item.page == Page::Library && m_page == Page::Details);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 size(width, S(42));
        ImGui::PushID(item.label);
        if (ImGui::InvisibleButton("##nav", size)) {
            m_page = item.page;
        }
        bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        if (selected || hovered) {
            draw->AddRectFilled(p, p + size,
                                selected ? WithAlpha(kAccent, 0.16f) : WithAlpha(kText, 0.04f),
                                S(8));
        }
        if (selected) {
            draw->AddRectFilled(p + ImVec2(0, S(11)), p + ImVec2(S(3), size.y - S(11)), kAccent,
                                S(2));
        }
        ImU32 color = selected ? kText : (hovered ? kText : kTextDim);
        float textY = p.y + (size.y - m_fonts.body->FontSize) * 0.5f;
        draw->AddText(m_fonts.body, m_fonts.body->FontSize, ImVec2(p.x + S(16), textY),
                      selected ? kAccent : color, item.icon);
        draw->AddText(selected ? m_fonts.bold : m_fonts.body, m_fonts.body->FontSize,
                      ImVec2(p.x + S(48), textY), color, item.label);
    }

    // "Now playing" panel at the bottom while a game launched from here is running.
    const GameSession* playing = nullptr;
    for (const auto& session : m_sessions) {
        if (!session->Finished()) {
            playing = session.get();
        }
    }
    if (playing) {
        const Game* game = m_library.Find(playing->GameId());
        GameSession::Stats stats = playing->Snapshot();
        float panelHeight = S(playing->Capture() ? 118 : 96);
        float bottom = ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - S(18);
        ImVec2 p(ImGui::GetCursorScreenPos().x, bottom - panelHeight);
        ImVec2 size(width, panelHeight);
        ImGui::SetCursorScreenPos(p);
        if (ImGui::InvisibleButton("##nowplaying", size) && game) {
            OpenDetails(game->id);
        }
        bool hovered = ImGui::IsItemHovered();
        draw->AddRectFilled(p, p + size, hovered ? kCardHover : kCardBg, S(8));

        ImVec2 inner = p + ImVec2(S(14), S(12));
        bool launching = stats.state != GameSession::State::Running;
        draw->AddCircleFilled(inner + ImVec2(S(4), S(8)), S(4),
                              launching ? kWarning : kAccent);
        draw->AddText(m_fonts.small, m_fonts.small->FontSize, inner + ImVec2(S(14), 0),
                      launching ? kWarning : kAccent,
                      launching ? SessionLabel(stats.state) : "NOW PLAYING");
        inner.y += S(22);
        TextEllipsis(draw, m_fonts.bold, inner, kText, game ? U8(game->name) : "Game",
                     size.x - S(28));
        inner.y += S(24);
        draw->AddText(m_fonts.small, m_fonts.small->FontSize, inner, kTextDim,
                      (ICON_CLOCK "  " + ClockText(stats.elapsedSeconds)).c_str());
        if (playing->Capture()) {
            inner.y += S(20);
            char text[64];
            if (launching) {
                snprintf(text, sizeof(text), ICON_CAPTURE "  Waiting for the game");
            } else if (stats.fps > 0.0) {
                snprintf(text, sizeof(text), ICON_CAPTURE "  %.0f fps  %ux%u", stats.fps,
                         stats.width, stats.height);
            } else {
                snprintf(text, sizeof(text), ICON_CAPTURE "  %s", HookStateText(stats.hookState));
            }
            TextEllipsis(draw, m_fonts.small, inner, kTextDim, text, size.x - S(28));
        }
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void App::DrawHome() {
    if (m_library.Games().empty()) {
        DrawEmptyLibrary();
        return;
    }

    // Feature the running game, else the last one played, else the newest.
    const Game* featured = nullptr;
    const char* eyebrow = "READY TO PLAY";
    for (const auto& session : m_sessions) {
        if (!session->Finished()) {
            featured = m_library.Find(session->GameId());
            eyebrow = "NOW PLAYING";
        }
    }
    if (!featured) {
        std::vector<const Game*> recent = SortedGames(Sort::RecentlyPlayed, "");
        featured = recent.front();
        eyebrow = featured->lastPlayed > 0 ? "CONTINUE PLAYING" : "RECENTLY ADDED";
    }

    ImGui::PushFont(m_fonts.title);
    ImGui::TextUnformatted("Home");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, S(6)));
    DrawGameBanner(*featured, S(320), eyebrow, true);
    ImGui::Dummy(ImVec2(0, S(28)));

    // One row of recent games, as many as fit.
    ImGui::PushFont(m_fonts.heading);
    ImGui::TextUnformatted("Your games");
    ImGui::PopFont();
    ImGui::SameLine(ImGui::GetContentRegionMax().x - S(70));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(6));
    if (LinkButton("View all##home", m_fonts.body, kAccent, kAccentHover)) {
        m_page = Page::Library;
    }
    ImGui::Dummy(ImVec2(0, S(4)));

    float gap = S(20);
    float avail = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, int((avail + gap) / (S(176) + gap)));
    float cardWidth = (avail - gap * float(columns - 1)) / float(columns);
    std::vector<const Game*> games = SortedGames(Sort::RecentlyPlayed, "");
    for (int i = 0; i < columns && i < int(games.size()); ++i) {
        if (i > 0) {
            ImGui::SameLine(0.0f, gap);
        }
        DrawGameCard(*games[i], cardWidth);
    }
}

void App::DrawEmptyLibrary() {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float blockHeight = S(260);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (avail.y - blockHeight) * 0.4f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float centerX = ImGui::GetCursorScreenPos().x + avail.x * 0.5f;
    float y = ImGui::GetCursorScreenPos().y;

    float circle = S(56);
    draw->AddCircleFilled(ImVec2(centerX, y + circle), circle, kCardBg);
    ImVec2 iconSize = m_fonts.hugeIcon->CalcTextSizeA(m_fonts.hugeIcon->FontSize, FLT_MAX, 0.0f,
                                                      ICON_GAME);
    draw->AddText(m_fonts.hugeIcon, m_fonts.hugeIcon->FontSize,
                  ImVec2(centerX - iconSize.x * 0.5f, y + circle - iconSize.y * 0.5f - S(4)),
                  kAccent, ICON_GAME);
    y += circle * 2 + S(24);

    auto centered = [&](ImFont* font, ImU32 color, const char* text) {
        ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text);
        draw->AddText(font, font->FontSize, ImVec2(centerX - size.x * 0.5f, y), color, text);
        y += size.y + S(6);
    };
    centered(m_fonts.heading, kText, "Your library is empty");
    centered(m_fonts.body, kTextDim, "Add your games to launch them from Gazeora.");
    centered(m_fonts.small, kTextFaint, "Pick an .exe or shortcut, or drop it onto this window.");
    y += S(16);

    ImVec2 buttonSize(S(170), S(42));
    ImGui::SetCursorScreenPos(ImVec2(centerX - buttonSize.x * 0.5f, y));
    if (AccentButton(ICON_ADD "   Add game##empty", buttonSize, m_fonts.bold, S(6),
                     AccentGradient())) {
        m_deferred.push_back([this] { AddGamesFromDialog(); });
    }
}

void App::DrawLibrary() {
    // Header: title + count on the left; search, sort and Add on the right.
    float headerY = ImGui::GetCursorPosY();
    ImGui::PushFont(m_fonts.title);
    ImGui::TextUnformatted("Library");
    ImGui::PopFont();
    ImGui::SameLine(0.0f, S(14));
    ImGui::SetCursorPosY(headerY + S(16));
    char count[32];
    snprintf(count, sizeof(count), "%zu %s", m_library.Games().size(),
             m_library.Games().size() == 1 ? "game" : "games");
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextUnformatted(count);
    ImGui::PopStyleColor();

    float right = ImGui::GetContentRegionMax().x;
    float addWidth = S(140), sortWidth = S(170), searchWidth = S(240), spacing = S(10);
    float controlsY = headerY + S(6);
    float x = right - addWidth - sortWidth - searchWidth - spacing * 2;

    ImGui::SetCursorPos(ImVec2(x, controlsY));
    ImGui::SetNextItemWidth(searchWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(12), S(10)));
    ImGui::InputTextWithHint("##search", ICON_SEARCH "  Search games", &m_search);

    ImGui::SetCursorPos(ImVec2(x + searchWidth + spacing, controlsY));
    ImGui::SetNextItemWidth(sortWidth);
    const char* sortNames[] = {"Recently played", "Name", "Recently added"};
    int sort = int(m_sort);
    if (ImGui::Combo("##sort", &sort, sortNames, IM_ARRAYSIZE(sortNames))) {
        m_sort = Sort(sort);
    }
    ImGui::PopStyleVar();

    ImGui::SetCursorPos(ImVec2(right - addWidth, controlsY));
    if (AccentButton(ICON_ADD "   Add game##library",
                     ImVec2(addWidth, ImGui::GetFrameHeight() + S(6)), m_fonts.bold, S(6),
                     AccentGradient())) {
        m_deferred.push_back([this] { AddGamesFromDialog(); });
    }
    ImGui::SetCursorPosY(headerY + S(76));

    if (m_library.Games().empty()) {
        DrawEmptyLibrary();
        return;
    }
    std::vector<const Game*> games = SortedGames(m_sort, m_search);
    if (games.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::Text("No games match \"%s\".", m_search.c_str());
        ImGui::PopStyleColor();
        return;
    }

    float gap = S(20);
    float avail = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, int((avail + gap) / (S(176) + gap)));
    float cardWidth = (avail - gap * float(columns - 1)) / float(columns);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, S(26)));
    for (size_t i = 0; i < games.size(); ++i) {
        if (i % size_t(columns) != 0) {
            ImGui::SameLine(0.0f, gap);
        }
        DrawGameCard(*games[i], cardWidth);
    }
    ImGui::PopStyleVar();
}

void App::DrawGameCard(const Game& game, float width) {
    ImGui::PushID(int(game.id));
    float coverHeight = std::round(width * 4.0f / 3.0f);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size(width, coverHeight + S(56));
    ImGui::InvisibleButton("##card", size);
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    DrawGameContextMenu(game);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 coverMax = pos + ImVec2(width, coverHeight);
    float rounding = S(10);
    const Texture& art = ArtFor(game);
    bool isCover = !game.coverPath.empty() && m_textures.Image(game.coverPath);

    if (isCover) {
        ImVec2 uv0, uv1;
        CoverUv(art.Aspect(), width / coverHeight, uv0, uv1);
        draw->AddImageRounded(ImTextureID(art.Id()), pos, coverMax, uv0, uv1,
                              IM_COL32_WHITE, rounding);
    } else {
        // Icon over a gradient in the icon's own color.
        float rgb[3];
        if (art) {
            std::copy(art.averageColor, art.averageColor + 3, rgb);
        } else {
            NameColor(game.name, rgb);
        }
        draw->AddRectFilled(pos, coverMax, Shade(rgb, 0.22f), rounding);
        draw->AddImageRounded(ImTextureID(m_textures.Gradient(true).Id()), pos, coverMax,
                              ImVec2(0, 0), ImVec2(1, 1), Shade(rgb, 0.55f, 0.85f), rounding);
        // Exes with only small icons get scaled less, so they don't turn to mush.
        float iconSize = std::min(width * 0.46f, S(112));
        if (art) {
            iconSize = std::min(iconSize, std::max(S(64), float(art.width) * 2.5f));
        }
        ImVec2 iconPos = pos + ImVec2((width - iconSize) * 0.5f, (coverHeight - iconSize) * 0.42f);
        if (art) {
            draw->AddImage(ImTextureID(art.Id()), iconPos, iconPos + ImVec2(iconSize, iconSize));
        } else {
            std::string initial = U8(game.name.substr(0, 1));
            ImVec2 textSize = m_fonts.hugeIcon->CalcTextSizeA(m_fonts.hugeIcon->FontSize, FLT_MAX,
                                                              0.0f, initial.c_str());
            draw->AddText(m_fonts.hugeIcon, m_fonts.hugeIcon->FontSize,
                          iconPos + (ImVec2(iconSize, iconSize) - textSize) * 0.5f, kText,
                          initial.c_str());
        }
    }

    // Running badge.
    GameSession* session = SessionFor(game.id);
    if (session) {
        const char* label = SessionLabel(session->Snapshot().state);
        ImVec2 textSize =
            m_fonts.small->CalcTextSizeA(m_fonts.small->FontSize, FLT_MAX, 0.0f, label);
        ImVec2 badge = pos + ImVec2(S(10), S(10));
        draw->AddRectFilled(badge, badge + textSize + ImVec2(S(16), S(6)), kAccent, S(4));
        draw->AddText(m_fonts.small, m_fonts.small->FontSize, badge + ImVec2(S(8), S(3)),
                      kOnAccent, label);
    }

    // Hover: accent outline, dim the art, and a round play button in the middle.
    bool overPlay = false;
    if (hovered) {
        draw->AddRectFilled(pos, coverMax, WithAlpha(kSidebarBg, 0.45f), rounding);
        draw->AddRect(pos, coverMax, kAccent, rounding, 0, S(2));
        ImVec2 center = pos + ImVec2(width * 0.5f, coverHeight * 0.5f);
        float radius = S(28);
        ImVec2 mouse = ImGui::GetIO().MousePos;
        ImVec2 d = mouse - center;
        overPlay = !session && d.x * d.x + d.y * d.y <= radius * radius;
        if (!session) {
            draw->AddCircleFilled(center, radius, overPlay ? kAccentHover : kAccent, 48);
            float t = radius * 0.42f;
            draw->AddTriangleFilled(center + ImVec2(-t * 0.7f, -t), center + ImVec2(-t * 0.7f, t),
                                    center + ImVec2(t * 1.05f, 0), kOnAccent);
        }
    }

    TextEllipsis(draw, m_fonts.bold, ImVec2(pos.x + S(2), coverMax.y + S(10)),
                 hovered ? kText : WithAlpha(kText, 0.9f), U8(game.name), width - S(4));
    std::string subtitle = session ? "Playing now" : RelativeTime(game.lastPlayed);
    draw->AddText(m_fonts.small, m_fonts.small->FontSize,
                  ImVec2(pos.x + S(2), coverMax.y + S(32)), session ? kAccent : kTextDim,
                  subtitle.c_str());

    if (clicked) {
        if (overPlay) {
            Launch(game.id, game.captureOnLaunch);
        } else {
            OpenDetails(game.id);
        }
    }
    ImGui::PopID();
}

void App::DrawGameContextMenu(const Game& game) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(8), S(8)));
    bool open = ImGui::BeginPopupContextItem("##context");
    ImGui::PopStyleVar();
    if (!open) {
        return;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(8), S(10)));
    bool running = SessionFor(game.id) != nullptr;
    if (ImGui::MenuItem(ICON_PLAY "   Play", nullptr, false, !running)) {
        Launch(game.id, game.captureOnLaunch);
    }
    if (ImGui::MenuItem(ICON_CAPTURE "   Play with capture", nullptr, false, !running)) {
        Launch(game.id, true);
    }
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_INFO "   Details")) {
        OpenDetails(game.id);
    }
    if (ImGui::MenuItem(ICON_FOLDER "   Open file location")) {
        ShowInExplorer(game.exePath);
    }
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
    if (ImGui::MenuItem(ICON_DELETE "   Remove from library")) {
        m_pendingRemoveId = game.id;
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::EndPopup();
}

void App::DrawGameBanner(const Game& game, float height, const char* eyebrow, bool showDetails) {
    ImGui::PushID("banner");
    ImGui::PushID(int(game.id));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    ImVec2 max = pos + ImVec2(width, height);
    float rounding = S(14);

    const Texture& art = ArtFor(game);
    bool isCover = !game.coverPath.empty() && m_textures.Image(game.coverPath);
    float rgb[3];
    if (art) {
        std::copy(art.averageColor, art.averageColor + 3, rgb);
    } else {
        NameColor(game.name, rgb);
    }

    // Background: the cover (or the icon's color), fading into the page on the left
    // so the text stays readable.
    draw->AddRectFilled(pos, max, Shade(rgb, 0.3f), rounding);
    if (isCover) {
        ImVec2 uv0, uv1;
        CoverUv(art.Aspect(), width / height, uv0, uv1);
        draw->AddImageRounded(ImTextureID(art.Id()), pos, max, uv0, uv1, IM_COL32_WHITE,
                              rounding);
    } else if (art) {
        float iconSize = std::min(height * 0.72f, std::max(S(128), float(art.width) * 4.0f));
        ImVec2 iconPos(max.x - iconSize - height * 0.16f, pos.y + (height - iconSize) * 0.5f);
        draw->AddImage(ImTextureID(art.Id()), iconPos, iconPos + ImVec2(iconSize, iconSize),
                       ImVec2(0, 0), ImVec2(1, 1), WithAlpha(IM_COL32_WHITE, 0.92f));
    }
    draw->AddImageRounded(ImTextureID(m_textures.Gradient(false).Id()), pos,
                          ImVec2(pos.x + width * 0.85f, max.y), ImVec2(0, 0), ImVec2(1, 1),
                          Shade(rgb, 0.12f, 0.97f), rounding,
                          ImDrawFlags_RoundCornersLeft);

    float padding = S(40);
    float textWidth = width * 0.55f;
    GameSession* session = SessionFor(game.id);
    GameSession::Stats stats;
    if (session) {
        stats = session->Snapshot();
    }
    bool starting = session && stats.state != GameSession::State::Running;

    ImVec2 cursor = pos + ImVec2(padding, padding);
    draw->AddText(m_fonts.small, m_fonts.small->FontSize, cursor, starting ? kWarning : kAccent,
                  starting ? "STARTING" : eyebrow);
    cursor.y += S(22);
    TextEllipsis(draw, m_fonts.title, cursor, kText, U8(game.name), textWidth);
    cursor.y += m_fonts.title->FontSize + S(8);

    std::string meta =
        starting  ? ICON_CLOCK "  Waiting for the game to start"
        : session ? ICON_CLOCK "  Playing for " + ClockText(stats.elapsedSeconds)
                  : ICON_CLOCK "  " + RelativeTime(game.lastPlayed) + "   \xC2\xB7   " +
                        PlaytimeText(game.playtimeSeconds);
    draw->AddText(m_fonts.body, m_fonts.body->FontSize, cursor, WithAlpha(kText, 0.85f),
                  meta.c_str());

    // Buttons along the bottom.
    float buttonHeight = S(46);
    ImVec2 buttons(pos.x + padding, max.y - padding - buttonHeight);
    ImGui::SetCursorScreenPos(buttons);
    const char* playLabel = !session ? ICON_PLAY "   PLAY##play"
                            : stats.state == GameSession::State::Launching ? "LAUNCHING...##play"
                            : stats.state == GameSession::State::WaitingForGame
                                ? "STARTING...##play"
                                : "RUNNING##play";
    if (AccentButton(playLabel, ImVec2(S(170), buttonHeight), m_fonts.bold, S(6),
                     AccentGradient(), !session)) {
        Launch(game.id, game.captureOnLaunch);
    }
    if (showDetails) {
        ImGui::SameLine(0.0f, S(12));
        if (FilledButton("Details##details", ImVec2(S(120), buttonHeight),
                         WithAlpha(kText, 0.12f), WithAlpha(kText, 0.2f), kText,
                         m_fonts.bold, S(6))) {
            OpenDetails(game.id);
        }
    }

    // Capture switch next to the buttons.
    ImGui::SameLine(0.0f, S(24));
    float toggleY = buttons.y + (buttonHeight - S(22)) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, toggleY));
    bool capture = game.captureOnLaunch;
    if (Toggle("##capture", &capture, m_scale)) {
        if (Game* editable = m_library.Find(game.id)) {
            editable->captureOnLaunch = capture;
            SaveLibrary();
        }
    }
    ImGui::SameLine(0.0f, S(10));
    ImGui::SetCursorScreenPos(
        ImVec2(ImGui::GetCursorScreenPos().x, toggleY + (S(22) - m_fonts.body->FontSize) * 0.5f));
    ImGui::TextUnformatted("Capture with Gazeora");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Starts the game with the Gazeora capture hook injected,\n"
                          "so every frame it renders is captured.");
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x, max.y));
    ImGui::Dummy(ImVec2(width, 0));
    ImGui::PopID();
    ImGui::PopID();
}

void App::DrawDetails() {
    Game* game = m_library.Find(m_detailsId);
    if (!game) {
        m_page = Page::Library;
        return;
    }
    if (m_editId != game->id) {
        m_editId = game->id;
        m_editName = U8(game->name);
        m_editArgs = U8(game->arguments);
        m_editWorkDir = U8(game->workingDir);
        m_editProcess = U8(game->processName);
        m_editSteamAppId = FindSteamAppId(game->exePath);
    }

    if (LinkButton(ICON_BACK "   Library##back", m_fonts.body, kTextDim, kText)) {
        m_page = Page::Library;
    }
    ImGui::Dummy(ImVec2(0, S(10)));
    DrawGameBanner(*game, S(300), SessionFor(game->id) ? "NOW PLAYING" : "GAME", false);
    ImGui::Dummy(ImVec2(0, S(20)));

    float gap = S(20);
    float sideWidth = S(330);
    if (ImGui::BeginTable("##details", 2, ImGuiTableFlags_None)) {
        ImGui::TableSetupColumn("main", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("side", ImGuiTableColumnFlags_WidthFixed, sideWidth);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(10), gap));
        DrawSessionCard(*game);
        DrawGameSettingsCard(*game);
        ImGui::PopStyleVar();
        ImGui::TableSetColumnIndex(1);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + gap * 0.5f);
        DrawGameInfoCard(*game);
        ImGui::EndTable();
    }
}

void App::DrawSessionCard(const Game& game) {
    BeginCard("##session", m_scale);
    GameSession* session = SessionFor(game.id);
    if (!session) {
        CardTitle(m_fonts.heading, ICON_CAPTURE "  Gazeora capture");
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(
            game.captureOnLaunch
                ? "Capture is on. When Gazeora runs the exe itself it injects its capture hook "
                  "before the game creates its renderer; when the game starts through Steam or "
                  "its launcher, the hook goes in as soon as the game process appears."
                : "Capture is off. The game starts normally. Turn on \"Capture with Gazeora\" "
                  "to inject the capture hook when it launches.");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        EndCard();
        return;
    }

    GameSession::Stats stats = session->Snapshot();
    CardTitle(m_fonts.heading, ICON_PLAY "  Session");
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    if (stats.state == GameSession::State::Launching) {
        ImGui::TextUnformatted(session->Capture() ? "Launching and injecting the capture hook..."
                                                  : "Launching...");
    } else if (stats.state == GameSession::State::WaitingForGame) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::Text("Started %s. Waiting for %s to start%s...",
                    session->ThroughSteam() ? "through Steam" : "its launcher",
                    U8(session->GameProcessName()).c_str(),
                    session->Capture() ? ", then attaching capture" : "");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        if (FilledButton("Stop waiting##cancel", ImVec2(S(140), S(36)), kSecondary,
                         kSecondaryHover, kText, m_fonts.bold, S(6))) {
            session->Cancel();
        }
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    } else {
        ImGui::Text("PID %lu   \xC2\xB7   running for %s", stats.pid,
                    ClockText(stats.elapsedSeconds).c_str());
    }
    ImGui::PopStyleColor();

    if (session->Capture() && stats.state == GameSession::State::Running) {
        ImGui::Dummy(ImVec2(0, S(4)));
        struct Tile {
            const char* label;
            std::string value;
        };
        char fps[32], res[32], latency[32];
        snprintf(fps, sizeof(fps), "%.0f", stats.fps);
        snprintf(res, sizeof(res), "%ux%u", stats.width, stats.height);
        snprintf(latency, sizeof(latency), "%.2f ms", stats.handoffMs);
        Tile tiles[] = {{"FPS", fps},
                        {"RESOLUTION", stats.width ? res : "-"},
                        {"GRAPHICS API", stats.api ? GraphicsApiName(stats.api) : "-"},
                        {"HANDOFF", stats.fps > 0 ? latency : "-"}};
        float gap = S(12);
        float tileWidth = (ImGui::GetContentRegionAvail().x - gap * 3) / 4;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        for (int i = 0; i < 4; ++i) {
            if (i > 0) {
                ImGui::SameLine(0.0f, gap);
            }
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImVec2 size(tileWidth, S(72));
            ImGui::Dummy(size);
            draw->AddRectFilled(p, p + size, kInset, S(8));
            draw->AddText(m_fonts.small, m_fonts.small->FontSize, p + ImVec2(S(14), S(12)),
                          kTextDim, tiles[i].label);
            draw->AddText(m_fonts.heading, m_fonts.heading->FontSize, p + ImVec2(S(14), S(32)),
                          i == 0 ? kAccent : kText, tiles[i].value.c_str());
        }

        ImU32 stateColor = stats.hookState == kHookError       ? kDanger
                           : stats.hookState == kHookCapturing ? kAccent
                                                               : kWarning;
        ImGui::PushStyleColor(ImGuiCol_Text, stateColor);
        ImGui::Text("%s  %s", stats.hookState == kHookError ? ICON_WARNING : ICON_CHECK,
                    HookStateText(stats.hookState));
        ImGui::PopStyleColor();
        if (!stats.hookStatus.empty()) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, kTextFaint);
            ImGui::TextWrapped("%s", stats.hookStatus.c_str());
            ImGui::PopStyleColor();
        }

        std::vector<std::wstring> urls = m_stream.Urls();
        if (FilledButton(ICON_CAPTURE "   Copy stream URL##stream", ImVec2(S(190), S(38)),
                         kSecondary, kSecondaryHover, kText, m_fonts.bold, S(6),
                         !urls.empty())) {
            ImGui::SetClipboardText(U8(urls.front()).c_str());
            ShowToast("Copied " + U8(urls.front()) + ". Open it in VLC on any device.");
        }
        ImGui::SameLine(0.0f, S(14));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(9));
        std::wstring streamError =
            !stats.streamError.empty() ? stats.streamError : m_stream.LastError();
        if (urls.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
            ImGui::TextUnformatted(("Stream server not running: " + U8(m_streamError)).c_str());
            ImGui::PopStyleColor();
        } else if (!streamError.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
            ImGui::TextUnformatted(("Can't stream: " + U8(streamError)).c_str());
            ImGui::PopStyleColor();
        } else {
            int viewers = m_stream.Viewers();
            ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
            ImGui::Text("%s   \xC2\xB7   %d %s", U8(urls.front()).c_str(), viewers,
                        viewers == 1 ? "viewer" : "viewers");
            ImGui::PopStyleColor();
            std::string health = StreamHealth(m_stream);
            if (!health.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, kWarning);
                ImGui::TextUnformatted((ICON_WARNING "  Weak network: " + health).c_str());
                ImGui::PopStyleColor();
            }
        }
    }
    EndCard();
}

void App::DrawGameSettingsCard(Game& game) {
    BeginCard("##settings", m_scale);
    CardTitle(m_fonts.heading, ICON_EDIT "  Game settings");
    float browseWidth = S(100);

    FieldLabel(m_fonts.small, "Name");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##name", &m_editName);
    if (ImGui::IsItemDeactivatedAfterEdit() && !m_editName.empty()) {
        game.name = Utf8ToWide(m_editName);
        SaveLibrary();
    }

    FieldLabel(m_fonts.small, "Executable");
    std::string exe = U8(game.exePath);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth - S(10));
    ImGui::InputText("##exe", &exe, ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine(0.0f, S(10));
    if (ImGui::Button("Show##exe", ImVec2(browseWidth, 0))) {
        ShowInExplorer(game.exePath);
    }

    FieldLabel(m_fonts.small, "Launch arguments");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##args", "e.g. -dx12 -windowed", &m_editArgs);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        game.arguments = Utf8ToWide(m_editArgs);
        SaveLibrary();
    }

    FieldLabel(m_fonts.small, "Working directory");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth - S(10));
    ImGui::InputTextWithHint("##workdir", "Defaults to the game's folder", &m_editWorkDir);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        game.workingDir = Utf8ToWide(m_editWorkDir);
        SaveLibrary();
    }
    ImGui::SameLine(0.0f, S(10));
    if (ImGui::Button("Browse##workdir", ImVec2(browseWidth, 0))) {
        std::vector<std::wstring> picked = PickFiles(m_hwnd, PickKind::Folder);
        if (!picked.empty()) {
            game.workingDir = picked[0];
            m_editWorkDir = U8(picked[0]);
            SaveLibrary();
        }
    }

    FieldLabel(m_fonts.small, "Launch through");
    std::string autoLabel = m_editSteamAppId.empty()
                                ? std::string("Automatic (run the exe)")
                                : "Automatic (Steam, app " + U8(m_editSteamAppId) + ")";
    const char* viaNames[] = {autoLabel.c_str(), "The game's exe", "Steam"};
    int via = int(game.launchVia);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##via", &via, viaNames, IM_ARRAYSIZE(viaNames))) {
        game.launchVia = LaunchVia(via);
        SaveLibrary();
    }
    if (game.launchVia == LaunchVia::Steam && m_editSteamAppId.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kWarning);
        ImGui::TextUnformatted(ICON_WARNING "  This exe isn't in a Steam library; it will run "
                                            "directly.");
        ImGui::PopStyleColor();
    }

    FieldLabel(m_fonts.small, "Game process");
    ImGui::SetNextItemWidth(-FLT_MIN);
    std::string processHint = "Defaults to " + U8(LaunchSpec{game.exePath}.GameProcessName()) +
                              "; set it if the exe is a launcher for another exe";
    ImGui::InputTextWithHint("##process", processHint.c_str(), &m_editProcess);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        game.processName = Utf8ToWide(m_editProcess);
        SaveLibrary();
    }

    FieldLabel(m_fonts.small, "Cover art");
    if (ImGui::Button(ICON_PICTURE "  Choose image...")) {
        std::vector<std::wstring> picked = PickFiles(m_hwnd, PickKind::Image);
        if (!picked.empty()) {
            m_textures.Forget(picked[0]);
            if (m_textures.Image(picked[0])) {
                game.coverPath = picked[0];
                SaveLibrary();
            } else {
                ShowToast("That image couldn't be loaded.", true);
            }
        }
    }
    if (!game.coverPath.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Use game icon")) {
            game.coverPath.clear();
            SaveLibrary();
        }
    }
    EndCard();
}

void App::DrawGameInfoCard(Game& game) {
    BeginCard("##info", m_scale);
    CardTitle(m_fonts.heading, ICON_INFO "  Info");
    auto row = [&](const char* label, const std::string& value) {
        FieldLabel(m_fonts.small, label);
        ImGui::TextUnformatted(value.c_str());
        ImGui::Dummy(ImVec2(0, S(2)));
    };
    row("Playtime", PlaytimeText(game.playtimeSeconds));
    row("Last played", game.lastPlayed ? DateText(game.lastPlayed) : "Never");
    row("Added", DateText(game.addedAt));
    row("Library ID", std::to_string(game.id) + "   (GazeoraCLI launch " +
                          std::to_string(game.id) + ")");

    ImGui::Dummy(ImVec2(0, S(6)));
    float width = ImGui::GetContentRegionAvail().x;
    if (FilledButton(ICON_FOLDER "   Open file location##open", ImVec2(width, S(38)), kSecondary,
                     kSecondaryHover, kText, m_fonts.bold, S(6))) {
        ShowInExplorer(game.exePath);
    }
    ImGui::Dummy(ImVec2(0, S(2)));
    if (FilledButton(ICON_DELETE "   Remove from library##remove", ImVec2(width, S(38)),
                     WithAlpha(kDanger, 0.14f), WithAlpha(kDanger, 0.26f), kDanger, m_fonts.bold,
                     S(6))) {
        m_pendingRemoveId = game.id;
    }
    EndCard();
}

void App::DrawSettings() {
    ImGui::PushFont(m_fonts.title);
    ImGui::TextUnformatted("Settings");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, S(10)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(10), S(20)));
    float width = std::min(ImGui::GetContentRegionAvail().x, S(760));

    BeginCard("##library", m_scale, width);
    CardTitle(m_fonts.heading, ICON_LIBRARY "  Library");
    FieldLabel(m_fonts.small, "Library file (shared with GazeoraCLI)");
    ImGui::TextUnformatted(U8(m_library.Path()).c_str());
    ImGui::Text("%zu games", m_library.Games().size());
    if (ImGui::Button(ICON_FOLDER "  Open folder")) {
        ShowInExplorer(FolderOf(m_library.Path()));
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_REFRESH "  Reload")) {
        m_deferred.push_back([this] {
            std::wstring error;
            if (m_library.Load(error)) {
                ShowToast("Library reloaded");
            } else {
                ShowToast(U8(error), true);
            }
            m_editId = 0;
        });
    }
    EndCard();

    BeginCard("##capture", m_scale, width);
    CardTitle(m_fonts.heading, ICON_CAPTURE "  Capture");
    std::wstring dll = HookDllPathNextToExe();
    bool found = GetFileAttributesW(dll.c_str()) != INVALID_FILE_ATTRIBUTES;
    FieldLabel(m_fonts.small, "Capture hook");
    ImGui::TextUnformatted(U8(dll).c_str());
    ImGui::PushStyleColor(ImGuiCol_Text, found ? kAccent : kDanger);
    ImGui::TextUnformatted(found ? ICON_CHECK "  Found" : ICON_WARNING "  Missing - rebuild Gazeora");
    ImGui::PopStyleColor();
    FieldLabel(m_fonts.small, "Frame stream");
    std::vector<std::wstring> urls = m_stream.Urls();
    if (!urls.empty()) {
        for (const std::wstring& url : urls) {
            std::string text = U8(url);
            ImGui::TextUnformatted(text.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(("Copy##" + text).c_str())) {
                ImGui::SetClipboardText(text.c_str());
                ShowToast("Copied " + text);
            }
        }
        DrawCodecChoice();
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::PushTextWrapPos(0.0f);
        std::wstring encoder = m_stream.EncoderName();
        ImGui::Text("Always on, %d %s. Shows the Gazeora logo while no game is captured. "
                    "Watch on any device on this network: in VLC, Media > Open Network "
                    "Stream (or ffplay, OBS). %s over RTP/UDP, or TCP for players set to "
                    "it%s%s.",
                    m_stream.Viewers(), m_stream.Viewers() == 1 ? "viewer" : "viewers",
                    CodecName(m_stream.Codec()), encoder.empty() ? "" : ", encoded by ",
                    U8(encoder).c_str());
        // VLC buffers a second of any network stream by default; that's most of the delay.
        ImGui::TextUnformatted(
            "Delay: VLC buffers 1 second by default. On a wired PC, 0-100 ms of network caching "
            "is fine. Phones on Wi-Fi need some slack or the picture stalls: start around "
            "300 ms. PC: Open Network Stream > Show more options > Caching. Android: Settings > "
            "Advanced > Network caching value. iOS: Settings > Network Caching Level > Low "
            "Latency (Lowest Latency stalls on most Wi-Fi).");
        ImGui::TextUnformatted(
            "On a weak link the stream lowers its bitrate, and raises it again once the link is "
            "clean. A lost packet smears the picture for under a second. A 5 GHz Wi-Fi network, "
            "and the PC on a cable, help most.");
        ImGui::TextUnformatted(
            "Phone smearing or stopping over Wi-Fi? Turn on RTSP over TCP in the player (VLC "
            "for iOS: the RTSP over TCP switch in Settings). Nothing is lost then; a slow link "
            "skips frames instead.");
        ImGui::TextUnformatted(("Log of what players did: " + U8(StreamLogPath())).c_str());
        std::string health = StreamHealth(m_stream);
        if (!health.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, kWarning);
            ImGui::TextUnformatted((ICON_WARNING "  Now: " + health).c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
        ImGui::TextUnformatted((ICON_WARNING "  Not running: " + U8(m_streamError)).c_str());
        ImGui::PopStyleColor();
    }
    EndCard();

    DrawVrCard(width);

    BeginCard("##about", m_scale, width);
    CardTitle(m_fonts.heading, ICON_INFO "  About");
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextUnformatted("Gazeora game launcher and frame capture.");
    ImGui::Text("Command line: GazeoraCLI.exe list | add | remove | launch | attach");
    ImGui::Text("Dear ImGui %s", IMGUI_VERSION);
    ImGui::PopStyleColor();
    EndCard();
    ImGui::PopStyleVar();
}

void App::DrawToasts() {
    // Remove confirmation modal (opened from details or a context menu).
    if (m_pendingRemoveId != 0 && !ImGui::IsPopupOpen("Remove game?")) {
        ImGui::OpenPopup("Remove game?");
    }
    ImGui::SetNextWindowSize(ImVec2(S(420), 0));
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(24), S(20)));
    if (ImGui::BeginPopupModal("Remove game?", nullptr,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoTitleBar)) {
        const Game* game = m_library.Find(m_pendingRemoveId);
        ImGui::PushFont(m_fonts.heading);
        ImGui::TextUnformatted("Remove from library?");
        ImGui::PopFont();
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::Text("%s will be removed from Gazeora. The game itself stays installed.",
                    game ? U8(game->name).c_str() : "This game");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, S(6)));
        float buttonWidth = S(110);
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - S(24) - buttonWidth * 2 - S(10));
        if (FilledButton("Cancel##remove", ImVec2(buttonWidth, S(38)), kSecondary,
                         kSecondaryHover, kText, m_fonts.bold, S(6)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_pendingRemoveId = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0.0f, S(10));
        if (FilledButton("Remove##remove", ImVec2(buttonWidth, S(38)), kDanger,
                         kDangerHover, IM_COL32_WHITE, m_fonts.bold,
                         S(6))) {
            uint32_t id = m_pendingRemoveId;
            m_deferred.push_back([this, id] { RemoveGame(id); });
            m_pendingRemoveId = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    // Notifications stacked in the bottom-right corner.
    double now = ImGui::GetTime();
    m_toasts.erase(std::remove_if(m_toasts.begin(), m_toasts.end(),
                                  [now](const Toast& toast) { return toast.expiresAt < now; }),
                   m_toasts.end());
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImVec2 corner = ImGui::GetMainViewport()->WorkPos + ImGui::GetMainViewport()->WorkSize -
                    ImVec2(S(24), S(24));
    float maxTextWidth = S(380);
    for (auto it = m_toasts.rbegin(); it != m_toasts.rend(); ++it) {
        float fade = float(std::min(1.0, (it->expiresAt - now) / 0.4));
        ImVec2 textSize = m_fonts.body->CalcTextSizeA(m_fonts.body->FontSize, FLT_MAX,
                                                      maxTextWidth, it->text.c_str());
        ImVec2 size = textSize + ImVec2(S(56), S(28));
        ImVec2 pos = corner - size;
        draw->AddRectFilled(pos, pos + size, WithAlpha(kCardHover, 0.97f * fade),
                            S(8));
        draw->AddRectFilled(pos, pos + ImVec2(S(4), size.y),
                            WithAlpha(it->error ? kDanger : kAccent, fade), S(8),
                            ImDrawFlags_RoundCornersLeft);
        draw->AddText(m_fonts.body, m_fonts.body->FontSize, pos + ImVec2(S(18), S(14)),
                      WithAlpha(it->error ? kDanger : kAccent, fade),
                      it->error ? ICON_WARNING : ICON_CHECK);
        draw->AddText(m_fonts.body, m_fonts.body->FontSize, pos + ImVec2(S(42), S(14)),
                      WithAlpha(kText, fade), it->text.c_str(), nullptr, maxTextWidth);
        corner.y -= size.y + S(10);
    }
}

void App::AddGamesFromDialog() { AddGamePaths(PickFiles(m_hwnd, PickKind::Games)); }

void App::AddGamePaths(const std::vector<std::wstring>& paths) {
    uint32_t lastAdded = 0;
    int added = 0;
    for (const std::wstring& path : paths) {
        Game game;
        if (EndsWithI(path, L".lnk")) {
            if (!ResolveShortcut(path, game)) {
                ShowToast("Skipped a shortcut that doesn't point at a program", true);
                continue;
            }
        } else if (EndsWithI(path, L".exe")) {
            game.exePath = path;
        } else {
            ShowToast("Only .exe files and shortcuts can be added", true);
            continue;
        }
        bool duplicate = false;
        for (const Game& existing : m_library.Games()) {
            duplicate = duplicate || (_wcsicmp(existing.exePath.c_str(), game.exePath.c_str()) == 0 &&
                                      existing.arguments == game.arguments);
        }
        if (duplicate) {
            ShowToast(U8(DefaultGameName(game.exePath)) + " is already in your library");
            continue;
        }
        lastAdded = m_library.Add(game);
        ++added;
    }
    if (added == 0) {
        return;
    }
    SaveLibrary();
    if (added == 1) {
        ShowToast("Added " + U8(m_library.Find(lastAdded)->name));
        OpenDetails(lastAdded);
    } else {
        ShowToast("Added " + std::to_string(added) + " games");
        m_page = Page::Library;
    }
}

void App::DrawCodecChoice() {
    struct Choice {
        VideoCodec codec;
        const char* name;
    };
    static const Choice kChoices[] = {
        {VideoCodec::H264, "H.264 - plays on every device"},
        {VideoCodec::H265, "H.265 - sharper on weak Wi-Fi, newer devices only"},
    };
    VideoCodec current = m_stream.Codec();
    FieldLabel(m_fonts.small, "Video codec");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##codec", kChoices[current == VideoCodec::H265].name)) {
        for (const Choice& choice : kChoices) {
            bool available = m_stream.CodecAvailable(choice.codec);
            std::string label = choice.name;
            if (!available) label += " (no encoder on this PC)";
            if (ImGui::Selectable(label.c_str(), choice.codec == current,
                                  available ? 0 : ImGuiSelectableFlags_Disabled) &&
                choice.codec != current) {
                bool watched = m_stream.Viewers() > 0;
                m_stream.SetCodec(choice.codec);
                SaveStreamCodec(choice.codec);
                ShowToast(std::string("Streaming ") + CodecName(choice.codec) +
                          (watched ? ". Players were disconnected: open the stream again." : "."));
            }
        }
        ImGui::EndCombo();
    }
}

void App::DrawVrCard(float width) {
    BeginCard("##vr", m_scale, width);
    CardTitle(m_fonts.heading, ICON_GAME "  VR headset");
    bool changed = false;
    // Sliders save when let go; the stream follows every step of the drag.
    auto slider = [&](const char* label, const char* id, float* value, float low, float high,
                      const char* format) {
        FieldLabel(m_fonts.small, label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderFloat(id, value, low, high, format)) {
            m_stream.SetVrLayout(m_vr);
            m_logoDirty = true;
        }
        changed |= ImGui::IsItemDeactivatedAfterEdit();
    };

    changed |= ImGui::Checkbox("Side by side for a phone in a VR viewer", &m_vr.enabled);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(
        "Shows the picture once per eye and pre-distorts it for the viewer's lenses. Play the "
        "stream full screen in landscape on the phone. Changes reach the stream right away, so "
        "you can tune them with the headset on.");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    if (m_vr.enabled) {
        int lens = IM_ARRAYSIZE(kLensPresets);  // custom
        for (int i = 0; i < IM_ARRAYSIZE(kLensPresets); ++i) {
            if (kLensPresets[i].k1 == m_vr.k1 && kLensPresets[i].k2 == m_vr.k2) lens = i;
        }
        FieldLabel(m_fonts.small, "Viewer lenses");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##vrlens", lens < IM_ARRAYSIZE(kLensPresets)
                                              ? kLensPresets[lens].name
                                              : "Custom")) {
            for (int i = 0; i < IM_ARRAYSIZE(kLensPresets); ++i) {
                if (ImGui::Selectable(kLensPresets[i].name, i == lens)) {
                    m_vr.k1 = kLensPresets[i].k1;
                    m_vr.k2 = kLensPresets[i].k2;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }

        int screen = -1;
        for (int i = 0; i < IM_ARRAYSIZE(kScreenPresets); ++i) {
            if (fabsf(kScreenPresets[i].aspect - m_vr.screenAspect) < 0.01f) screen = i;
        }
        char custom[32];
        snprintf(custom, sizeof(custom), "%.2f:1", m_vr.screenAspect);
        FieldLabel(m_fonts.small, "Phone screen shape (landscape)");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##vrscreen", screen >= 0 ? kScreenPresets[screen].name : custom)) {
            for (int i = 0; i < IM_ARRAYSIZE(kScreenPresets); ++i) {
                if (ImGui::Selectable(kScreenPresets[i].name, i == screen)) {
                    m_vr.screenAspect = kScreenPresets[i].aspect;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }

        slider("Barrel distortion (k1)", "##vrk1", &m_vr.k1, 0.0f, 1.0f, "%.3f");
        slider("Edge distortion (k2)", "##vrk2", &m_vr.k2, 0.0f, 1.0f, "%.3f");
        slider("Picture size", "##vrscale", &m_vr.imageScale, 0.3f, 1.5f, "%.2f");
        slider("Lens separation (share of the screen width)", "##vrsep", &m_vr.lensSeparation,
               0.3f, 0.7f, "%.3f");
        slider("Color fringe correction", "##vrchroma", &m_vr.chromaCorrection, 0.0f, 0.05f,
               "%.3f");
        if (ImGui::Button(ICON_REFRESH "  Reset VR settings")) {
            m_vr = VrLayout();
            m_vr.enabled = true;
            changed = true;
        }

        if (m_logoPreview) {
            FieldLabel(m_fonts.small, "Preview of the idle frame");
            float previewWidth = ImGui::GetContentRegionAvail().x;
            ImGui::Image(reinterpret_cast<ImTextureID>(m_logoPreview.Get()),
                         ImVec2(previewWidth, previewWidth / m_logoPreviewAspect));
        }
    }
    if (changed) {
        ApplyVrLayout();
    }
    EndCard();
}

void App::ApplyVrLayout() {
    m_stream.SetVrLayout(m_vr);
    SaveVrLayout(m_vr);
    m_logoDirty = true;
}

void App::UpdateLogoFrame() {
    std::vector<uint8_t> logo = RenderLogoFrame(kLogoFrameWidth, kLogoFrameHeight);
    uint32_t width = kLogoFrameWidth, height = kLogoFrameHeight;
    if (logo.empty()) {
        return;
    }

    // Through the same GPU pass as game frames, so the logo matches them in the headset.
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data = {logo.data(), width * 4, 0};
    ComPtr<ID3D11Texture2D> texture;
    if (m_vr.enabled && SUCCEEDED(m_device->CreateTexture2D(&desc, &data, &texture))) {
        TextureReader reader;
        reader.SetMaxSize(FrameStreamServer::kMaxWidth, FrameStreamServer::kMaxHeight);
        reader.SetVrLayout(m_vr);
        std::vector<uint8_t> warped;
        UINT warpedWidth, warpedHeight;
        std::wstring error;
        if (reader.Read(m_device.Get(), texture.Get(), warpedWidth, warpedHeight, warped,
                        error)) {
            logo.swap(warped);
            width = warpedWidth;
            height = warpedHeight;
        }
    }

    // The preview shows exactly what goes out.
    m_logoPreview.Reset();
    desc.Width = width;
    desc.Height = height;
    data = {logo.data(), width * 4, 0};
    if (SUCCEEDED(m_device->CreateTexture2D(&desc, &data, texture.ReleaseAndGetAddressOf()))) {
        m_device->CreateShaderResourceView(texture.Get(), nullptr, &m_logoPreview);
        m_logoPreviewAspect = float(width) / float(height);
    }
    if (m_stream.Running()) {
        m_stream.SetIdleFrame(width, height, std::move(logo));
    }
}

void App::Launch(uint32_t gameId, bool capture) {
    Game* game = m_library.Find(gameId);
    if (!game) {
        return;
    }
    if (SessionFor(gameId)) {
        ShowToast(U8(game->name) + " is already running");
        return;
    }
    m_sessions.push_back(std::make_unique<GameSession>(gameId, ToLaunchSpec(*game), capture,
                                                       &m_stream));
    game->lastPlayed = UnixNow();
    SaveLibrary();
    ShowToast("Launching " + U8(game->name) + (capture ? " with capture" : ""));
}

void App::RemoveGame(uint32_t gameId) {
    Game* game = m_library.Find(gameId);
    if (!game) {
        return;
    }
    std::string name = U8(game->name);
    m_library.Remove(gameId);
    SaveLibrary();
    ShowToast("Removed " + name);
    if (m_page == Page::Details && m_detailsId == gameId) {
        m_page = Page::Library;
    }
}

void App::OpenDetails(uint32_t gameId) {
    m_detailsId = gameId;
    m_page = Page::Details;
}

void App::SaveLibrary() {
    std::wstring error;
    if (!m_library.Save(error)) {
        ShowToast(U8(error), true);
    }
}

void App::ShowToast(const std::string& text, bool error) {
    double now = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    m_toasts.push_back({text, error, now + (error ? 6.0 : 3.0)});
    if (m_toasts.size() > 4) {
        m_toasts.erase(m_toasts.begin());
    }
}

GameSession* App::SessionFor(uint32_t gameId) const {
    for (const auto& session : m_sessions) {
        if (session->GameId() == gameId && !session->Finished()) {
            return session.get();
        }
    }
    return nullptr;
}

uint64_t App::AccentGradient() { return m_textures.Gradient(false).Id(); }

const Texture& App::ArtFor(const Game& game) {
    if (!game.coverPath.empty()) {
        const Texture& cover = m_textures.Image(game.coverPath);
        if (cover) {
            return cover;
        }
    }
    return m_textures.Icon(game.exePath);
}

std::vector<const Game*> App::SortedGames(Sort sort, const std::string& filter) const {
    std::string needle = Lower(filter);
    std::vector<const Game*> games;
    for (const Game& game : m_library.Games()) {
        if (needle.empty() || Lower(U8(game.name)).find(needle) != std::string::npos) {
            games.push_back(&game);
        }
    }
    std::stable_sort(games.begin(), games.end(), [sort](const Game* a, const Game* b) {
        switch (sort) {
            case Sort::Name:
                return _wcsicmp(a->name.c_str(), b->name.c_str()) < 0;
            case Sort::RecentlyAdded:
                return a->addedAt > b->addedAt || (a->addedAt == b->addedAt && a->id > b->id);
            case Sort::RecentlyPlayed:
            default:
                if (a->lastPlayed != b->lastPlayed) {
                    return a->lastPlayed > b->lastPlayed;
                }
                return a->addedAt > b->addedAt || (a->addedAt == b->addedAt && a->id > b->id);
        }
    });
    return games;
}

}  // namespace gazeora::app
