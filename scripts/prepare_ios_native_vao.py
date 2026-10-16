#!/usr/bin/env python3
"""Apply after performance, vertex-submission, and indexed-draw preparation.
Install IOSNativeVAOCache.hpp in the existing src/source include directory.
"""
from pathlib import Path
import sys

def apply(root):
    def replace(path, old, new, count=1):
        p = root / path
        text = p.read_text()
        assert text.count(old) == count, (path, old, text.count(old))
        p.write_text(text.replace(old, new))

    for path in ('public/togles/linuxwin/glmgr.h', 'graphics-compat/togles/linuxwin/glmgr.h'):
        p = root / path
        p.write_text('#ifdef SOURCE_IOS\n#include "IOSNativeVAOCache.hpp"\n#endif\n' + p.read_text())
        replace(path, '\tvoid ProcessTextureDeletes();', '''#ifdef SOURCE_IOS
    IOSNativeVAOCache<> m_iosVertexArrays;
    double m_iosVertexArrayReportTime = 0.0;
    void IOSDrawIndexed(GLenum mode, GLsizei count, GLenum type,
                        const void *indices, CGLMBuffer *indexBuffer);
#endif
\tvoid ProcessTextureDeletes();''')

    replace('togles/linuxwin/glmgr.cpp',
        'gGL->glDrawElements(mode, count, type, indicesActual);',
        'IOSDrawIndexed(mode, count, type, indicesActual, pIndexBuf);', 2)
    replace('togles/linuxwin/glmgr.cpp', 'GLMContext::~GLMContext\t()\n{',
        '''GLMContext::~GLMContext\t()
{
#ifdef SOURCE_IOS
    CheckCurrent();
    m_iosVertexArrays.clear(*gGL);
#endif''')
    replace('togles/linuxwin/cglmbuffer.cpp',
        '\t\tgGL->glDeleteBuffers( 1, &m_nHandle );',
        '''#ifdef SOURCE_IOS
        m_pCtx->m_iosVertexArrays.invalidateBuffer(*gGL, m_nHandle);
#endif
\t\tgGL->glDeleteBuffers( 1, &m_nHandle );''')
    replace('togles/linuxwin/glmgr.cpp',
        '    IOSRenderWork().report(Plat_FloatTime());',
        '''    IOSRenderWork().report(Plat_FloatTime());
    const double now = Plat_FloatTime();
    if (now - m_iosVertexArrayReportTime >= 5.0) {
        m_iosVertexArrayReportTime = now;
        const auto &s = m_iosVertexArrays.counters;
        Warning("iOS native VAO cache: hits=%llu misses=%llu evictions=%llu invalidations=%llu bypasses=%llu admissions=%llu\\n",
            (unsigned long long)s.hits, (unsigned long long)s.misses,
            (unsigned long long)s.evictions, (unsigned long long)s.invalidations,
            (unsigned long long)s.bypasses, (unsigned long long)s.admissions);
    }
''')
    p = root / 'togles/linuxwin/glmgr.cpp'
    p.write_text(p.read_text() + '''
#ifdef SOURCE_IOS
void GLMContext::IOSDrawIndexed(GLenum mode, GLsizei count, GLenum type,
                                const void *indices, CGLMBuffer *indexBuffer)
{
    // Persistent allocations have a separate lifetime from CGLMBuffer. Keep
    // their original path until those allocations participate in invalidation.
    if (indexBuffer->m_bPseudo || indexBuffer->m_bUsingPersistentBuffer ||
        gGL->m_bHave_GL_EXT_buffer_storage) {
        gGL->glDrawElements(mode, count, type, indices);
        return;
    }
    IOSNativeVAOCache<>::Key key;
    key.mask = m_lastKnownVertexAttribMask;
    key.indexBuffer = m_nBoundGLBuffer[kGLMIndexBuffer];
    for (unsigned i = 0; i < 16; ++i) {
        if (!(key.mask & (1u << i))) continue;
        const VertexAttribs_t &source = m_boundVertexAttribs[i];
        auto &a = key.attributes[i];
        a.buffer = source.m_iosBuffer;
        a.stride = source.m_stride;
        a.type = source.m_datatype;
        a.components = source.m_nCompCount;
        a.offset = reinterpret_cast<uintptr_t>(source.m_pPtr);
        a.normalized = source.m_normalized != 0;
    }
    m_iosVertexArrays.draw(*gGL, key, m_nBoundGLBuffer[kGLMVertexBuffer],
                          mode, count, type, indices);

}
#endif
''')

if __name__ == '__main__':
    apply(Path(sys.argv[1]))
