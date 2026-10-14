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

# GLES 3 sampler objects avoid modifying texture objects each time filtering
# changes. Keep the original texture-state path for unsupported border modes.
replace('togles/linuxwin/glmgr.cpp',
        '\tGLMDebugPrintf( "GL sampler object usage: %s\\n", m_bUseSamplerObjects ? "ENABLED" : "DISABLED" );',
        '''#ifdef SOURCE_IOS
    m_bUseSamplerObjects = true;
    Warning("iOS GLES sampler cache enabled; unsupported border states retain texture path\\n");
#endif
\tGLMDebugPrintf( "GL sampler object usage: %s\\n", m_bUseSamplerObjects ? "ENABLED" : "DISABLED" );''')
for path in ('public/togles/linuxwin/glmgr.h', 'graphics-compat/togles/linuxwin/glmgr.h'):
    p = root / path
    p.write_text('#ifdef SOURCE_IOS\n#include "IOSSamplerBindingCache.hpp"\n#endif\n' + p.read_text())
    replace(path, '\t\tFORCEINLINE GLuint FindSamplerObject( const GLMTexSamplingParams &desiredParams );',
            '''\t\tFORCEINLINE GLuint FindSamplerObject( const GLMTexSamplingParams &desiredParams );
#ifdef SOURCE_IOS
        IOSSamplerBindingCache<GLM_SAMPLER_COUNT> m_iosSamplerBindings;
        FORCEINLINE void IOSBindSampler(unsigned unit, unsigned name) {
            m_iosSamplerBindings.bind(unit, name,
                [](unsigned index, unsigned object) { gGL->glBindSampler(index, object); });
        }
#endif''')
replace('togles/linuxwin/glmgr.cpp', '\t\tgGL->glBindSampler( 15, 0 );',
        '#ifdef SOURCE_IOS\n\t\tIOSBindSampler(15, 0);\n#else\n\t\tgGL->glBindSampler( 15, 0 );\n#endif')
replace('togles/linuxwin/glmgr_flush.inl',
        '\t\t\tgGL->glBindSampler( nSamplerIndex, FindSamplerObject( m_samplers[nSamplerIndex].m_samp ) );',
        '''#ifdef SOURCE_IOS
            const GLMTexSamplingParams &desired = m_samplers[nSamplerIndex].m_samp;
            if (desired.m_packed.m_addressU >= D3DTADDRESS_BORDER ||
                desired.m_packed.m_addressV >= D3DTADDRESS_BORDER ||
                desired.m_packed.m_addressW >= D3DTADDRESS_BORDER) {
                IOSBindSampler(nSamplerIndex, 0);
                CGLMTex *texture = m_samplers[nSamplerIndex].m_pBoundTex;
                if (texture && !(texture->m_SamplingParams == desired)) {
                    SelectTMU(nSamplerIndex);
                    m_samplers[nSamplerIndex].m_samp.DeltaSetToTarget(texture->m_texGLTarget, texture->m_SamplingParams);
                    texture->m_SamplingParams = desired;
                }
            } else {
                IOSBindSampler(nSamplerIndex, FindSamplerObject(desired));
            }
#else
\t\t\tgGL->glBindSampler( nSamplerIndex, FindSamplerObject( m_samplers[nSamplerIndex].m_samp ) );
#endif''')
for path in ('public/togles/linuxwin/cglmtex.h', 'graphics-compat/togles/linuxwin/cglmtex.h'):
    replace(path,
            '\t\tgGL->glSamplerParameterfv( nSamplerObject, GL_TEXTURE_BORDER_COLOR, flBorderColor ); // <-- this crashes ATI\'s driver, remark it out',
            '#ifndef SOURCE_IOS\n\t\tgGL->glSamplerParameterfv( nSamplerObject, GL_TEXTURE_BORDER_COLOR, flBorderColor ); // <-- this crashes ATI\'s driver, remark it out\n#endif')

# CPU wall timings include time spent inside the GL driver. These are not
# timestamp-query GPU measurements and must not be interpreted as such.
def time_function(path, signature, phase, count=1):
    p = root / path
    text = p.read_text()
    marker = signature
    starts = []
    position = 0
    while True:
        position = text.find(marker, position)
        if position < 0: break
        starts.append(text.index('\n{', position) + 2)
        position += len(marker)
    assert len(starts) == count, (path, signature, starts)
    for position in reversed(starts):
        text = text[:position] + '\n\tIOS_RENDER_TIMING("' + phase + '");' + text[position:]
    if '#include "IOSRenderTiming.hpp"' not in text:
        text = '#include "IOSRenderTiming.hpp"\n' + text
    p.write_text(text)

for signature, phase in (
    ('void CRendering3dView::BuildWorldRenderLists(', 'world_list_build'),
    ('void CRendering3dView::BuildRenderableRenderLists(', 'entity_list_build'),
    ('void CRendering3dView::DrawWorld(', 'world_draw'),
    ('void CRendering3dView::DrawOpaqueRenderables(', 'opaque_models'),
    ('void CRendering3dView::DrawTranslucentRenderables(', 'translucent_draw'),
    ('void CViewRender::DrawViewModels(', 'viewmodel_draw'),
):
    time_function('game/client/viewrender.cpp', signature, phase)
time_function('game/client/viewpostprocess.cpp', 'void DoEnginePostProcessing(', 'postprocessing')
time_function('togles/linuxwin/dxabstract.cpp', 'HRESULT IDirect3DDevice9::DrawIndexedPrimitive(', 'backend_indexed_draw', 2)
time_function('togles/linuxwin/glmgr_flush.inl', 'FORCEINLINE void GLMContext::FlushDrawStates(', 'backend_draw_state')
time_function('togles/linuxwin/cglmquery.cpp', 'void\tCGLMQuery::Complete(', 'query_complete')
