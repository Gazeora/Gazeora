// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/library/repositories/json_game_repository.h"

#include <windows.h>
#include <shlobj.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

#include "gazeora/common/text/utf8.h"
#include "gazeora/library/exceptions/library_storage_error.h"
#include "mappers/game_json_mapper.h"

using nlohmann::json;

namespace gazeora {

namespace {

constexpr int kLibraryVersion = 1;
constexpr int kJsonIndent = 2;
constexpr wchar_t kTempSuffix[] = L".tmp";
constexpr int kHighDwordShift = 32;

bool IsUsable(const Game& game) { return game.id != 0 && !game.exePath.empty(); }

void CreateParentFolder(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        SHCreateDirectoryExW(nullptr, path.substr(0, slash).c_str(), nullptr);
    }
}

}  // namespace

JsonGameRepository::JsonGameRepository(std::wstring path) : m_path(std::move(path)) {}

std::vector<Game> JsonGameRepository::Load() {
    m_accessedWriteTime = FileWriteTime();
    std::optional<std::string> text = ReadFile();
    std::vector<Game> games;
    if (!text) {
        return games;  // nothing saved yet
    }
    GameJsonMapper mapper;
    try {
        for (const json& entry : json::parse(*text).value("games", json::array())) {
            Game game = mapper.FromJson(entry);
            if (IsUsable(game)) {
                games.push_back(std::move(game));
            }
        }
    } catch (const json::exception& e) {
        throw LibraryStorageError(L"could not read " + m_path + L": " + Utf8::ToWide(e.what()));
    }
    return games;
}

void JsonGameRepository::Save(const std::vector<Game>& games) {
    GameJsonMapper mapper;
    json entries = json::array();
    for (const Game& game : games) {
        entries.push_back(mapper.ToJson(game));
    }
    CreateParentFolder(m_path);
    WriteFileAtomically(json{{"version", kLibraryVersion}, {"games", entries}}.dump(kJsonIndent));
    m_accessedWriteTime = FileWriteTime();
}

bool JsonGameRepository::ChangedSinceLastAccess() const {
    return FileWriteTime() != m_accessedWriteTime;
}

std::optional<std::string> JsonGameRepository::ReadFile() const {
    std::ifstream file(std::filesystem::path(m_path), std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::stringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

// A temp file swapped in, so a crash never leaves a half-written library.
void JsonGameRepository::WriteFileAtomically(const std::string& text) const {
    std::wstring tempPath = m_path + kTempSuffix;
    {
        std::ofstream file(std::filesystem::path(tempPath), std::ios::binary | std::ios::trunc);
        if (!file || !file.write(text.data(), std::streamsize(text.size()))) {
            throw LibraryStorageError(L"could not write " + tempPath);
        }
    }
    if (!MoveFileExW(tempPath.c_str(), m_path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        throw LibraryStorageError(L"could not replace " + m_path + L" (error " +
                                  std::to_wstring(GetLastError()) + L")");
    }
}

uint64_t JsonGameRepository::FileWriteTime() const {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(m_path.c_str(), GetFileExInfoStandard, &data)) {
        return 0;
    }
    return (uint64_t(data.ftLastWriteTime.dwHighDateTime) << kHighDwordShift) |
           data.ftLastWriteTime.dwLowDateTime;
}

}  // namespace gazeora
