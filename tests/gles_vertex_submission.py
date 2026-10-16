"""Exercise production GLES pointer caching and nonzero base-vertex offsets.

Pass a generated clientmod-port directory patched by prepare_ios_vertex_submission.
Extracts the actual production inline method (not a duplicate cache implementation).
"""
import ctypes as c
from pathlib import Path
import subprocess
import sys
import tempfile
import gles_texture_channels as gl

root = Path(sys.argv[1])
header = (root / 'graphics-compat/togles/linuxwin/glmgr.h').read_text()
flush = (root / 'togles/linuxwin/glmgr_flush.inl').read_text()
draw = (root / 'togles/linuxwin/dxabstract.cpp').read_text()

def block(text, marker):
    start = text.index(marker)
    begin = text.index('{', start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

method = block(header, 'FORCEINLINE void SetBufAndVertexAttribPointer(')
structure = block(header, 'struct VertexAttribs_t') + ';'
# Missing streams and inactive attributes must retain the original disable path.
assert 'if ( pBuf == m_pDevice->m_pDummy_vtx_buffer )' in flush
assert 'gGL->glDisableVertexAttribArray( nIndex );' in flush
assert '(m_CurAttribs.m_iosBaseVertex != nBaseVertex)' in flush
assert 'm_CurAttribs.m_iosBaseVertex = nBaseVertex;' in flush
assert 'MinVertexIndex + NumVertices - 1, BaseVertexIndex);' in draw
start = flush.index('intp nBufOffset =')
expression = flush[start:flush.index(';', start) + 1]

source = '''#include <cstdint>
#include <cstring>
#define SOURCE_IOS 1
#define FORCEINLINE inline
using uint = unsigned; using int32 = int32_t; using intp = intptr_t;
using GLuint = unsigned; using GLenum = unsigned; using GLboolean = unsigned char;
enum {kGLMVertexBuffer=0, GL_ARRAY_BUFFER=0x8892};
struct GL { void (*glBindBuffer)(GLenum,GLuint);
 void (*glVertexAttribPointer)(GLuint,int,GLenum,GLboolean,int,const void*); } api;
GL *gGL=&api;
struct Context { STRUCTURE
 VertexAttribs_t m_boundVertexAttribs[16]; unsigned m_nBoundGLBuffer[2];
 Context(){ memset(this,255,sizeof(*this)); } METHOD
} context;
extern "C" {
void init(decltype(api.glBindBuffer) bind,decltype(api.glVertexAttribPointer) pointer){
 api.glBindBuffer=bind;api.glVertexAttribPointer=pointer;}
void apply(unsigned attr,unsigned buffer,unsigned stride,unsigned type,unsigned char norm,
 unsigned components,intptr_t offset,unsigned revision){
 context.SetBufAndVertexAttribPointer(attr,buffer,stride,type,norm,components,(void*)offset,revision);}
intptr_t offset(int decl,unsigned stream,int base,unsigned stride){
 struct {struct {int m_offset;} m_gldecl;} declaration{{decl}};
 struct {unsigned m_offset; unsigned m_stride;} streamDesc{stream,stride};
 auto *pDeclElem=&declaration;auto *pStream=&streamDesc;unsigned nBaseVertex=(unsigned)base;
 EXPRESSION
 return nBufOffset; }
}
'''.replace('STRUCTURE', structure).replace('METHOD', method).replace('EXPRESSION', expression)

with tempfile.TemporaryDirectory() as folder:
    path = Path(folder)
    (path / 'bridge.cpp').write_text(source)
    subprocess.run(['c++', '-std=c++17', '-shared', '-fPIC', '-O2', str(path/'bridge.cpp'),
                    '-o', str(path/'bridge.so')], check=True)
    bridge = c.CDLL(str(path/'bridge.so'))
    bind_type = c.CFUNCTYPE(None, gl.U, gl.U)
    pointer_type = c.CFUNCTYPE(None, gl.U, gl.I, gl.U, c.c_ubyte, gl.I, gl.P)
    calls = []
    @bind_type
    def bind(target, name):
        calls.append(('bind', name))
        gl.fn('glBindBuffer', None, [gl.U,gl.U])(target,name)
    @pointer_type
    def pointer(attr, count, kind, normalized, stride, address):
        calls.append(('pointer', attr, address))
        gl.fn('glVertexAttribPointer', None,
              [gl.U,gl.I,gl.U,c.c_ubyte,gl.I,gl.P])(attr,count,kind,normalized,stride,address)
    bridge.init.argtypes = [bind_type,pointer_type]
    bridge.init(bind,pointer)
    bridge.apply.argtypes = [gl.U,gl.U,gl.U,gl.U,c.c_ubyte,gl.U,c.c_ssize_t,gl.U]
    bridge.offset.argtypes = [gl.I,gl.U,gl.I,gl.U]
    bridge.offset.restype = c.c_ssize_t
    assert bridge.offset(4,20,-1,8)==16

    vertex = gl.shader(0x8B31, '#version 300 es\nlayout(location=0)in vec2 p;'
        'void main(){gl_Position=vec4(p,0.,1.);}')
    fragment = gl.shader(0x8B30, '#version 300 es\nprecision highp float;'
        'out vec4 o;void main(){o=vec4(.2,.6,.8,1.);}')
    program = gl.fn('glCreateProgram', gl.U, [])()
    attach = gl.fn('glAttachShader', None, [gl.U,gl.U])
    attach(program,vertex);attach(program,fragment)
    gl.fn('glLinkProgram',None,[gl.U])(program)
    gl.fn('glUseProgram',None,[gl.U])(program)
    buffers=(gl.U*2)()
    gl.fn('glGenBuffers',None,[gl.I,c.POINTER(gl.U)])(2,buffers)
    # Each vertex has padding: wrong base/stride/stream offset reads degenerates.
    vertices=(c.c_float*24)(0,0,0,0, 0,0,0,0,
                            -1,-1,0,0, 3,-1,0,0, -1,3,0,0, 0,0,0,0)
    for name in buffers:
        gl.fn('glBindBuffer',None,[gl.U,gl.U])(0x8892,name)
        gl.fn('glBufferData',None,[gl.U,c.c_ssize_t,gl.P,gl.U])(
            0x8892,c.sizeof(vertices),vertices,0x88E4)
    indices=(c.c_ushort*3)(0,1,2)
    ib=gl.U()
    gl.fn('glGenBuffers',None,[gl.I,c.POINTER(gl.U)])(1,c.byref(ib))
    gl.fn('glBindBuffer',None,[gl.U,gl.U])(0x8893,ib)
    gl.fn('glBufferData',None,[gl.U,c.c_ssize_t,gl.P,gl.U])(0x8893,6,indices,0x88E4)
    gl.fn('glEnableVertexAttribArray',None,[gl.U])(0)
    for name,decl,stream,base,stride,revision in [
        (buffers[0],0,0,2,16,1),
        (buffers[0],0,16,1,16,1), # same effective address
        (buffers[1],0,0,2,16,1), # same revision/offset, different VBO
        (buffers[1],0,0,2,16,2), # changed backing/revision
        (buffers[0],0,0,4,8,1),  # stride transition, restored below
        (buffers[0],0,0,2,16,1),
    ]:
        offset=bridge.offset(decl,stream,base,stride)
        bridge.apply(0,name,stride,0x1406,0,2,offset,revision)
        before=len(calls)
        bridge.apply(0,name,stride,0x1406,0,2,offset,revision)
        assert len(calls)==before, 'Repeated identical attribute was resubmitted'
        if stride==8: continue
        gl.fn('glDrawElements',None,[gl.U,gl.I,gl.U,gl.P])(4,3,0x1403,None)
        pixels=(c.c_ubyte*8)()
        gl.fn('glReadPixels',None,[gl.I,gl.I,gl.I,gl.I,gl.U,gl.U,gl.P])(
            0,0,2,1,0x1908,0x1401,pixels)
        assert all(abs(pixels[i]-[51,153,204,255][i%4])<=1 for i in range(8)),list(pixels)
        assert gl.geterror()==0
    # Array-buffer changes for another attribute do not invalidate attr 0's captured VBO.
    bridge.apply(1,buffers[1],16,0x1406,0,2,0,1)
    before=len(calls)
    bridge.apply(0,buffers[0],16,0x1406,0,2,32,1)
    assert len(calls)==before
    print('GLES production vertex cache: signed/nonzero bases, stream offsets, strides, VBO and revision transitions passed')
