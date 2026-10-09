#!/usr/bin/env python3
"""Recover original graphical sources and replay audited SDK portability edits."""
import argparse
from pathlib import Path
import shutil
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('upstream', type=Path)
p.add_argument('output', type=Path)
p.add_argument('sdk', type=Path)
p.add_argument('prepared_sdk', type=Path)
a = p.parse_args()
import fcntl
lock = (a.output / ".prepare.lock").open("w")
fcntl.flock(lock, fcntl.LOCK_EX)
previous = {str(f.relative_to(a.output)): (f.read_bytes(), f.stat().st_mtime_ns)
            for f in a.output.rglob('*') if f.is_file()}
for folder in ('engine', 'inputsystem', 'launcher', 'togles', 'video', 'datamodel', 'appframework', 'utils/bzip2'):
    shutil.copytree(a.upstream / folder, a.output / folder, dirs_exist_ok=True)
for folder in ('engine', 'materialsystem', 'common', 'public'):
    for prepared in (a.prepared_sdk / folder).rglob('*'):
        relative = prepared.relative_to(a.prepared_sdk)
        baseline, target = a.sdk / relative, a.output / relative
        if not prepared.is_file() or not baseline.is_file() or not target.is_file():
            continue
        # These edits only exist to support the dedicated Metal preview.
        if str(relative) == 'engine/l_studio.cpp':
            continue
        if prepared.read_bytes() == baseline.read_bytes():
            continue
        merged = subprocess.run(['git', 'merge-file', '-p', str(prepared), str(baseline), str(target)], capture_output=True)
        if merged.returncode:
            raise RuntimeError(f'Conflicting portability adaptation: {relative}')
        target.write_bytes(merged.stdout)
compat = a.output / 'graphics-compat'
shutil.copytree(a.upstream / 'public/togl', compat / 'togl', dirs_exist_ok=True)
shutil.copytree(a.upstream / 'public/togles', compat / 'togles', dirs_exist_ok=True)
# SDL's pinned EGL headers predate the no-X11 header option. Native checks
# use opaque EGL handles; no X11 calls are compiled in this SDL build.
shutil.copytree(a.sdk / 'thirdparty/SDL-src/src/video/khronos/EGL', compat / 'EGL', dirs_exist_ok=True)
f = compat / 'EGL/eglplatform.h'
text = f.read_text()
text = text.replace('#elif defined(__unix__)', '#elif defined(MESA_EGL_NO_X11_HEADERS)\n\ntypedef void *EGLNativeDisplayType;\ntypedef void *EGLNativePixmapType;\ntypedef void *EGLNativeWindowType;\n\n#elif defined(__unix__)')
f.write_text(text)
(compat / 'GL').mkdir(parents=True, exist_ok=True)
(compat / 'GL/gl.h').write_text('#include "SDL_opengl.h"\n')
(compat / 'GL/glext.h').write_text('#include "SDL_opengl_glext.h"\n')
f = a.output / 'togles/linuxwin/glentrypoints.cpp'
s = f.read_text()
assert s.count('#include <GL/glx.h>') == 1
f.write_text(s.replace('#include <GL/glx.h>', '#ifndef USE_SDL\n#include <GL/glx.h>\n#endif'))
# Adapt original input events to the SDK interface used by the paired client.
def replace(path, old, new, count=1):
    f = a.output / path
    text = f.read_text()
    if text.count(old) != count:
        raise RuntimeError(f'Input adaptation context changed: {path}: {old}')
    f.write_text(text.replace(old, new))
replace('inputsystem/inputsystem.h', '#include "appframework/ilaunchermgr.h"',
        '#include "appframework/ilaunchermgr.h"\n#include "TouchFingerSlots.hpp"\n#define TOUCH_FINGER_MAX_COUNT 10')
replace('inputsystem/inputsystem.h', 'bool GetTouchAccumulators( InputEventType_t &event, int &fingerId, int& accumX, int& accumY );',
        'virtual bool GetTouchAccumulators(int fingerId, float &dx, float &dy);\n\tTouchFingerSlots m_touchFingerSlots;')
replace('inputsystem/inputsystem.h', 'InputEventType_t m_touchAccumEvent;\n\tint m_touchAccumFingerId, m_touchAccumX, m_touchAccumY;',
        'float m_touchAccumX[TOUCH_FINGER_MAX_COUNT]{};\n\tfloat m_touchAccumY[TOUCH_FINGER_MAX_COUNT]{};')
f = a.output / 'inputsystem/inputsystem.cpp'
s = f.read_text()
start = s.index('bool CInputSystem::GetTouchAccumulators(')
end = s.index('\n}', start) + 2
f.write_text(s[:start] + s[end:])
replace('utils/bzip2/bzlib_private.h', '__inline__ Int32 BZ2_indexIntoF', 'static __inline__ Int32 BZ2_indexIntoF')
# Each graphical DLL owns its original material convars independently.
replace('materialsystem/cmaterialsystem.cpp', 'extern ConVar mat_debugalttab;',
        'ConVar mat_debugalttab( "mat_debugalttab", "0", FCVAR_CHEAT );')
replace('gameui/GameUI_Interface.cpp', '\tsteamapicontext->Init();',
        '#ifndef NO_STEAM\n\tsteamapicontext->Init();\n#endif')
replace('engine/audio/voice.cpp', '\t\tsteamapicontext->Init();',
        '#ifndef NO_STEAM\n\t\tsteamapicontext->Init();\n#endif')
