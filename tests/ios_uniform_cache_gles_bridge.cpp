#include "IOSUniformCache.hpp"
extern "C" {
void *make_cache() { return new IOSUniformCache<>; }
void free_cache(void *p) { delete static_cast<IOSUniformCache<> *>(p); }
bool update_cache(void *p, int first, int count, const float *values,
                  void (*upload)(int, int, const float *)) {
    return static_cast<IOSUniformCache<> *>(p)->update(first, count, values, upload);
}
}
