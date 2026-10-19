#pragma once
// A single optional bridge from independently loaded engine modules to UIKit.
// Standalone tools and the non-iOS build have no dependency on the app binary.
enum IOSStartupEvent { IOSStartupPhase = 0, IOSStartupMenuReady = 1, IOSStartupPresented = 2 };
#ifdef SOURCE_IOS
#include <dlfcn.h>
inline void IOSStartupProgress(const char *phase, int event = IOSStartupPhase) {
    typedef void (*Callback)(const char *, int);
    static Callback callback = reinterpret_cast<Callback>(dlsym(RTLD_MAIN_ONLY, "ClientModIOSStartupSignal"));
    if (callback) callback(phase, event);
}
#else
inline void IOSStartupProgress(const char *, int = IOSStartupPhase) {}
#endif
