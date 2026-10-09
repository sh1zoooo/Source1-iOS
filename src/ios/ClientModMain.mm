#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "SDL_main.h"
#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <unistd.h>

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
        std::fprintf(stdout, "Original ClientMod runtime; content: %s\n", content.path.UTF8String);
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
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
        NSString *launcherPath = [libraries stringByAppendingPathComponent:@"liblauncher.dylib"];
        void *launcher = dlopen(launcherPath.fileSystemRepresentation, RTLD_NOW | RTLD_LOCAL);
        if (!launcher) { std::fprintf(stderr, "ClientMod launcher load failed: %s\n", dlerror()); return 1; }
        using LauncherMain = int (*)(int, char **);
        auto launch = reinterpret_cast<LauncherMain>(dlsym(launcher, "LauncherMain"));
        if (!launch) { std::fprintf(stderr, "Original LauncherMain export missing\n"); return 1; }
        if (![files changeCurrentDirectoryPath:content.path]) return 1;
        NSArray<NSString *> *arguments = @[@"ClientMod", @"-basedir", content.path,
            @"-game", @"cm", @"-insecure", @"-noip", @"-windowed"];
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
