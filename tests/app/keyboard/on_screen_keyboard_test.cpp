#include "keyboard/on_screen_keyboard.h"

#include <gtest/gtest.h>

#include <string>

namespace gazeora::app {
namespace {

using input::GamepadButton;
using input::GamepadState;

// Remembers what was typed into it.
class RecordingSink final : public ITextSink {
public:
    void Type(char32_t character) override { text += char(character); }
    void Erase() override {
        if (!text.empty()) {
            text.pop_back();
        }
    }
    void Finish() override { finished = true; }

    std::string text;
    bool finished = false;
};

class OnScreenKeyboardTest : public ::testing::Test {
protected:
    void SetUp() override { m_keyboard.Open(); }

    // Presses `button` and lets go of it, a frame each.
    void Tap(GamepadButton button) {
        GamepadState pressed;
        pressed.SetButton(button, true);
        Frame({pressed});
        Frame({GamepadState()});
    }

    void Frame(const std::vector<GamepadState>& states) {
        m_presses.Update(states);
        m_keyboard.Update(m_presses, m_now);
        m_now += kFrameSeconds;
    }

    std::string Chosen() const {
        return m_keyboard.Layout().Key(m_keyboard.Row(), m_keyboard.Column()).Label(false);
    }

    static constexpr double kFrameSeconds = 1.0 / 60.0;

    RecordingSink m_sink;
    OnScreenKeyboard m_keyboard{m_sink};
    GamepadPresses m_presses;
    double m_now = 0.0;
};

TEST_F(OnScreenKeyboardTest, OpensAtQAndTypesIt) {
    EXPECT_EQ(Chosen(), "q");
    Tap(GamepadButton::South);
    EXPECT_EQ(m_sink.text, "q");
}

TEST_F(OnScreenKeyboardTest, MovesAcrossARowAndWraps) {
    Tap(GamepadButton::DpadLeft);
    EXPECT_EQ(Chosen(), "p");
    Tap(GamepadButton::DpadRight);
    EXPECT_EQ(Chosen(), "q");
}

TEST_F(OnScreenKeyboardTest, MovesToTheKeyStraightBelow) {
    for (int i = 0; i < 6; ++i) {
        Tap(GamepadButton::DpadRight);  // to u
    }
    Tap(GamepadButton::DpadDown);
    Tap(GamepadButton::DpadDown);  // past j to b: the row below starts with the wide Shift
    EXPECT_EQ(Chosen(), "b");
    Tap(GamepadButton::DpadDown);  // the wide key under b
    EXPECT_EQ(Chosen(), "Delete");
}

TEST_F(OnScreenKeyboardTest, ShiftCapitalizesOneCharacter) {
    Tap(GamepadButton::LeftShoulder);
    Tap(GamepadButton::South);
    Tap(GamepadButton::South);
    EXPECT_EQ(m_sink.text, "Qq");
}

TEST_F(OnScreenKeyboardTest, ShortcutsEraseAndAddSpaces) {
    Tap(GamepadButton::South);
    Tap(GamepadButton::North);
    Tap(GamepadButton::South);
    Tap(GamepadButton::West);
    EXPECT_EQ(m_sink.text, "q ");
}

TEST_F(OnScreenKeyboardTest, RightShoulderSwitchesToSymbols) {
    Tap(GamepadButton::RightShoulder);
    EXPECT_EQ(Chosen(), "!");
    Tap(GamepadButton::South);
    EXPECT_EQ(m_sink.text, "!");
}

TEST_F(OnScreenKeyboardTest, TheRightButtonFinishes) {
    Tap(GamepadButton::East);
    EXPECT_TRUE(m_sink.finished);
    EXPECT_FALSE(m_keyboard.IsOpen());
}

TEST_F(OnScreenKeyboardTest, AHeldDirectionRepeats) {
    GamepadState held;
    held.SetButton(GamepadButton::DpadRight, true);
    for (int i = 0; i < 60; ++i) {  // a second
        Frame({held});
    }
    EXPECT_NE(Chosen(), "w");  // well past the next key
}

}  // namespace
}  // namespace gazeora::app
