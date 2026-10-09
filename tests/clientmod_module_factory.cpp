#include <dlfcn.h>
#include <cstdio>
#include <cstring>
// Strict-link and factory discovery only: no GPU or gameplay claim.
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    void *module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) { std::fprintf(stderr, "%s\n", dlerror()); return 1; }
    using Factory = void *(*)(const char *, int *);
    auto factory = reinterpret_cast<Factory>(dlsym(module, "CreateInterface"));
    if (!factory) { std::fprintf(stderr, "missing original factory\n"); return 1; }
    int status = -1;
    void *interface = factory(argv[2], &status);
    if (!interface || status != 0) return 1;
    int unknown = -1;
    if (factory("Source1IOS_NoSuchInterface", &unknown) || unknown == 0) return 1;
    std::printf("original %s factory, RTLD_NOW and unknown-interface rejection: PASS\n", argv[2]);
    // Preserve engine singletons until process exit; no lifecycle was initialized.
    return 0;
}
