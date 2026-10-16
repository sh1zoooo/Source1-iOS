#!/usr/bin/env python3
"""Correct static buffer placement hints; count driver work without visual changes.
Run after existing performance patches and optional indexed-draw patch.
"""
from pathlib import Path
import sys
root=Path(sys.argv[1])
def replace(path,old,new,count=1):
    p=root/path; text=p.read_text()
    assert text.count(old)==count,(path,old,text.count(old))
    p.write_text(text.replace(old,new))
for path in ('togles/linuxwin/cglmbuffer.cpp','togles/linuxwin/glmgr.cpp'):
    p=root/path
    p.write_text('#include "IOSRenderWorkStats.hpp"\n'+p.read_text())
# Immutable geometry was labelled STREAM despite being written once. Preserve
# all dynamic/streaming/orphan semantics and restrict this hint to static V/IBs.
for case in ('kGLMVertexBuffer','kGLMIndexBuffer'):
    old=f'case {case}:\thint = m_bDynamic ? GL_DYNAMIC_DRAW : GL_STREAM_DRAW; break;'
    replace('togles/linuxwin/cglmbuffer.cpp',old,
        '#ifdef SOURCE_IOS\n\t\t\t'+old.replace('GL_STREAM_DRAW','GL_STATIC_DRAW')+
        '\n#else\n\t\t\t'+old+'\n#endif')
replace('togles/linuxwin/cglmbuffer.cpp',
    '\tif ( nActualSize > (int)m_LockParams.m_nSize )',
    '''#ifdef SOURCE_IOS
    if (nActualSize >= 0 && nActualSize <= (int)m_LockParams.m_nSize &&
        (m_type == kGLMVertexBuffer || m_type == kGLMIndexBuffer))
        IOSRenderWork().write(m_type == kGLMIndexBuffer, m_bDynamic, (unsigned)nActualSize);
#endif
\tif ( nActualSize > (int)m_LockParams.m_nSize )''')
# Record orphan commands in Lock only, not initial constructor allocations.
p=root/'togles/linuxwin/cglmbuffer.cpp';text=p.read_text()
start=text.index('void CGLMBuffer::Lock(');end=text.index('void CGLMBuffer::Unlock(',start)
lock=text[start:end];old='gGL->glBufferData( m_buffGLTarget, m_nSize, (const GLvoid*)NULL, hint );'
assert lock.count(old)==2,lock.count(old)
lock=lock.replace(old,old+'\n#ifdef SOURCE_IOS\n            IOSRenderWork().orphan(m_nSize);\n#endif')
p.write_text(text[:start]+lock+text[end:])
replace('togles/linuxwin/glmgr.cpp','void GLMContext::BeginFrame( void )\n{',
    'void GLMContext::BeginFrame( void )\n{\n#ifdef SOURCE_IOS\n    IOSRenderWork().report(Plat_FloatTime());\n#endif')
# Only the OSX-style fallback is compiled on native iOS. Instrument submitted
# ranges there, outside the raw GL timing owned by prepare_ios_indexed_draw.py.
replace('togles/linuxwin/glmgr.cpp',
    'void GLMContext::DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, CGLMBuffer *pIndexBuf)\n{',
    '''void GLMContext::DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, CGLMBuffer *pIndexBuf)
{
#ifdef SOURCE_IOS
    if (m_pBoundPair && count > 0)
        IOSRenderWork().draw(mode, (unsigned)count, start, end, pIndexBuf->m_bDynamic);
#endif''')
for signature,phase in [('void IDirect3DDevice9::UpdateBoundFBO()','backend_fbo_update'),
                        ('HRESULT IDirect3DDevice9::FlushIndexBindings( void )','backend_index_bind')]:
    replace('togles/linuxwin/dxabstract.cpp',signature+'\n{',signature+'\n{\n\tIOS_RENDER_TIMING("'+phase+'");')
