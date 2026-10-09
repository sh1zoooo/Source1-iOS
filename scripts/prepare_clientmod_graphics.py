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
for folder in ('engine', 'inputsystem', 'launcher', 'togles', 'video', 'datamodel', 'appframework', 'utils/bzip2', 'utils/common', 'filesystem', 'datacache', 'studiorender', 'vphysics', 'soundemittersystem', 'scenefilecache', 'serverbrowser'):
    shutil.copytree(a.upstream / folder, a.output / folder, dirs_exist_ok=True)
for folder in ('engine', 'materialsystem', 'common', 'public', 'filesystem', 'datacache', 'studiorender', 'vphysics'):
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
# ToGLES uses desktop GL enums/types as its Direct3D compatibility ABI. Keep
# SDL's original declarations on iOS; actual GL calls still use SDL/EAGL.
text = (a.sdk / 'thirdparty/SDL-src/include/SDL_opengl.h').read_text()
text = text.replace('#ifndef __IPHONEOS__  /* No OpenGL on iOS. */', '#if 1 /* GL declarations for the ToGLES compatibility ABI. */')
(compat / 'SDL_opengl.h').write_text(text)
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
replace('inputsystem/inputsystem.cpp', '\tBaseClass::Shutdown();',
        '\tShutdownTouch();\n\tBaseClass::Shutdown();')
replace('utils/bzip2/bzlib_private.h', '__inline__ Int32 BZ2_indexIntoF', 'static __inline__ Int32 BZ2_indexIntoF')
replace('datacache/datacache.cpp', 'extern ConVar developer;',
        'ConVar developer( "developer", "0", FCVAR_INTERNAL_USE );')
replace('datacache/mdlcache.cpp', '#ifndef SOURCE_ENGINE_PORT\nconst studiohdr_t *studiohdr_t::FindModel',
        'const studiohdr_t *studiohdr_t::FindModel')
replace('datacache/mdlcache.cpp', 'return g_MDLCache.GetStudioHdr( VoidPtrToMDLHandle( cache ) );\n}\n#endif',
        'return g_MDLCache.GetStudioHdr( VoidPtrToMDLHandle( cache ) );\n}')
replace('studiorender/studiorendercontext.cpp', '#ifndef SOURCE_ENGINE_PORT\nconst vertexFileHeader_t * mstudiomodel_t::CacheVertexData',
        'const vertexFileHeader_t * mstudiomodel_t::CacheVertexData')
replace('studiorender/studiorendercontext.cpp', 'return g_pStudioDataCache->CacheVertexData( (studiohdr_t *)pModelData );\n}\n#endif',
        'return g_pStudioDataCache->CacheVertexData( (studiohdr_t *)pModelData );\n}')
# Each graphical DLL owns its original material convars independently.
replace('materialsystem/cmaterialsystem.cpp', 'extern ConVar mat_debugalttab;',
        'ConVar mat_debugalttab( "mat_debugalttab", "0", FCVAR_CHEAT );')
replace('gameui/GameUI_Interface.cpp', '\tsteamapicontext->Init();',
        '#ifndef NO_STEAM\n\tsteamapicontext->Init();\n#endif')
# Preserve original platform UI and resolve its desktop module names for iOS.
replace('gameui/VGuiSystemModuleLoader.cpp', '\t\tif ( IsOSX() )',
        '#ifdef SOURCE_IOS\n\t\tchar iosModule[MAX_PATH];\n\t\tV_FileBase(it->GetString("dll_osx", it->GetString("dll")), iosModule, sizeof(iosModule));\n\t\tV_strlower(iosModule);\n\t\tV_strncat(iosModule, DLL_EXT_STRING, sizeof(iosModule));\n\t\tdllPath = iosModule;\n#else\n\t\tif ( IsOSX() )')
replace('gameui/VGuiSystemModuleLoader.cpp', '\n\n\t\t// load the module',
        '\n#endif\n\n\t\t// load the module')
