#include "platform/game_open_request.h"

#include <shellapi.h>

#include <utility>

namespace gazeora::app {

namespace {

// Tells Gazeora's WM_COPYDATA from anything else sent to its window: "GZOP".
constexpr ULONG_PTR kCopyDataId = 0x475A4F50;
constexpr wchar_t kOptionPrefix = L'-';
constexpr wchar_t kSeparator = L'\0';

}  // namespace

GameOpenRequest GameOpenRequest::FromCommandLine() {
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    std::vector<std::wstring> paths;
    for (int i = 1; arguments && i < count; ++i) {
        if (arguments[i][0] != kOptionPrefix) {
            paths.emplace_back(arguments[i]);
        }
    }
    LocalFree(arguments);
    return GameOpenRequest(std::move(paths));
}

std::optional<GameOpenRequest> GameOpenRequest::FromCopyData(const COPYDATASTRUCT& data) {
    if (data.dwData != kCopyDataId || data.cbData % sizeof(wchar_t) != 0) {
        return std::nullopt;
    }
    std::wstring joined(static_cast<const wchar_t*>(data.lpData), data.cbData / sizeof(wchar_t));
    std::vector<std::wstring> paths;
    size_t from = 0;
    for (size_t at = joined.find(kSeparator); at != std::wstring::npos;
         at = joined.find(kSeparator, from)) {
        paths.push_back(joined.substr(from, at - from));
        from = at + 1;
    }
    return GameOpenRequest(std::move(paths));
}

bool GameOpenRequest::SendTo(HWND target) const {
    std::wstring joined;
    for (const std::wstring& path : m_paths) {
        joined += path;
        joined += kSeparator;
    }
    COPYDATASTRUCT data = {kCopyDataId, DWORD(joined.size() * sizeof(wchar_t)), joined.data()};
    return SendMessageW(target, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data)) != 0;
}

}  // namespace gazeora::app
