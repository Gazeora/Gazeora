#pragma once

#include <cstdio>
#include <mutex>
#include <string>

namespace gazeora {

// A timestamped text log of what players did, for working out why a stream stalled on one
// device. Thread-safe; stops writing past a few megabytes so a long session can't fill the
// disk. Writing without a file is a no-op.
class StreamLog {
public:
    StreamLog() = default;
    StreamLog(const StreamLog&) = delete;
    StreamLog& operator=(const StreamLog&) = delete;
    ~StreamLog() { Open({}); }

    // Starts a new log at `path`, replacing what was there. Empty closes the log.
    void Open(const std::wstring& path);
    // One line, printf style.
    void Write(const char* format, ...)
#ifdef __GNUC__
        __attribute__((format(printf, 2, 3)))
#endif
        ;

private:
    std::mutex m_mutex;
    FILE* m_file = nullptr;
    long m_bytes = 0;
};

}  // namespace gazeora
