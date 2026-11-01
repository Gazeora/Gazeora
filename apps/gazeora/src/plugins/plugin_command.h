// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::app {

// Gazeora.exe run with one of these, as an administrator (ElevatedRun), changes the plugins
// folder next to it and exits:
//   --install-plugin <pipe> <id> <source.dll> <sha256>
//   --uninstall-plugin <pipe> <file.dll>
constexpr wchar_t kInstallPluginArgument[] = L"--install-plugin";
constexpr wchar_t kUninstallPluginArgument[] = L"--uninstall-plugin";

// For main: when the command line asks for one of the above, does it, writes what went wrong
// to the pipe and returns the exit code. -1 when the command line doesn't ask.
class PluginCommand final {
public:
    PluginCommand() = delete;
    static int Run();
};

}  // namespace gazeora::app
