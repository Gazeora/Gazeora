#include "gazeora/library/game_library.h"

#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <ctime>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

using Microsoft::WRL::ComPtr;
using nlohmann::json;

namespace gazeora {

namespace {

constexpr int kLibraryVersion = 1;

const char* LaunchViaName(LaunchVia via) {
    switch (via) {
        case LaunchVia::Direct:
            return "direct";
        case LaunchVia::Steam:
            return "steam";
        case LaunchVia::Auto:
        default:
            return "auto";
    }
}

LaunchVia LaunchViaFromName(const std::string& name) {
    if (name == "direct") return LaunchVia::Direct;
    if (name == "steam") return LaunchVia::Steam;
    return LaunchVia::Auto;
}

json GameToJson(const Game& game) {
    return json{
        {"id", game.id},
        {"name", WideToUtf8(game.name)},
        {"exePath", WideToUtf8(game.exePath)},
        {"arguments", WideToUtf8(game.arguments)},
        {"workingDir", WideToUtf8(game.workingDir)},
        {"coverPath", WideToUtf8(game.coverPath)},
        {"captureOnLaunch", game.captureOnLaunch},
        {"launchVia", LaunchViaName(game.launchVia)},
        {"processName", WideToUtf8(game.processName)},
        {"addedAt", game.addedAt},
        {"lastPlayed", game.lastPlayed},
        {"playtimeSeconds", game.playtimeSeconds},
    };
}

Game GameFromJson(const json& j) {
    Game game;
    game.id = j.value("id", 0u);
    game.name = Utf8ToWide(j.value("name", ""));
    game.exePath = Utf8ToWide(j.value("exePath", ""));
    game.arguments = Utf8ToWide(j.value("arguments", ""));
    game.workingDir = Utf8ToWide(j.value("workingDir", ""));
    game.coverPath = Utf8ToWide(j.value("coverPath", ""));
    game.captureOnLaunch = j.value("captureOnLaunch", false);
    game.launchVia = LaunchViaFromName(j.value("launchVia", "auto"));
    game.processName = Utf8ToWide(j.value("processName", ""));
    game.addedAt = j.value("addedAt", int64_t(0));
    game.lastPlayed = j.value("lastPlayed", int64_t(0));
    game.playtimeSeconds = j.value("playtimeSeconds", int64_t(0));
    return game;
}

std::wstring FileStem(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    std::wstring file = slash == std::wstring::npos ? path : path.substr(slash + 1);
    size_t dot = file.find_last_of(L'.');
    return dot == std::wstring::npos ? file : file.substr(0, dot);
}

std::wstring VersionFileDescription(const std::wstring& exePath) {
    DWORD size = GetFileVersionInfoSizeW(exePath.c_str(), nullptr);
    if (size == 0) {
        return {};
    }
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(exePath.c_str(), 0, size, data.data())) {
        return {};
    }
    struct Translation {
        WORD language;
        WORD codePage;
    }* translations = nullptr;
    UINT bytes = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
                        reinterpret_cast<void**>(&translations), &bytes) ||
        bytes < sizeof(Translation)) {
        return {};
    }
    wchar_t key[64];
    swprintf(key, 64, L"\\StringFileInfo\\%04x%04x\\FileDescription", translations[0].language,
             translations[0].codePage);
    wchar_t* value = nullptr;
    UINT chars = 0;
    if (!VerQueryValueW(data.data(), key, reinterpret_cast<void**>(&value), &chars) || chars <= 1) {
        return {};
    }
    std::wstring description(value, chars - 1);
    // Trim the whitespace some publishers leave around it.
    size_t first = description.find_first_not_of(L" \t");
    size_t last = description.find_last_not_of(L" \t");
    return first == std::wstring::npos ? std::wstring()
                                       : description.substr(first, last - first + 1);
}

bool IEquals(const std::wstring& a, const std::wstring& b) {
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

}  // namespace

std::wstring GameLibrary::DefaultPath() {
    wchar_t overridePath[MAX_PATH];
    if (GetEnvironmentVariableW(L"GAZEORA_LIBRARY", overridePath, MAX_PATH) > 0) {
        return overridePath;
    }
    PWSTR appData = nullptr;
    std::wstring path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        path = std::wstring(appData) + L"\\Gazeora\\library.json";
    }
    CoTaskMemFree(appData);
    return path;
}

bool GameLibrary::Load(std::wstring& error) {
    m_games.clear();
    m_loadedWriteTime = FileWriteTime();
    std::ifstream file(std::filesystem::path(m_path), std::ios::binary);
    if (!file) {
        return true;  // nothing saved yet
    }
    std::stringstream contents;
    contents << file.rdbuf();
    try {
        json root = json::parse(contents.str());
        for (const json& entry : root.value("games", json::array())) {
            Game game = GameFromJson(entry);
            if (game.id != 0 && !game.exePath.empty()) {
                m_games.push_back(std::move(game));
            }
        }
    } catch (const json::exception& e) {
        error = L"could not read " + m_path + L": " + Utf8ToWide(e.what());
        return false;
    }
    return true;
}

