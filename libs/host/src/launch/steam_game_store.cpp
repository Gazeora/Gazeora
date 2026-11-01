// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/launch/steam_game_store.h"

#include <windows.h>
#include <shellapi.h>

#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gazeora/common/text/utf8.h"

namespace gazeora {

namespace {

constexpr wchar_t kCommonMarker[] = L"\\steamapps\\common\\";
constexpr wchar_t kSteamApps[] = L"\\steamapps";
constexpr wchar_t kManifestPattern[] = L"\\appmanifest_*.acf";
constexpr wchar_t kRunGameUri[] = L"steam://rungameid/";
constexpr char kAppIdKey[] = "appid";
constexpr char kInstallDirKey[] = "installdir";
// ShellExecute returns a value above this on success.
constexpr INT_PTR kShellExecuteFailedMax = 32;

// The quoted tokens of one line of a Steam .acf/.vdf file: "key"  "value".
std::vector<std::string> QuotedTokens(const std::string& line) {
    std::vector<std::string> tokens;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] != '"') {
            continue;
        }
        std::string token;
        for (++i; i < line.size() && line[i] != '"'; ++i) {
            if (line[i] == '\\' && i + 1 < line.size()) {
                ++i;  // \\ and \" escapes
            }
            token += line[i];
        }
        tokens.push_back(token);
    }
    return tokens;
}

std::wstring LowerWithBackslashes(std::wstring path) {
    for (wchar_t& c : path) {
        c = c == L'/' ? L'\\' : wchar_t(towlower(c));
    }
    return path;
}

}  // namespace

std::wstring SteamGameStore::AppIdFor(const std::wstring& exePath) const {
    std::wstring steamapps, installDir;
    if (!SplitLibraryPath(exePath, steamapps, installDir)) {
        return {};
    }
    WIN32_FIND_DATAW found;
    HANDLE find = FindFirstFileW((steamapps + kManifestPattern).c_str(), &found);
    if (find == INVALID_HANDLE_VALUE) {
        return {};
    }
    std::wstring result;
    do {
        result = ManifestAppId(steamapps + L"\\" + found.cFileName, installDir);
    } while (result.empty() && FindNextFileW(find, &found));
    FindClose(find);
    return result;
}

bool SteamGameStore::SplitLibraryPath(const std::wstring& exePath, std::wstring& steamapps,
                                      std::wstring& installDir) {
    // ...\steamapps\common\<installdir>\...\game.exe
    const std::wstring lower = LowerWithBackslashes(exePath);
    const std::wstring marker = kCommonMarker;
    size_t at = lower.find(marker);
    if (at == std::wstring::npos) {
        return false;
    }
    steamapps = exePath.substr(0, at + wcslen(kSteamApps));
    size_t dirStart = at + marker.size();
    size_t dirEnd = lower.find(L'\\', dirStart);
    installDir = exePath.substr(dirStart, dirEnd - dirStart);
    return true;
}

std::wstring SteamGameStore::ManifestAppId(const std::wstring& manifestPath,
                                           const std::wstring& installDir) {
    std::ifstream file{std::filesystem::path(manifestPath)};
    std::string line, appId, dir;
    while (std::getline(file, line)) {
        std::vector<std::string> tokens = QuotedTokens(line);
        if (tokens.size() == 2 && tokens[0] == kAppIdKey) {
            appId = tokens[1];
        } else if (tokens.size() == 2 && tokens[0] == kInstallDirKey) {
            dir = tokens[1];
        }
    }
    bool matches = !appId.empty() && _wcsicmp(Utf8::ToWide(dir).c_str(), installDir.c_str()) == 0;
    return matches ? Utf8::ToWide(appId) : std::wstring();
}

bool SteamGameStore::Launch(const std::wstring& appId, std::wstring& error) const {
    std::wstring uri = kRunGameUri + appId;
    auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= kShellExecuteFailedMax) {
        error = L"could not start the game through Steam (is Steam installed?), error " +
                std::to_wstring(result);
        return false;
    }
    return true;
}

}  // namespace gazeora
