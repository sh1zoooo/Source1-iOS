#!/usr/bin/env python3
"""Fix the GLES base-vertex fallback and remove redundant vertex work.

Run after prepare_clientmod_graphics.py. Both public layouts must be patched.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1])

def replace(path, old, new, count=1):
    p = root / path
    text = p.read_text()
    assert text.count(old) == count, (path, old, text.count(old))
    p.write_text(text.replace(old, new))

replace('togles/linuxwin/dxabstract.cpp',
    '\tthis->FlushVertexBindings( BaseVertexIndex );\n\tm_ctx->FlushDrawStates( MinVertexIndex, MinVertexIndex + NumVertices - 1, 0 );',
    '''#ifdef SOURCE_IOS
    // Active FlushDrawStates reads device streams directly. The old OSX
    // setup written by FlushVertexBindings is unused by that implementation.
    m_ctx->FlushDrawStates(MinVertexIndex, MinVertexIndex + NumVertices - 1, BaseVertexIndex);
#else
\tthis->FlushVertexBindings( BaseVertexIndex );
\tm_ctx->FlushDrawStates( MinVertexIndex, MinVertexIndex + NumVertices - 1, 0 );
#endif''')

for path in ('public/togles/linuxwin/glmgr.h', 'graphics-compat/togles/linuxwin/glmgr.h'):
    replace(path, '\t\t\tuint m_nTotalBufferRevision;', '''#ifdef SOURCE_IOS
            uint m_iosBaseVertex;
#endif
\t\t\tuint m_nTotalBufferRevision;''')
    replace(path, '\t\t\tm_CurAttribs.m_nTotalBufferRevision = 0;', '''#ifdef SOURCE_IOS
            m_CurAttribs.m_iosBaseVertex = ~0u;
#endif
\t\t\tm_CurAttribs.m_nTotalBufferRevision = 0;''')
    replace(path, '\t\t\tuint m_revision;', '''#ifdef SOURCE_IOS
            GLuint m_iosBuffer;
#endif
\t\t\tuint m_revision;''')
    replace(path, '\t\t\tVertexAttribs_t &curAttribs = m_boundVertexAttribs[nIndex];', '''\t\t\tVertexAttribs_t &curAttribs = m_boundVertexAttribs[nIndex];
#ifdef SOURCE_IOS
            // GL_ARRAY_BUFFER is captured per attribute by VertexAttribPointer.
            // A cache hit needs neither rebinding nor reissuing the pointer.
            if (curAttribs.m_iosBuffer == nGLName && curAttribs.m_pPtr == pBuf &&
                curAttribs.m_revision == nRevision && curAttribs.m_stride == stride &&
                curAttribs.m_datatype == datatype && curAttribs.m_normalized == normalized &&
                curAttribs.m_nCompCount == nCompCount) return;
#endif''')
    replace(path, '\t\t\telse if ( ( curAttribs.m_pPtr == pBuf ) &&', '''#ifndef SOURCE_IOS
\t\t\telse if ( ( curAttribs.m_pPtr == pBuf ) &&''')
    replace(path, '\t\t\tcurAttribs.m_nCompCount = nCompCount;', '''#endif
#ifdef SOURCE_IOS
            curAttribs.m_iosBuffer = nGLName;
#endif
\t\t\tcurAttribs.m_nCompCount = nCompCount;''')

replace('togles/linuxwin/glmgr_flush.inl',
    '\tif ( ( nCurTotalBufferRevision != m_CurAttribs.m_nTotalBufferRevision ) ||',
    '''\tif (
#ifdef SOURCE_IOS
        (m_CurAttribs.m_iosBaseVertex != nBaseVertex) ||
#endif
        ( nCurTotalBufferRevision != m_CurAttribs.m_nTotalBufferRevision ) ||''')
replace('togles/linuxwin/glmgr_flush.inl',
    '\t\tm_CurAttribs.m_nTotalBufferRevision = nCurTotalBufferRevision;',
    '''#ifdef SOURCE_IOS
        m_CurAttribs.m_iosBaseVertex = nBaseVertex;
#endif
\t\tm_CurAttribs.m_nTotalBufferRevision = nCurTotalBufferRevision;''')
replace('togles/linuxwin/glmgr_flush.inl',
    '\t\t\tint nBufOffset = pDeclElem->m_gldecl.m_offset + pStream->m_offset;',
    '''#ifdef SOURCE_IOS
            // D3D base vertex is signed; offsets use pointer-width arithmetic.
            intp nBufOffset = (intp)pDeclElem->m_gldecl.m_offset + pStream->m_offset
                + (intp)(int32)nBaseVertex * pStream->m_stride;
#else
\t\t\tint nBufOffset = pDeclElem->m_gldecl.m_offset + pStream->m_offset;
#endif''')
