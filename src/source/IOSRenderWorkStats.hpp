#pragma once
#ifdef SOURCE_IOS
#include "tier0/platform.h"
#include "tier0/dbg.h"
// Counts represent submitted work and bytes written, not GPU timings. One
// cheap increment path per call; the engine clock is read only once/frame.
struct IOSRenderWorkStats {
    unsigned long long draws = 0, primitives = 0, indices = 0, indexRange = 0;
    unsigned long long dynamicIndexDraws = 0;
    unsigned long long writeBytes[2][2] = {}, writes[2][2] = {};
    unsigned long long orphans = 0, orphanBytes = 0;
    double reported = 0;
    void draw(unsigned mode, unsigned count, unsigned start, unsigned end, bool dynamic) {
        ++draws; indices += count; dynamicIndexDraws += dynamic;
        indexRange += end >= start ? (unsigned long long)end - start + 1 : 0;
        // GL_LINES=1, GL_TRIANGLES=4, GL_TRIANGLE_STRIP=5.
        primitives += mode == 1 ? count / 2 : mode == 4 ? count / 3 :
            mode == 5 && count > 2 ? count - 2 : 0;
    }
    void write(bool index, bool dynamic, unsigned bytes) {
        ++writes[index][dynamic]; writeBytes[index][dynamic] += bytes;
    }
    void orphan(unsigned bytes) { ++orphans; orphanBytes += bytes; }
    void report(double now) {
        if (!reported) { reported = now; return; }
        if (now - reported < 5.0) return;
        Warning("iOS render work: draws=%llu dynamic_index_draws=%llu primitives=%llu indices=%llu declared_vertex_range=%llu static_vb_writes=%llu static_vb_bytes=%llu dynamic_vb_writes=%llu dynamic_vb_bytes=%llu static_ib_writes=%llu static_ib_bytes=%llu dynamic_ib_writes=%llu dynamic_ib_bytes=%llu orphans=%llu orphan_bytes=%llu window_s=%.3f\n",
            draws, dynamicIndexDraws, primitives, indices, indexRange,
            writes[0][0], writeBytes[0][0], writes[0][1], writeBytes[0][1],
            writes[1][0], writeBytes[1][0], writes[1][1], writeBytes[1][1],
            orphans, orphanBytes, now - reported);
        *this = IOSRenderWorkStats(); reported = now;
    }
};
inline IOSRenderWorkStats &IOSRenderWork() {
    static IOSRenderWorkStats stats;
    return stats;
}
#endif
