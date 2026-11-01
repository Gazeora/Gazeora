// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace gazeora::app {

// Runs Gazeora.exe again as an administrator for one task, and carries back what went wrong.
// The elevated run's command line is: <command> <pipe> <arguments...>, where <pipe> is the
// write end of a pipe in the process that asked. A file named on the command line would let
// anything that starts Gazeora have an administrator write wherever it liked.
class ElevatedRun final {
public:
    ElevatedRun() = delete;

    // Starts it (Windows asks the user first) and waits. False with `error` set if it failed or
    // the user said no. `what` names the task in errors, e.g. L"The driver install".
    static bool Run(HWND owner, const wchar_t* command, const std::vector<std::wstring>& arguments,
                    const wchar_t* what, std::wstring& error);
    // In the elevated run: writes `text` to the pipe whose handle number is `pipe`, in the
    // process that started this one. Only a pipe, and only out of that process: the number
    // comes from the command line, which anything could have written.
    static void ReportToParent(const std::wstring& pipe, const std::wstring& text);
    // In the elevated run, first: lets the process that started it come back to the front.
    // Windows hands the foreground to the elevated run once the user agrees, and it has no
    // window, so without this the foreground goes to whichever window is next when it exits.
    static void HandForegroundToParent();

private:
    // In the process that asked: brings `owner` back to the front once the run is done.
    static void BringBack(HWND owner);
    static std::wstring Quoted(const std::vector<std::wstring>& arguments);
    static std::wstring ReadPipe(HANDLE pipe);
    static DWORD ParentProcessId();
};

}  // namespace gazeora::app
