// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// Timing helpers shared by the benchmarks (gazeora_bench). Each benchmark prints one line per
// measurement: mean, median, 99th percentile and worst, in units picked to fit.
#pragma once

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace gazeora::bench {

inline double QpcSeconds() {
    static const double kPeriod = [] {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        return 1.0 / double(freq.QuadPart);
    }();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return double(now.QuadPart) * kPeriod;
}

// CPU time this process has used, all threads together. Against wall time it shows how many
// cores a parallel loop keeps busy, including OpenMP threads spinning between frames.
inline double CpuSeconds() {
    FILETIME created, exited, kernel, user;
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    auto seconds = [](FILETIME t) {
        return double(uint64_t(t.dwHighDateTime) << 32 | t.dwLowDateTime) * 1e-7;
    };
    return seconds(kernel) + seconds(user);
}

// Durations in seconds, summarized.
struct Stats {
    double mean = 0, median = 0, p99 = 0, max = 0;
    size_t count = 0;

    static Stats Of(std::vector<double> samples) {
        Stats s;
        s.count = samples.size();
        if (samples.empty()) return s;
        std::sort(samples.begin(), samples.end());
        for (double v : samples) s.mean += v;
        s.mean /= double(samples.size());
        s.median = samples[samples.size() / 2];
        s.p99 = samples[std::min(samples.size() - 1, samples.size() * 99 / 100)];
        s.max = samples.back();
        return s;
    }
};

// "2.31 ms", "840 ns": three significant digits.
inline std::string FormatSeconds(double seconds) {
    const char* unit = "s";
    double value = seconds;
    if (seconds < 1e-6) {
        value = seconds * 1e9, unit = "ns";
    } else if (seconds < 1e-3) {
        value = seconds * 1e6, unit = "us";
    } else if (seconds < 1.0) {
        value = seconds * 1e3, unit = "ms";
    }
    char text[32];
    snprintf(text, sizeof(text),
             value < 10    ? "%.2f %s"
             : value < 100 ? "%.1f %s"
                           : "%.0f %s",
             value, unit);
    return text;
}

inline void PrintHeader(const char* title) {
    printf("\n%-46s %10s %10s %10s %10s\n", title, "mean", "median", "p99", "max");
}

inline void Print(const std::string& name, const Stats& s, const std::string& note = {}) {
    if (s.count == 0) {
        printf("  %-44s %10s  %s\n", name.c_str(), "-", note.c_str());
        return;
    }
    printf("  %-44s %10s %10s %10s %10s  %s\n", name.c_str(), FormatSeconds(s.mean).c_str(),
           FormatSeconds(s.median).c_str(), FormatSeconds(s.p99).c_str(),
           FormatSeconds(s.max).c_str(), note.c_str());
    fflush(stdout);
}

// A line without timings, e.g. a size or a rate.
inline void PrintNote(const std::string& name, const std::string& note) {
    printf("  %-44s %s\n", name.c_str(), note.c_str());
    fflush(stdout);
}

// Keeps the compiler from dropping a computation whose result is otherwise unused.
inline void Consume(const void* p) {
    static volatile const void* sink;
    sink = p;
}

// Times `body` `samples` times, after `warmup` untimed calls. Each sample runs it `batch`
// times and counts the average, for calls too short to time one by one.
template <class Body>
Stats Measure(int samples, Body&& body, int warmup = 3, int batch = 1) {
    for (int i = 0; i < warmup * batch; ++i) body();
    std::vector<double> times;
    times.reserve(size_t(samples));
    for (int i = 0; i < samples; ++i) {
        double start = QpcSeconds();
        for (int j = 0; j < batch; ++j) body();
        times.push_back((QpcSeconds() - start) / batch);
    }
    return Stats::Of(std::move(times));
}

// "123 MB/s" for `bytes` processed per `seconds`.
inline std::string Throughput(double bytes, double seconds) {
    char text[32];
    snprintf(text, sizeof(text), "%.0f MB/s", seconds > 0 ? bytes / seconds / 1e6 : 0.0);
    return text;
}

// Sleeps until QpcSeconds() reaches `deadline`, to within about a millisecond: a
// high-resolution waitable timer, as Sleep(1) often takes 15 ms.
class Pacer {
public:
    Pacer()
        : m_timer(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                         TIMER_ALL_ACCESS)) {}
    ~Pacer() {
        if (m_timer) CloseHandle(m_timer);
    }
    Pacer(const Pacer&) = delete;
    Pacer& operator=(const Pacer&) = delete;

    void SleepUntil(double deadline) {
        double wait = deadline - QpcSeconds();
        if (wait <= 0) return;
        if (!m_timer) {
            Sleep(DWORD(wait * 1000));
            return;
        }
        LARGE_INTEGER due;
        due.QuadPart = -LONGLONG(wait * 1e7);  // relative, in 100 ns units
        SetWaitableTimer(m_timer, &due, 0, nullptr, nullptr, FALSE);
        WaitForSingleObject(m_timer, INFINITE);
    }

private:
    HANDLE m_timer;
};

// For printing encoder names and errors, which are ASCII in practice.
inline std::string Narrow(const std::wstring& text) {
    std::string out;
    for (wchar_t c : text) out += c < 128 ? char(c) : '?';
    return out;
}

// Pseudo-random bytes, the same on every run.
inline void FillNoise(std::vector<uint8_t>& bytes, uint32_t seed = 1) {
    for (uint8_t& b : bytes) b = uint8_t((seed = seed * 1664525u + 1013904223u) >> 24);
}

// A BGRA8 picture that changes like a game's does from frame to frame: a gradient that
// shifts with `frame`, a noisy square moving across it, and fine noise everywhere so the
// encoder has detail to spend bits on.
inline void FillScene(std::vector<uint8_t>& bgra, uint32_t width, uint32_t height, int frame) {
    bgra.resize(size_t(width) * height * 4);
    uint32_t seed = 0x9E3779B9u * uint32_t(frame + 1);
    uint32_t boxX = uint32_t(frame * 13) % (width > 256 ? width - 256 : 1);
    uint32_t boxY = uint32_t(frame * 7) % (height > 256 ? height - 256 : 1);
    for (uint32_t y = 0; y < height; ++y) {
        uint8_t* row = bgra.data() + size_t(y) * width * 4;
        for (uint32_t x = 0; x < width; ++x) {
            seed = seed * 1664525u + 1013904223u;
            uint8_t grain = uint8_t(seed >> 29);
            bool inBox = x - boxX < 256 && y - boxY < 256;
            row[x * 4 + 0] = inBox ? uint8_t(seed >> 24) : uint8_t((x + frame * 2) / 8 + grain);
            row[x * 4 + 1] = inBox ? uint8_t(seed >> 16) : uint8_t((y + frame) / 5 + grain);
            row[x * 4 + 2] = uint8_t((x + y) / 12 + grain);
            row[x * 4 + 3] = 255;
        }
    }
}

struct Size {
    uint32_t width, height;
    const char* name;
};
constexpr Size k720p = {1280, 720, "720p"};
constexpr Size k1080p = {1920, 1080, "1080p"};
constexpr Size k1440p = {2560, 1440, "1440p"};

}  // namespace gazeora::bench
