#include "gazeora/host/stream/stream_log.h"

#include <windows.h>

#include <algorithm>
#include <cstdarg>

namespace gazeora {

namespace {
constexpr long kMaxLogBytes = 4 << 20;
}

void StreamLog::Open(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
    m_bytes = 0;
    if (!path.empty()) {
        m_file = _wfopen(path.c_str(), L"w");
    }
}

void StreamLog::Write(const char* format, ...) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_file || m_bytes > kMaxLogBytes) {
            return;  // don't format what nobody will read
        }
    }
    char text[512];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    SYSTEMTIME time;
    GetLocalTime(&time);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_file) {
        return;
    }
    int written = fprintf(m_file, "%02d:%02d:%02d.%03d  %s\n", time.wHour, time.wMinute,
                          time.wSecond, time.wMilliseconds, text);
    fflush(m_file);
    m_bytes += std::max(written, 0);
}

}  // namespace gazeora
