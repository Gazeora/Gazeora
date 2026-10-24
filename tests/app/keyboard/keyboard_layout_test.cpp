#include "keyboard/keyboard_layout.h"

#include <gtest/gtest.h>

namespace gazeora::app {
namespace {

constexpr float kRowWidth = 10.0f;

void ExpectRowsTenKeysWide(const KeyboardLayout& layout) {
    for (int row = 0; row < layout.RowCount(); ++row) {
        float width = 0.0f;
        for (int column = 0; column < layout.ColumnCount(row); ++column) {
            width += layout.Key(row, column).Span();
        }
        EXPECT_FLOAT_EQ(width, kRowWidth) << "row " << row;
    }
}

TEST(KeyboardLayoutTest, EveryRowIsTenKeysWide) {
    ExpectRowsTenKeysWide(KeyboardLayout::Letters());
    ExpectRowsTenKeysWide(KeyboardLayout::Symbols());
}

TEST(KeyboardLayoutTest, BothLayoutsHaveTheSameRows) {
    EXPECT_EQ(KeyboardLayout::Letters().RowCount(), KeyboardLayout::Symbols().RowCount());
}

TEST(KeyboardLayoutTest, LettersShowCapitalsWithShift) {
    KeyboardLayout letters = KeyboardLayout::Letters();
    EXPECT_EQ(letters.Key(1, 0).Label(false), "q");
    EXPECT_EQ(letters.Key(1, 0).Label(true), "Q");
    EXPECT_EQ(letters.Key(0, 0).Label(true), "!");
}

TEST(KeyboardLayoutTest, SymbolsHaveWhatPathsNeed) {
    KeyboardLayout symbols = KeyboardLayout::Symbols();
    std::string all;
    for (int row = 0; row < symbols.RowCount(); ++row) {
        for (int column = 0; column < symbols.ColumnCount(row); ++column) {
            all += symbols.Key(row, column).Label(false);
        }
    }
    for (char needed : std::string("\\/:._-\"")) {
        EXPECT_NE(all.find(needed), std::string::npos) << needed;
    }
}

}  // namespace
}  // namespace gazeora::app