replace('engine/host.cpp', '\tISteamRemoteStorage *pRemoteStorage = SteamClient()?',
        '#ifdef NO_STEAM\n\tISteamRemoteStorage *pRemoteStorage = NULL;\n#else\n\tISteamRemoteStorage *pRemoteStorage = SteamClient()?', count=2)
replace('engine/host.cpp', 'STEAMREMOTESTORAGE_INTERFACE_VERSION ):NULL;',
        'STEAMREMOTESTORAGE_INTERFACE_VERSION ):NULL;\n#endif', count=2)
replace('engine/host.cpp', '\t\t\t\tSteamAPI_RunCallbacks();',
        '#ifndef NO_STEAM\n\t\t\t\tSteamAPI_RunCallbacks();\n#endif')
# Offline profile must not initialize an unavailable Steam controller API.
replace('inputsystem/inputsystem.cpp', 'if ( !m_bSkipControllerInitialization && SteamAPI_InitSafe() )',
        '#ifndef NO_STEAM\n\tif ( !m_bSkipControllerInitialization && SteamAPI_InitSafe() )')
replace('inputsystem/inputsystem.cpp', '\tButtonCode_InitKeyTranslationTable();',
        '#endif\n\tButtonCode_InitKeyTranslationTable();')
f = a.output / 'inputsystem/touch_sdl.cpp'
s = (a.sdk / 'inputsystem/touch_sdl.cpp').read_text()
s = s.replace('event->tfinger.fingerId,', 'slot,')
s = s.replace('case SDL_FINGERDOWN:', 'case SDL_FINGERDOWN: {\n\tint slot = pInputSystem->m_touchFingerSlots.press(event->tfinger.fingerId);')
s = s.replace('case SDL_FINGERUP:', 'case SDL_FINGERUP: {\n\tint slot = pInputSystem->m_touchFingerSlots.find(event->tfinger.fingerId);')
s = s.replace('case SDL_FINGERMOTION:', 'case SDL_FINGERMOTION: {\n\tint slot = pInputSystem->m_touchFingerSlots.find(event->tfinger.fingerId);')
s = s.replace('\t\tbreak;', '\t\tbreak; }')
s = s.replace('pInputSystem->FingerEvent( IE_FingerUp, slot, event->tfinger.x, event->tfinger.y, event->tfinger.dx, event->tfinger.dy );',
              'pInputSystem->FingerEvent( IE_FingerUp, slot, event->tfinger.x, event->tfinger.y, event->tfinger.dx, event->tfinger.dy );\n\tpInputSystem->m_touchFingerSlots.release(event->tfinger.fingerId);')
s = s.replace('m_bJoystickInitialized = true;', 'm_bTouchInitialized = true;')
s = s.replace('m_bTouchInitialized = false;', 'm_bTouchInitialized = false;\n\tm_touchFingerSlots.reset();\n\tmemset(m_touchAccumX, 0, sizeof(m_touchAccumX));\n\tmemset(m_touchAccumY, 0, sizeof(m_touchAccumY));')
s = s.replace('\tdx = m_touchAccumX[fingerId];', '\tif (fingerId < 0 || fingerId >= TOUCH_FINGER_MAX_COUNT) { dx = dy = 0; return false; }\n\tdx = m_touchAccumX[fingerId];')
s = s.replace('if( fingerId >= TOUCH_FINGER_MAX_COUNT )', 'if( fingerId < 0 || fingerId >= TOUCH_FINGER_MAX_COUNT )')
f.write_text(s)

# CGL/Carbon paths are desktop macOS services. On iOS retain the original
# portable SDL paths and use SDL's UIKit/EAGL context provider.
import re
for folder in ('graphics-compat/togl', 'graphics-compat/togles', 'togles', 'appframework'):
    for f in (a.output / folder).rglob('*'):
        if f.suffix not in ('.h', '.cpp', '.inl'):
            continue
        text = f.read_text()
        text = re.sub(r'#ifdef\s+OSX\b', '#if defined(OSX) && !defined(SOURCE_IOS)', text)
        text = re.sub(r'defined\(\s*OSX\s*\)', '(defined(OSX) && !defined(SOURCE_IOS))', text)
        f.write_text(text)
replace('appframework/sdlmgr.cpp', 'if (SDL_GL_LoadLibrary("libGLESv3.so") == -1)',
        '#ifdef SOURCE_IOS\n\t\tif (SDL_GL_LoadLibrary(NULL) == -1)\n#else\n\t\tif (SDL_GL_LoadLibrary("libGLESv3.so") == -1)\n#endif')
replace('appframework/sdlmgr.cpp', '#ifdef TOGLES\n\tl_egl = dlopen("libEGL.so", RTLD_LAZY);',
        '#ifdef SOURCE_IOS\n\t_glGetProcAddress = SDL_GL_GetProcAddress;\n\tSET_GL_ATTR(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);\n\tSET_GL_ATTR(SDL_GL_CONTEXT_MAJOR_VERSION, 3);\n\tSET_GL_ATTR(SDL_GL_CONTEXT_MINOR_VERSION, 0);\n#elif defined(TOGLES)\n\tl_egl = dlopen("libEGL.so", RTLD_LAZY);')

# Keep rebuilds incremental when a configure pass produces identical content.
import os
for relative, (content, mtime) in previous.items():
    f = a.output / relative
    if f.is_file() and f.read_bytes() == content:
        os.utime(f, ns=(f.stat().st_atime_ns, mtime))
    elif f.is_file():
        os.utime(f, None)
