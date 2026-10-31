// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "report/windows_description.h"

#include <gtest/gtest.h>

#include <regex>

namespace gazeora::app {
namespace {

TEST(WindowsDescriptionTest, NamesTheVersionProcessorAndBuild) {
    EXPECT_TRUE(std::regex_match(DescribeWindows(),
                                 std::regex(R"(Windows \d+\.\d+\.\d+, (x64|ARM64|x86) PC, )"
                                            R"((x64|ARM64|x86) build)")));
}

}  // namespace
}  // namespace gazeora::app