bool GameLibrary::Save(std::wstring& error) {
    size_t slash = m_path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        SHCreateDirectoryExW(nullptr, m_path.substr(0, slash).c_str(), nullptr);
    }

    json games = json::array();
    for (const Game& game : m_games) {
        games.push_back(GameToJson(game));
    }
    std::string text = json{{"version", kLibraryVersion}, {"games", games}}.dump(2);

    // Write a temp file and swap it in, so a crash never leaves a half-written library.
    std::wstring tempPath = m_path + L".tmp";
    {
        std::ofstream file(std::filesystem::path(tempPath), std::ios::binary | std::ios::trunc);
        if (!file || !file.write(text.data(), std::streamsize(text.size()))) {
            error = L"could not write " + tempPath;
            return false;
        }
    }
    if (!MoveFileExW(tempPath.c_str(), m_path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        error = L"could not replace " + m_path + L" (error " + std::to_wstring(GetLastError()) +
                L")";
        return false;
    }
    m_loadedWriteTime = FileWriteTime();
    return true;
}

bool GameLibrary::ChangedOnDisk() const { return FileWriteTime() != m_loadedWriteTime; }

uint64_t GameLibrary::FileWriteTime() const {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(m_path.c_str(), GetFileExInfoStandard, &data)) {
        return 0;
    }
    return (uint64_t(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
}

uint32_t GameLibrary::Add(Game game) {
    uint32_t maxId = 0;
    for (const Game& existing : m_games) {
        maxId = std::max(maxId, existing.id);
    }
    game.id = maxId + 1;
    game.addedAt = UnixNow();
    if (game.name.empty()) {
        game.name = DefaultGameName(game.exePath);
    }
    m_games.push_back(std::move(game));
    return m_games.back().id;
}

bool GameLibrary::Remove(uint32_t id) {
    auto it = std::find_if(m_games.begin(), m_games.end(),
                           [id](const Game& game) { return game.id == id; });
    if (it == m_games.end()) {
        return false;
    }
    m_games.erase(it);
    return true;
}

Game* GameLibrary::Find(uint32_t id) {
    for (Game& game : m_games) {
        if (game.id == id) {
            return &game;
        }
    }
    return nullptr;
}

Game* GameLibrary::FindByIdOrName(const std::wstring& key) {
    if (!key.empty() && std::all_of(key.begin(), key.end(), iswdigit)) {
        if (Game* game = Find(wcstoul(key.c_str(), nullptr, 10))) {
            return game;
        }
    }
    Game* match = nullptr;
    for (Game& game : m_games) {
        if (IEquals(game.name, key)) {
            if (match) {
                return nullptr;  // ambiguous
            }
            match = &game;
        }
    }
    return match;
}

LaunchSpec ToLaunchSpec(const Game& game) {
    LaunchSpec spec;
    spec.exePath = game.exePath;
    spec.arguments = game.arguments;
    spec.workingDir = game.workingDir;
    spec.processName = game.processName;
    if (game.launchVia != LaunchVia::Direct) {
        spec.steamAppId = FindSteamAppId(game.exePath);
    }
    return spec;
}

std::wstring DefaultGameName(const std::wstring& exePath) {
    std::wstring description = VersionFileDescription(exePath);
    return description.empty() ? FileStem(exePath) : description;
}

bool ResolveShortcut(const std::wstring& lnkPath, Game& game) {
    ComPtr<IShellLinkW> link;
    ComPtr<IPersistFile> file;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&link))) ||
        FAILED(link.As(&file)) || FAILED(file->Load(lnkPath.c_str(), STGM_READ))) {
        return false;
    }
    wchar_t buffer[MAX_PATH * 2] = {};
    if (FAILED(link->GetPath(buffer, ARRAYSIZE(buffer), nullptr, SLGP_RAWPATH)) || !buffer[0]) {
        return false;  // e.g. a shortcut to a URL or a shell folder
    }
    wchar_t expanded[MAX_PATH * 2];
    ExpandEnvironmentStringsW(buffer, expanded, ARRAYSIZE(expanded));
    game.exePath = expanded;

    buffer[0] = 0;
    link->GetArguments(buffer, ARRAYSIZE(buffer));
    game.arguments = buffer;
    buffer[0] = 0;
    link->GetWorkingDirectory(buffer, ARRAYSIZE(buffer));
    game.workingDir = buffer;
    if (game.name.empty()) {
        game.name = FileStem(lnkPath);
    }
    return true;
}

int64_t UnixNow() { return int64_t(std::time(nullptr)); }

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    int bytes = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr,
                                    nullptr);
    std::string result(bytes, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), &result[0], bytes, nullptr,
                        nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    int chars = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
    std::wstring result(chars, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), &result[0], chars);
    return result;
}

}  // namespace gazeora
