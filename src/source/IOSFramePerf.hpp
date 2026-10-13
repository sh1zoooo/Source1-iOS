#pragma once
#ifdef SOURCE_IOS
#include <chrono>
#include <cstdio>
struct IOSFramePerf {
    using Clock = std::chrono::steady_clock;
    Clock::time_point interval = Clock::now();
    double totalMs = 0, maximumMs = 0;
    unsigned samples = 0;
    bool active = false;
};
class IOSFrameScope {
    IOSFramePerf &stats;
    const char *phase;
    IOSFramePerf::Clock::time_point start;
    bool outer;
public:
    IOSFrameScope(IOSFramePerf &s, const char *name) : stats(s), phase(name),
        start(IOSFramePerf::Clock::now()), outer(!s.active) { if (outer) s.active = true; }
    ~IOSFrameScope() {
        if (!outer) return;
        const auto now = IOSFramePerf::Clock::now();
        const double ms = std::chrono::duration<double, std::milli>(now - start).count();
        stats.active = false;
        stats.totalMs += ms;
        if (ms > stats.maximumMs) stats.maximumMs = ms;
        ++stats.samples;
        const double elapsed = std::chrono::duration<double>(now - stats.interval).count();
        if (elapsed >= 5.0) {
            std::fprintf(stderr, "iOS perf: phase=%s samples=%u rate=%.1f/s avgMs=%.2f maxMs=%.2f\n",
                phase, stats.samples, stats.samples / elapsed,
                stats.totalMs / stats.samples, stats.maximumMs);
            stats.totalMs = stats.maximumMs = 0;
            stats.samples = 0;
            stats.interval = now;
        }
    }
};
#endif
