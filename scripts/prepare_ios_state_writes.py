#!/usr/bin/env python3
"""Coalesce audited GLES state writes; never coalesce explicit Flush.

Install IOSStateWriteStats.hpp under src/source. Run after other iOS patches.
States altered by raw viewport/scissor/mask/enable calls are deliberately excluded.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1])
safe = ['GLCullFrontFace_t', 'GLDepthBias_t', 'GLViewportDepthRange_t',
        'GLBlendFactor_t', 'GLBlendEquation_t', 'GLBlendColor_t',
        'GLDepthFunc_t', 'GLStencilFunc_t', 'GLClearDepth_t', 'GLClearStencil_t']
policy = '''#ifdef SOURCE_IOS
// Audited: no raw setter outside GLContextSet mutates these states.
// Flush remains unconditional for explicit restoration and context resets.
template<class T> struct IOSCoalesceGLState { static constexpr bool value = false; };
''' + ''.join('template<> struct IOSCoalesceGLState<' + name +
              '> { static constexpr bool value = true; };\n' for name in safe) + '#endif\n\n'

for relative in ('public/togles/linuxwin/glmgr.h', 'graphics-compat/togles/linuxwin/glmgr.h'):
    path = root / relative
    text = path.read_text()
    text = '#ifdef SOURCE_IOS\n#include "IOSStateWriteStats.hpp"\n#endif\n' + text
    marker = 'template<typename T> class GLState\n'
    assert text.count(marker) == 1
    text = text.replace(marker, policy + marker)
    old = '''\t\tFORCEINLINE void Write( const T *src )
\t\t{
\t\t\tdata = *src;
\t\t\tFlush();
\t\t}'''
    new = '''\t\tFORCEINLINE void Write( const T *src )
\t\t{
#ifdef SOURCE_IOS
            if (IOSCoalesceGLState<T>::value) {
                // First Write always submits, independent of constructor defaults.
                if (m_iosWritten && data == *src) {
                    ++IOSStateWrites().skipped;
                    return;
                }
                m_iosWritten = true;
            }
#endif
\t\t\tdata = *src;
\t\t\tFlush();
\t\t}'''
    assert text.count(old) == 1
    text = text.replace(old, new)
    # Only the single-state template is changed. Array states keep original
    # write-through behavior until their indexed setter mutations are audited.
    old = '\t\t\tGLContextSet( &data );'
    new = '''#ifdef SOURCE_IOS
            if (IOSCoalesceGLState<T>::value) ++IOSStateWrites().submitted;
#endif
\t\t\tGLContextSet( &data );'''
    assert text.count(old) == 1
    text = text.replace(old, new)
    old = '\t\tT data;'
    assert text.count(old) == 1
    text = text.replace(old, '''#ifdef SOURCE_IOS
        bool m_iosWritten = false;
#endif
\t\tT data;''')
    path.write_text(text)

path = root / 'togles/linuxwin/glmgr.cpp'
text = path.read_text()
anchor = '    IOSRenderWork().report(Plat_FloatTime());'
assert text.count(anchor) == 1
path.write_text(text.replace(anchor, anchor + '\n    IOSStateWrites().report(Plat_FloatTime());'))
