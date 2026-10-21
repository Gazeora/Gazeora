#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "gazeora/host/launch/launcher.h"

namespace gazeora {

enum class LaunchVia {
    Auto,    // through Steam if the exe is in a Steam library, else directly
    Direct,  // run the exe
    Steam,   // steam://rungameid/<app id>
};

struct Game {
    uint32_t id = 0;  // stable, small, unique within the library; used by the CLI
    std::wstring name;
    std::wstring exePath;
    std::wstring arguments;
    std::wstring workingDir;  // empty = the exe's folder
    std::wstring coverPath;   // optional image shown instead of the exe icon
    bool captureOnLaunch = false;
    LaunchVia launchVia = LaunchVia::Auto;
    // The real game exe, when exePath is a launcher that starts something else. Empty =
    // exePath's file name.
    std::wstring processName;
    int64_t addedAt = 0;     // Unix seconds
    int64_t lastPlayed = 0;  // Unix seconds, 0 = never
    int64_t playtimeSeconds = 0;
};

// The user's games, persisted as JSON. Shared by Gazeora.exe and GazeoraCLI.exe.
class GameLibrary {
public:
    // %APPDATA%\Gazeora\library.json, or %GAZEORA_LIBRARY% if set.
    static std::wstring DefaultPath();

    explicit GameLibrary(std::wstring path = DefaultPath()) : m_path(std::move(path)) {}

    // A missing file loads as an empty library.
    bool Load(std::wstring& error);
    bool Save(std::wstring& error);

    // True if the file was written by someone else since Load/Save.
    bool ChangedOnDisk() const;

    const std::wstring& Path() const { return m_path; }
    const std::vector<Game>& Games() const { return m_games; }

    // Assigns the id and addedAt; fills name from the exe if empty. Returns the new id.
    uint32_t Add(Game game);
    bool Remove(uint32_t id);
    Game* Find(uint32_t id);
    // Accepts an id ("3") or a name (case-insensitive). Returns null if not found or ambiguous.
    Game* FindByIdOrName(const std::wstring& key);

private:
    uint64_t FileWriteTime() const;

    std::wstring m_path;
    std::vector<Game> m_games;
    uint64_t m_loadedWriteTime = 0;
};

// "C:\Games\Foo\foo_game.exe" -> "foo_game"; uses the exe's FileDescription if it has one.
std::wstring DefaultGameName(const std::wstring& exePath);

// What to start for `game`, resolving LaunchVia::Auto (looks up the Steam app id).
LaunchSpec ToLaunchSpec(const Game& game);

// Resolves a .lnk shortcut into the exe, arguments and working directory it points at.
bool ResolveShortcut(const std::wstring& lnkPath, Game& game);

int64_t UnixNow();

std::string WideToUtf8(const std::wstring& text);
std::wstring Utf8ToWide(const std::string& text);

}  // namespace gazeora
