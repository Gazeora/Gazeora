#include "gazeora/input/gestures/air_box.h"

#include <algorithm>

namespace gazeora::input {

namespace {

Vec2 Clamped(Vec2 position) {
    return {std::clamp(position.x, 0.0f, 1.0f), std::clamp(position.y, 0.0f, 1.0f)};
}

Vec2 Scaled(Vec2 value, Vec2 scale) { return {value.x * scale.x, value.y * scale.y}; }

}  // namespace

AirBox::AirBox(Vec2 tip, Vec2 screenPosition, float widthPalms, float screenAspect)
    : m_size{widthPalms, widthPalms / screenAspect} {
    m_topLeft = tip - Scaled(Clamped(screenPosition), m_size);
}

Vec2 AirBox::ToScreen(Vec2 tip) const {
    Vec2 inside = tip - m_topLeft;
    return {inside.x / m_size.x, inside.y / m_size.y};
}

Vec2 AirBox::ScreensFor(Vec2 palms) const { return {palms.x / m_size.x, palms.y / m_size.y}; }

void AirBox::KeepOnScreen(Vec2 tip) {
    Vec2 screen = ToScreen(tip);
    m_topLeft = m_topLeft + Scaled(screen - Clamped(screen), m_size);
}

void AirBox::Resize(float widthPalms, Vec2 tip) {
    Vec2 screen = ToScreen(tip);
    m_size = m_size * (widthPalms / m_size.x);
    m_topLeft = tip - Scaled(screen, m_size);
}

}  // namespace gazeora::input