replace('gameui/VGuiSystemModuleLoader.cpp', 'it->GetString("dll"));', 'dllPath);')
replace('serverbrowser/ServerBrowser.cpp', '\tSteamAPI_InitSafe();\n\tSteamAPI_SetTryCatchCallbacks( false ); // We don\'t use exceptions, so tell steam not to use try/catch in callback handlers\n\tsteamapicontext->Init();',
        '#ifndef NO_STEAM\n\tSteamAPI_InitSafe();\n\tSteamAPI_SetTryCatchCallbacks( false ); // We don\'t use exceptions, so tell steam not to use try/catch in callback handlers\n\tsteamapicontext->Init();\n#endif')
replace('engine/audio/voice.cpp', '\t\tsteamapicontext->Init();',
        '#ifndef NO_STEAM\n\t\tsteamapicontext->Init();\n#endif')
replace('engine/host.cpp', '\tISteamRemoteStorage *pRemoteStorage = SteamClient()?',
        '#ifdef NO_STEAM\n\tISteamRemoteStorage *pRemoteStorage = NULL;\n#else\n\tISteamRemoteStorage *pRemoteStorage = SteamClient()?', count=2)
replace('engine/host.cpp', 'STEAMREMOTESTORAGE_INTERFACE_VERSION ):NULL;',
        'STEAMREMOTESTORAGE_INTERFACE_VERSION ):NULL;\n#endif', count=2)
replace('engine/host.cpp', '\t\t\t\tSteamAPI_RunCallbacks();',
        '#ifndef NO_STEAM\n\t\t\t\tSteamAPI_RunCallbacks();\n#endif')
shutil.copy2(a.upstream / 'game/server/gameinterface.cpp', a.output / 'game/server/gameinterface.cpp')
for operation in ('Init', 'Clear'):
    replace('game/server/gameinterface.cpp', f'#ifndef _X360\n\ts_SteamAPIContext.{operation}();',
            f'#if !defined(_X360) && !defined(NO_STEAM)\n\ts_SteamAPIContext.{operation}();')
# Reuse audited SDK offline patches for account-bound server votes/statistics.
for path in ('game/server/vote_controller.cpp', 'game/server/cstrike/cs_gamestats.cpp'):
    prepared, baseline, target = a.prepared_sdk / path, a.sdk / path, a.output / path
    shutil.copy2(a.upstream / path, target)
    if prepared.is_file():
        merged = subprocess.run(['git', 'merge-file', '-p', str(prepared), str(baseline), str(target)], capture_output=True)
        if merged.returncode:
            raise RuntimeError(f'Conflicting offline account adaptation: {path}')
        target.write_bytes(merged.stdout)
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
for folder in ('graphics-compat/togl', 'graphics-compat/togles', 'togles', 'appframework', 'public/togl', 'public/togles', 'video', 'materialsystem/shaderapidx9'):
    for f in (a.output / folder).rglob('*'):
        if f.suffix not in ('.h', '.cpp', '.inl'):
            continue
        text = f.read_text()
        text = re.sub(r'#ifndef\s+OSX\b', '#if !defined(OSX) || defined(SOURCE_IOS)', text)
        text = re.sub(r'#ifdef\s+OSX\b', '#if defined(OSX) && !defined(SOURCE_IOS)', text)
        text = re.sub(r'defined\(\s*OSX\s*\)', '(defined(OSX) && !defined(SOURCE_IOS))', text)
        if folder != 'materialsystem/shaderapidx9':
            text = re.sub(r'defined\s*\(\s*(_?LINUX)\s*\)', r'(defined(\1) || defined(SOURCE_IOS))', text)
        f.write_text(text)
