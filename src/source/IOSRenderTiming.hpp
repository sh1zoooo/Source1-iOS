#pragma once
#ifdef SOURCE_IOS
#include "tier0/platform.h"
#include "tier0/dbg.h"
// Existing engine clock only; no new native clock or startup dependencies.
struct IOSRenderTimingStats {
    unsigned calls = 0;
    double total = 0, maximum = 0, reported = 0;
};
class IOSRenderTiming {
    IOSRenderTimingStats &stats_;
    const char *phase_;
    double start_;
public:
    IOSRenderTiming(IOSRenderTimingStats &stats, const char *phase)
        : stats_(stats), phase_(phase), start_(Plat_FloatTime()) {}
    ~IOSRenderTiming() {
        double now = Plat_FloatTime(), elapsed = now - start_;
        ++stats_.calls; stats_.total += elapsed;
        if (elapsed > stats_.maximum) stats_.maximum = elapsed;
        if (!stats_.reported) stats_.reported = now;
        if (now - stats_.reported >= 5.0) {
            Warning("iOS render timing: phase=%s calls=%u avg_ms=%.3f max_ms=%.3f total_ms=%.3f window_s=%.3f\n",
                phase_, stats_.calls, stats_.total * 1000 / stats_.calls,
                stats_.maximum * 1000, stats_.total * 1000, now - stats_.reported);
            stats_.calls = 0; stats_.total = stats_.maximum = 0; stats_.reported = now;
        }
    }
};
#define IOS_RENDER_TIMING(phase) static IOSRenderTimingStats iosTimingStats; IOSRenderTiming iosTiming(iosTimingStats, phase)
#else
#define IOS_RENDER_TIMING(phase)
#endif
