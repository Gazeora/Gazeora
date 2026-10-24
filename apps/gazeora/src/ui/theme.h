// The launcher's look: colors taken from the app icon (navy surfaces, and a violet accent
// that fades to cyan on primary actions) and the icon glyphs merged into every text font.
#pragma once

#include <imgui.h>

#include <string>

namespace gazeora::app {

inline const ImU32 kAccent = IM_COL32(124, 92, 255, 255);
inline const ImU32 kAccentHover = IM_COL32(146, 120, 255, 255);
inline const ImU32 kAccentEnd = IM_COL32(56, 189, 248, 255);  // far end of the accent gradient
inline const ImU32 kOnAccent = IM_COL32(255, 255, 255, 255);
inline const ImU32 kWindowBg = IM_COL32(18, 20, 30, 255);
inline const ImU32 kSidebarBg = IM_COL32(13, 14, 22, 255);
inline const ImU32 kCardBg = IM_COL32(27, 30, 44, 255);
inline const ImU32 kCardHover = IM_COL32(35, 39, 56, 255);
inline const ImU32 kInset = IM_COL32(20, 22, 33, 255);  // tiles inside cards
inline const ImU32 kSecondary = IM_COL32(42, 46, 66, 255);
inline const ImU32 kSecondaryHover = IM_COL32(54, 59, 82, 255);
inline const ImU32 kBorder = IM_COL32(52, 57, 80, 255);
inline const ImU32 kDisabled = IM_COL32(40, 43, 60, 255);
inline const ImU32 kText = IM_COL32(236, 238, 246, 255);
inline const ImU32 kTextDim = IM_COL32(152, 158, 182, 255);
inline const ImU32 kTextFaint = IM_COL32(100, 106, 130, 255);
inline const ImU32 kDanger = IM_COL32(244, 86, 112, 255);
inline const ImU32 kDangerHover = IM_COL32(250, 112, 136, 255);
inline const ImU32 kWarning = IM_COL32(250, 190, 70, 255);
inline const ImU32 kSuccess = IM_COL32(74, 222, 128, 255);

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
#define ICON_HAND u8""  // TouchPointer

// Every glyph above, for building the font atlas.
inline const char kAllIcons[] = ICON_HOME ICON_LIBRARY ICON_SETTINGS ICON_ADD ICON_PLAY ICON_BACK
    ICON_DELETE ICON_FOLDER ICON_PICTURE ICON_CAMERA ICON_SEARCH ICON_GAME ICON_REFRESH ICON_CAPTURE
        ICON_CLOCK ICON_INFO ICON_CHECK ICON_WARNING ICON_EDIT ICON_HAND;

ImU32 WithAlpha(ImU32 color, float alpha);
ImU32 Shade(const float rgb[3], float brightness, float alpha = 1.0f);
// A stable color per name, for games whose exe has no icon.
void NameColor(const std::wstring& name, float rgb[3]);

}  // namespace gazeora::app