f = a.output / 'engine/audio/voice_mixer_controls_openal.cpp'
text = f.read_text().replace('#ifdef OSX', '#if defined(OSX) && !defined(SOURCE_IOS)').replace('#ifndef OSX', '#if !defined(OSX) || defined(SOURCE_IOS)')
f.write_text(text)
for path in ('engine/audio/snd_win.cpp', 'engine/audio/voice.cpp'):
    f = a.output / path
    text = f.read_text()
    text = re.sub(r'#ifdef\s+OSX\b', '#if defined(OSX) && !defined(SOURCE_IOS)', text)
    text = re.sub(r'defined\(\s*OSX\s*\)', '(defined(OSX) && !defined(SOURCE_IOS))', text)
    f.write_text(text)
replace('appframework/sdlmgr.cpp', '#include "tier1/convar.h"', '#include "tier1/convar.h"\n#include <dlfcn.h>\n#ifdef SOURCE_IOS\n#include "SDL_syswm.h"\n#endif')
# UIKit owns a nonzero drawable FBO and requires its color renderbuffer at swap.
# Keep the original texture resolve; only adapt the final SDL presentation.
replace('togles/linuxwin/glmgr.cpp', '\t\tif ( (gl_blitmode.GetInt() != 0) )',
        '\t\tif (\n#ifdef SOURCE_IOS\n\t\t\tfalse // SDL UIKit performs the final drawable blit.\n#else\n\t\t\t(gl_blitmode.GetInt() != 0)\n#endif\n\t\t)')
replace('appframework/sdlmgr.cpp', '\tif (params->m_onlySyncView)\n\t\treturn;', '''\tif (params->m_onlySyncView)
\t\treturn;
#ifdef SOURCE_IOS
\tSDL_SysWMinfo info;
\tSDL_VERSION(&info.version);
\tif (!SDL_GetWindowWMInfo(m_Window, &info) || info.subsystem != SDL_SYSWM_UIKIT) {
\t\tWarning("iOS presentation: UIKit drawable unavailable: %s\\n", SDL_GetError());
\t\treturn;
\t}
\tint width = 0, height = 0;
\tSDL_GL_GetDrawableSize(m_Window, &width, &height);
\tGLint readFBO = 0, drawFBO = 0, renderbuffer = 0;
\tgGL->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFBO);
\tgGL->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);
\tgGL->glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
\tGLboolean scissor = GL_FALSE, colorMask[4];
\tgGL->glGetBooleanv(GL_SCISSOR_TEST, &scissor);
\tgGL->glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
\tgGL->glDisable(GL_SCISSOR_TEST);
\tgGL->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
\tgGL->glBindFramebuffer(GL_READ_FRAMEBUFFER, m_readFBO);
\tgGL->glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
\t\tGL_TEXTURE_2D, params->m_srcTexName, 0);
\tgGL->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, info.info.uikit.framebuffer);
\tGLenum readStatus = gGL->glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
\tGLenum drawStatus = gGL->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
\tif (readStatus == GL_FRAMEBUFFER_COMPLETE && drawStatus == GL_FRAMEBUFFER_COMPLETE && width > 0 && height > 0)
\t\tgGL->glBlitFramebuffer(0, 0, params->m_width, params->m_height,
\t\t\t0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
\tGLenum blitError = gGL->glGetError();
\tgGL->glBindRenderbuffer(GL_RENDERBUFFER, info.info.uikit.colorbuffer);
\tCFastTimer timer;
\ttimer.Start();
\tSDL_GL_SwapWindow(m_Window);
\tm_flPrevGLSwapWindowTime = timer.GetDurationInProgress().GetMillisecondsF();
\tGLenum swapError = gGL->glGetError();
\tstatic unsigned frame = 0;
\t++frame;
\tif (frame <= 3 || frame == 120 || (frame % 3600) == 0)
\t\tWarning("iOS present: frame=%u texture=%u source=%dx%d drawable=%dx%d fbo=%u color=%u read=0x%x draw=0x%x blit=0x%x swap=0x%x\\n",
\t\t\tframe, params->m_srcTexName, params->m_width, params->m_height, width, height,
\t\t\tinfo.info.uikit.framebuffer, info.info.uikit.colorbuffer, readStatus, drawStatus, blitError, swapError);
\tgGL->glBindFramebuffer(GL_READ_FRAMEBUFFER, m_readFBO);
\tgGL->glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
\tgGL->glBindFramebuffer(GL_READ_FRAMEBUFFER, readFBO);
\tgGL->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);
\tgGL->glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
\tgGL->glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
\tif (scissor) gGL->glEnable(GL_SCISSOR_TEST);
\treturn;
#endif''')
replace('engine/sys_engine.cpp', 'void CEngine::Frame( void )\n{',
        'void CEngine::Frame( void )\n{\n#ifdef SOURCE_IOS\n\tstatic unsigned iosFrame = 0;\n\tif (++iosFrame <= 3 || iosFrame == 120) Warning("iOS engine frame: %u\\n", iosFrame);\n#endif')
