#!/usr/bin/env python3
"""Use indexed GLES draws without an unneeded desktop vertex-range hint.

Run after prepare_ios_vertex_submission.py. Actual index values and primitive
counts are unchanged; the iOS vertex binding path already applies base vertex.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1])

def replace(path, old, new, count=1):
    p = root / path
    text = p.read_text()
    assert text.count(old) == count, (path, old, text.count(old))
    p.write_text(text.replace(old, new))

# Keep both headers byte-identical in entrypoint order/class layout.
for path in ('public/togles/linuxwin/glfuncs.h', 'graphics-compat/togles/linuxwin/glfuncs.h'):
    replace(path,
        'GL_FUNC_VOID(OpenGL,true,glDrawRangeElements,(GLenum a,GLuint b,GLuint c,GLsizei d,GLenum e,const GLvoid *f),(a,b,c,d,e,f))',
        '''GL_FUNC_VOID(OpenGL,true,glDrawRangeElements,(GLenum a,GLuint b,GLuint c,GLsizei d,GLenum e,const GLvoid *f),(a,b,c,d,e,f))
#ifdef SOURCE_IOS
GL_FUNC_VOID(OpenGL,true,glDrawElements,(GLenum a,GLsizei b,GLenum c,const GLvoid *d),(a,b,c,d))
#endif''')

p = root / 'togles/linuxwin/glmgr.cpp'
text = p.read_text()
lines = text.splitlines(keepends=True)
calls = 0
for i, line in enumerate(lines):
    if line.strip() != 'gGL->glDrawRangeElements(mode, start, end, count, type, indicesActual);':
        continue
    lines[i] = '''#ifdef SOURCE_IOS
        { IOS_RENDER_TIMING("backend_gl_draw");
          gGL->glDrawElements(mode, count, type, indicesActual); }
#else
''' + line + '#endif\n'
    calls += 1
assert calls == 2, calls
p.write_text(''.join(lines))
