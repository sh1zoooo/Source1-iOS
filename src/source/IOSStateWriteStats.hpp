#pragma once
#ifdef SOURCE_IOS
#include "tier0/platform.h"
#include "tier0/dbg.h"
// Eligible state writes only. Forced Flush calls remain unconditional.
struct IOSStateWriteStats {
    unsigned long long submitted = 0, skipped = 0;
    double reported = 0;
    void report(double now) {
        if (!reported) { reported = now; return; }
        if (now - reported < 5.0) return;
        Warning("iOS GL state writes: submitted=%llu skipped=%llu window_s=%.3f\n",
                submitted, skipped, now - reported);
        submitted = skipped = 0;
        reported = now;
    }
};
inline IOSStateWriteStats &IOSStateWrites() {
    static IOSStateWriteStats stats;
    return stats;
}
#endif