replace('gameui/GameUI_Interface.cpp', 'void CGameUI::RunFrame()\n{',
        'void CGameUI::RunFrame()\n{\n#ifdef SOURCE_IOS\n\tstatic unsigned iosFrame = 0;\n\tif (++iosFrame <= 3 || iosFrame == 120) Warning("iOS GameUI frame: %u\\n", iosFrame);\n#endif')
# Use the same GLES entry-point layout as the linked ToGLES library on iOS.
replace('appframework/sdlmgr.cpp', '#include "togl/rendermechanism.h"',
        '#ifdef SOURCE_IOS\n#include "togles/rendermechanism.h"\n#else\n#include "togl/rendermechanism.h"\n#endif')
# ShaderAPI includes the desktop-named umbrella as well; it must share the
# GLES class layouts and inline methods with the linked renderer.
f = compat / 'togl/rendermechanism.h'
f.write_text('#ifdef SOURCE_IOS\n#include "togles/rendermechanism.h"\n#else\n' + f.read_text() + '\n#endif\n')
f = compat / 'togles/linuxwin/dxabstract.h'
text = f.read_text()
marker = text.index('// required for 10.6 support')
guard = text.rfind('#if ', 0, marker)
end = text.index('\n', guard)
f.write_text(text[:guard] + '#if defined(OSX) || defined(SOURCE_IOS)' + text[end:])
# GLES 3.0 has no desktop base-vertex draw entry point. Select ClientMod's
# existing path that offsets vertex bindings before glDrawRangeElements.
for path, count in (('togles/linuxwin/dxabstract.cpp', 1),
                    ('togles/linuxwin/glmgr.cpp', 1),
                    ('graphics-compat/togles/linuxwin/glmgr.h', 2)):
    replace(path, '#if 1 //ifndef OSX', '#if !defined(SOURCE_IOS) // desktop base-vertex path', count)
for path in ('graphics-compat/togles/linuxwin/glfuncs.h',):
    replace(path, '#if 1 //ifndef OSX', '#if !defined(SOURCE_IOS)')
    # Fixed-function desktop state is unused by the original GLES shaders.
    for name in ('glAlphaFunc', 'glColor4f', 'glClientActiveTexture', 'glGetTexLevelParameteriv'):
        f = a.output / path
        text = f.read_text()
        lines = text.splitlines(keepends=True)
        matches = [i for i, line in enumerate(lines) if f'GL_FUNC_VOID(OpenGL,true,{name},' in line]
        if len(matches) != 1:
            raise RuntimeError(f'GLES legacy state context changed: {path}: {name}')
        i = matches[0]
        lines[i] = '#ifndef SOURCE_IOS\n' + lines[i] + '#endif\n'
        f.write_text(''.join(lines))
replace('togles/linuxwin/glentrypoints.cpp', '\tconst int NEED_MINOR = 2;',
        '#ifdef SOURCE_IOS\n\tconst int NEED_MINOR = 0; // OpenGL ES 3.0\n#else\n\tconst int NEED_MINOR = 2;\n#endif')
replace('togles/linuxwin/glmgr.cpp', '(int)indicesActual + (int)pIndexBuf->m_pPseudoBuf',
        '(uintptr_t)indicesActual + (uintptr_t)pIndexBuf->m_pPseudoBuf')
