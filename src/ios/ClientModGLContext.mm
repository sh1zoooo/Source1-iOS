#import <OpenGLES/EAGL.h>

// SDL caches its current context in TLS. UIKit can change the native EAGL
// context without updating that cache, so SDL_GL_MakeCurrent may return early.
extern "C" bool ClientModIOSRestoreGLContext(void *context) {
    EAGLContext *expected = (__bridge EAGLContext *)context;
    if ([EAGLContext currentContext] == expected) return true;
    return [EAGLContext setCurrentContext:expected];
}
