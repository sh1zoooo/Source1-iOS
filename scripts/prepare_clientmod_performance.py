#!/usr/bin/env python3
"""Apply iOS render optimizations after the graphical portability patches."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
def replace(path, old, new, count=1):
    p = root / path
    text = p.read_text()
    if text.count(old) != count:
        raise RuntimeError(f'Performance patch context changed: {path}: {old}')
    p.write_text(text.replace(old, new))

# Desktop macOS retains a second CPU copy to recreate textures when changing
# their sRGB format. iOS uses immutable channel swizzles, not that flip path.
# Existing Unlock frees 2D backing when this is false; 3D backing is preserved.
replace('togles/linuxwin/glmgr.cpp',
        '\tm_bTexClientStorage = !gGL->m_bHave_GL_EXT_texture_sRGB_decode;',
        '''#ifdef SOURCE_IOS
    m_bTexClientStorage = false;
    Warning("iOS renderer optimizations: uniform cache, async streaming, release 2D CPU texture backing\\n");
#else
\tm_bTexClientStorage = !gGL->m_bHave_GL_EXT_texture_sRGB_decode;
#endif''')

# Every module must see the same GLES class layout.
for path in ('public/togles/linuxwin/cglmprogram.h', 'graphics-compat/togles/linuxwin/cglmprogram.h'):
    p = root / path
    p.write_text('#ifdef SOURCE_IOS\n#include "IOSUniformCache.hpp"\n#endif\n' + p.read_text())
    replace(path, '\tGLint\t\t\t\t\tm_locAlphaRef; // "alpha_ref"', '''#ifdef SOURCE_IOS
    IOSUniformCache<> m_iosUniforms[2];
    float m_iosAlphaRef = -1.0f;
    FORCEINLINE void IOSSetUniform4fv(int stage, int first, GLint location, int count, const float *values) {
        if (location < 0 || count <= 0) return;
        m_iosUniforms[stage].update(first, count, values,
            [location](int offset, int slots, const float *data) {
                gGL->glUniform4fv(location + offset, slots, data);
            });
    }
#endif
\tGLint\t\t\t\t\tm_locAlphaRef; // "alpha_ref"''')

replace('togles/linuxwin/cglmprogram.cpp', 'm_nScreenWidthHeight = 0xFFFFFFFF;', '''m_nScreenWidthHeight = 0xFFFFFFFF;
#ifdef SOURCE_IOS
    m_iosUniforms[0].reset(); m_iosUniforms[1].reset(); m_iosAlphaRef = -1.0f;
#endif''', 3)

p = root / 'togles/linuxwin/glmgr_flush.inl'
text = p.read_text()
lines = text.splitlines(keepends=True)
calls = 0
for i, line in enumerate(lines):
    if 'gGL->glUniform4fv(' not in line:
        continue
    call = line.strip()
    args = call[len('gGL->glUniform4fv('):-2].strip()
    if 'vconstBoneLoc' in call:
        stage, first = 'kGLMVertexProgram', 'DXABSTRACT_VS_FIRST_BONE_SLOT'
    elif 'DXABSTRACT_VS_LAST_BONE_SLOT+1' in call:
        stage, first = 'kGLMVertexProgram', '(DXABSTRACT_VS_LAST_BONE_SLOT+1)'
    else:
        stage = 'kGLMFragmentProgram' if 'kGLMFragmentProgram' in call else 'kGLMVertexProgram'
        first = 'firstDirtySlot'
    lines[i] = '#ifdef SOURCE_IOS\n\t\t\t\tm_pBoundPair->IOSSetUniform4fv(' + stage + ', ' + first + ', ' + args + ');\n#else\n' + line + '#endif\n'
    calls += 1
assert calls == 5
p.write_text(''.join(lines))

replace('togles/linuxwin/glmgr_flush.inl', '''\t\tif( !m_AlphaTestEnable.GetData().enable )
\t\t\tgGL->glUniform1f( m_pBoundPair->m_locAlphaRef, 0.0 );
\t\telse
\t\t\tgGL->glUniform1f( m_pBoundPair->m_locAlphaRef, m_AlphaTestFunc.GetData().ref );''', '''#ifdef SOURCE_IOS
        const float alpha = m_AlphaTestEnable.GetData().enable ? m_AlphaTestFunc.GetData().ref : 0.0f;
        if (m_pBoundPair->m_iosAlphaRef != alpha) {
            gGL->glUniform1f(m_pBoundPair->m_locAlphaRef, alpha);
            m_pBoundPair->m_iosAlphaRef = alpha;
        }
#else
\t\tif( !m_AlphaTestEnable.GetData().enable )
\t\t\tgGL->glUniform1f( m_pBoundPair->m_locAlphaRef, 0.0 );
\t\telse
\t\t\tgGL->glUniform1f( m_pBoundPair->m_locAlphaRef, m_AlphaTestFunc.GetData().ref );
#endif''')

# Keep CPU staging for initial/static writes. Dynamic DISCARD/NOOVERWRITE
# locks can use the existing GLES map path without a synchronous SubData copy.
replace('togles/linuxwin/cglmbuffer.cpp', '''( m_type == kGLMVertexBuffer || m_type == kGLMIndexBuffer ) &&''', '''( m_type == kGLMVertexBuffer || m_type == kGLMIndexBuffer ) &&
        !(m_bDynamic && (pParams->m_bDiscard || pParams->m_bNoOverwrite)) &&''')
replace('togles/linuxwin/cglmbuffer.cpp', '''\t\t\t*pAddressOut = NULL;
\t\t\treturn;
\t\t}
#endif''', '''            if ((m_type == kGLMVertexBuffer || m_type == kGLMIndexBuffer) && pParams->m_nSize <= GL_STATIC_BUFFER_SIZE) {
                m_pStaticBuffer = m_StaticBuffers[m_type == kGLMVertexBuffer ? 0 : 1];
                mapPtr = m_pStaticBuffer;
            } else {
                *pAddressOut = NULL;
                return;
            }
\t\t}
#endif''')
replace('togles/linuxwin/cglmbuffer.cpp', '\tchar *resultPtr = NULL;', '\tIOS_RENDER_TIMING("buffer_lock");\n\tchar *resultPtr = NULL;')
replace('togles/linuxwin/cglmbuffer.cpp', 'void CGLMBuffer::Unlock( int nActualSize, const void *pActualData )\n{',
        'void CGLMBuffer::Unlock( int nActualSize, const void *pActualData )\n{\n\tIOS_RENDER_TIMING("buffer_unlock");')

# SDL keeps window dimensions in points; allow a full-density EAGL drawable.
replace('appframework/sdlmgr.cpp', '\tflags |= SDL_WINDOW_OPENGL;',
        '\tflags |= SDL_WINDOW_OPENGL;\n#ifdef SOURCE_IOS\n\tflags |= SDL_WINDOW_ALLOW_HIGHDPI;\n#endif')