replace('togles/linuxwin/glmgr.cpp', '(int)indicesActual + (int)pIndexBuf->m_nPersistentBufferStartOffset',
        '(uintptr_t)indicesActual + (uintptr_t)pIndexBuf->m_nPersistentBufferStartOffset')
# Keep real GPU buffers, but use ClientMod's existing CPU staging/SubData
# upload path for bounded vertex/index writes on iOS, including initial writes.
# Client-side pseudo buffers are not suitable for GLES vertex attributes.
replace('togles/linuxwin/cglmbuffer.cpp', '\tchar *resultPtr = NULL;',
        '\tchar *resultPtr = NULL;\n#ifdef SOURCE_IOS\n\t*pAddressOut = NULL;\n\tif (m_bMapped || pParams->m_nOffset >= m_nSize || pParams->m_nSize > m_nSize - pParams->m_nOffset)\n\t\tWarning("iOS GL buffer lock rejected: handle=%u mapped=%d buffer=%u offset=%u size=%u\\n", m_nHandle, m_bMapped, m_nSize, pParams->m_nOffset, pParams->m_nSize);\n#endif')
replace('togles/linuxwin/cglmbuffer.cpp',
        'else if ( !g_bDisableStaticBuffer && ( pParams->m_bDiscard || pParams->m_bNoOverwrite ) && ( pParams->m_nSize <= GL_STATIC_BUFFER_SIZE ) )',
        'else if (\n#ifdef SOURCE_IOS\n\t\t( m_type == kGLMVertexBuffer || m_type == kGLMIndexBuffer ) &&\n#else\n\t\t!g_bDisableStaticBuffer && ( pParams->m_bDiscard || pParams->m_bNoOverwrite ) &&\n#endif\n\t\t( pParams->m_nSize <= GL_STATIC_BUFFER_SIZE ) )')
replace('togles/linuxwin/cglmbuffer.cpp',
        '\t\tmapPtr = (char*)gGL->glMapBufferRange( m_buffGLTarget, pParams->m_nOffset, pParams->m_nSize, parms);',
        '\t\tmapPtr = (char*)gGL->glMapBufferRange( m_buffGLTarget, pParams->m_nOffset, pParams->m_nSize, parms);\n#ifdef SOURCE_IOS\n\t\tif (!mapPtr) {\n\t\t\tWarning("iOS GL buffer map failed: handle=%u target=0x%x buffer=%u offset=%u size=%u flags=0x%x error=0x%x\\n", m_nHandle, m_buffGLTarget, m_nSize, pParams->m_nOffset, pParams->m_nSize, parms, gGL->glGetError());\n\t\t\t*pAddressOut = NULL;\n\t\t\treturn;\n\t\t}\n#endif')
replace('togles/linuxwin/glmgrbasics.cpp', '\tsystem( temp );',
        '#ifndef SOURCE_IOS\n\tsystem( temp );\n#else\n\tWarning("Desktop shader editor is unavailable on iOS.\\n");\n#endif')
replace('launcher/launcher.cpp', '\t\t\t\tsystem( szOpenLine );',
        '#ifndef SOURCE_IOS\n\t\t\t\tsystem( szOpenLine );\n#else\n\t\t\t\tWarning("Desktop process relaunch is unavailable on iOS.\\n");\n#endif')
# iOS cannot open the desktop /tmp singleton lock. Retain the original POSIX
# fcntl lock and TMPDIR selection inside the application's sandbox.
replace('launcher/launcher.cpp', '#elif defined (LINUX) || defined(PLATFORM_BSD)',
        '#elif defined (LINUX) || defined(PLATFORM_BSD) || defined(SOURCE_IOS)')
replace('engine/sys_mainwind.cpp', '#ifdef OSX\n\tid nsWindow',
        '#ifdef SOURCE_IOS\n\treturn (void*)pInfo.info.uikit.window;\n#elif defined(OSX)\n\tid nsWindow')
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
