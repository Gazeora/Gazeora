# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# cmake -P TextToRtf.cmake: the license page's text (WiX shows only RTF there). Takes INPUTS, the
# plain-text files in order separated by | (a ; wouldn't survive the command line), and OUTPUT.
# Lines wrapped by hand are joined so the text flows in the dialog; blank lines and list items
# still start new lines. The inputs are ASCII.

set(rtf "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Segoe UI;}}\\fs16\n")
string(REPLACE "|" ";" inputs "${INPUTS}")
foreach(input IN LISTS inputs)
    file(READ "${input}" text)
    string(REPLACE "\r\n" "\n" text "${text}")
    string(REPLACE "\\" "\\\\" text "${text}")
    string(REPLACE "{" "\\{" text "${text}")
    string(REPLACE "}" "\\}" text "${text}")
    # Paragraphs and list items keep their breaks; other line breaks become spaces.
    string(REGEX REPLACE "\n[ \t]*\n" "@PAR@" text "${text}")
    string(REGEX REPLACE "\n([ \t]*([0-9]+|[a-z])\\. )" "@LINE@\\1" text "${text}")
    string(REGEX REPLACE "\n[ \t]*" " " text "${text}")
    string(REPLACE "@PAR@" "\\par\\par\n" text "${text}")
    string(REPLACE "@LINE@" "\\line\n" text "${text}")
    string(APPEND rtf "${text}\\par\\par\\page\n")
endforeach()
string(APPEND rtf "}\n")
file(WRITE "${OUTPUT}" "${rtf}")
