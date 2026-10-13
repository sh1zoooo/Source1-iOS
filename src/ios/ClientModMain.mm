#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "SDL_main.h"
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <unistd.h>
#include "../runtime/CrashDiagnostics.hpp"
#include <mach-o/dyld.h>

static void reportException(NSException *exception) {
    std::fprintf(stderr, "iOS exception: %s: %s\n%s\n",
        exception.name.UTF8String, exception.reason.UTF8String,
        [exception.callStackSymbols componentsJoinedByString:@"\n"].UTF8String);
}

static void reportImage(const struct mach_header *header, intptr_t) {
    for (uint32_t i = 0; i < _dyld_image_count(); ++i) {
        if (_dyld_get_image_header(i) != header) continue;
        const char *name = _dyld_get_image_name(i);
        if (name && std::strstr(name, ".app/"))
            std::fprintf(stderr, "iOS image: base=%p name=%s\n", (const void *)header, name);
        break;
    }
}

// UIKit/EAGL startup only. Menus, HUD, input and the game loop belong to the
// original launcher/engine/client modules, with no preview runtime linked here.
static int runClientMod(int, char **) {
    @autoreleasepool {
        NSFileManager *files = [NSFileManager defaultManager];
        NSURL *documents = [files URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
        NSURL *content = [documents URLByAppendingPathComponent:@"Source1IOS/content" isDirectory:YES];
        NSError *error = nil;
        if (![files createDirectoryAtURL:content withIntermediateDirectories:YES attributes:nil error:&error]) {
            NSLog(@"ClientMod content directory: %@", error);
            return 1;
        }
        NSString *logPath = [[content URLByDeletingLastPathComponent].path stringByAppendingPathComponent:@"ClientMod-native.log"];
        if (FILE *log = std::fopen(logPath.fileSystemRepresentation, "w")) {
            dup2(fileno(log), STDOUT_FILENO);
            dup2(fileno(log), STDERR_FILENO);
            std::fclose(log);
            setvbuf(stdout, nullptr, _IOLBF, 0);
            setvbuf(stderr, nullptr, _IOLBF, 0);
        }
        std::fprintf(stdout, "Original ClientMod runtime %s; content: %s\n", [[[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"] UTF8String], content.path.UTF8String);
        InstallClientModCrashDiagnostics();
        NSSetUncaughtExceptionHandler(reportException);
        _dyld_register_func_for_add_image(reportImage);
        // Source's POSIX singleton lock must live in the iOS container. The
        // desktop /tmp path is outside our sandbox and reports a false duplicate.
        NSString *temporaryDirectory = NSTemporaryDirectory();
        if (temporaryDirectory.length == 0 ||
            setenv("TMPDIR", temporaryDirectory.fileSystemRepresentation, 1) != 0) {
            std::fprintf(stderr, "ClientMod sandbox temporary directory unavailable\n");
            return 1;
        }
        std::fprintf(stdout, "ClientMod singleton lock directory: %s\n", temporaryDirectory.fileSystemRepresentation);
        NSString *libraries = [[NSBundle mainBundle].bundlePath stringByAppendingPathComponent:@"Frameworks"];
        setenv("SOURCE_CLIENTMOD_LIBDIR", libraries.fileSystemRepresentation, 1);
        NSString *fonts = [[NSBundle mainBundle].bundlePath stringByAppendingPathComponent:@"Fonts"];
        setenv("SOURCE_CLIENTMOD_FONTDIR", fonts.fileSystemRepresentation, 1);
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
        // The original inputsystem already consumes SDL_FINGER events.
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
        NSString *launcherPath = [libraries stringByAppendingPathComponent:@"liblauncher.dylib"];
        void *launcher = dlopen(launcherPath.fileSystemRepresentation, RTLD_NOW | RTLD_LOCAL);
        if (!launcher) { std::fprintf(stderr, "ClientMod launcher load failed: %s\n", dlerror()); return 1; }
        using LauncherMain = int (*)(int, char **);
        auto launch = reinterpret_cast<LauncherMain>(dlsym(launcher, "LauncherMain"));
        if (!launch) { std::fprintf(stderr, "Original LauncherMain export missing\n"); return 1; }
        if (![files changeCurrentDirectoryPath:content.path]) return 1;
        // UIKit display bounds can still be portrait during engine startup.
        // Render into a landscape canvas with enough room for desktop dialogs;
        // the final drawable blit and normalized touch coordinates share it.
        CGSize display = UIScreen.mainScreen.bounds.size;
        double longSide = MAX(display.width, display.height);
        double shortSide = MIN(display.width, display.height);
        int renderHeight = 720;
        int renderWidth = shortSide > 0 ? int(renderHeight * longSide / shortSide + .5) : 1280;
        if (renderWidth < 960) renderWidth = 960;
        std::fprintf(stderr, "iOS landscape canvas: %dx%d display=%.0fx%.0f\n",
                     renderWidth, renderHeight, display.width, display.height);
        NSArray<NSString *> *arguments = @[@"ClientMod", @"-basedir", content.path,
            @"-game", @"cm", @"-insecure", @"-noip", @"-windowed",
            @"-w", [NSString stringWithFormat:@"%d", renderWidth],
            @"-h", [NSString stringWithFormat:@"%d", renderHeight]];
        std::vector<char *> argv;
        for (NSString *argument in arguments) argv.push_back(const_cast<char *>(argument.UTF8String));
        argv.push_back(nullptr);
        NSLog(@"Starting original ClientMod LauncherMain; content %@", content.path);
        const int result = launch(static_cast<int>(arguments.count), argv.data());
        std::fprintf(stderr, "Original ClientMod LauncherMain returned: %d\n", result);
        return result;
    }
}

int main(int argc, char **argv) {
    return SDL_UIKitRunApp(argc, argv, runClientMod);
}
