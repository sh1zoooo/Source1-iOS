"""Render-equivalence test of the actual generated GLES indexed fallback.

Pass a generated clientmod-port patched by prepare_ios_indexed_draw.py.
"""
import ctypes as c
from pathlib import Path
import subprocess
import sys
import tempfile
import gles_texture_channels as gl

root = Path(sys.argv[1])
text = (root / 'togles/linuxwin/glmgr.cpp').read_text()
start = text.index('void GLMContext::DrawRangeElements(GLenum')
begin = text.index('{',start)
depth,end = 1,begin+1
while depth:
    depth += (text[end]=='{')-(text[end]=='}')
    end += 1
method = text[start:end]
assert ('gGL->glDrawElements(mode, count, type, indicesActual);' in method or
        'IOSDrawIndexed(mode, count, type, indicesActual, pIndexBuf);' in method)
source = '''#include <cstdint>
#define SOURCE_IOS 1
#define GLMDEBUG 0
#define GLM_FUNC
#define IOS_RENDER_TIMING(x)
using GLenum=unsigned;using GLuint=unsigned;using GLsizei=int;using GLvoid=void;
using uintptr_t=std::uintptr_t;
struct Work {void draw(unsigned,unsigned,unsigned,unsigned,bool){}};
Work work;Work& IOSRenderWork(){return work;}
struct CGLMBuffer {bool m_bPseudo=false,m_bUsingPersistentBuffer=false,m_bDynamic=false;
 char *m_pPseudoBuf=nullptr;unsigned m_nPersistentBufferStartOffset=0;};
struct GL {void(*glDrawElements)(GLenum,GLsizei,GLenum,const GLvoid*);
 void(*glDrawRangeElements)(GLenum,GLuint,GLuint,GLsizei,GLenum,const GLvoid*);} api;
GL *gGL=&api;
struct GLMContext {unsigned m_nBatchCounter=0;void *m_pBoundPair=(void*)1;
 void SetIndexBuffer(CGLMBuffer*){}
 // Native VAO correctness is covered separately by gles_native_vao.py.
 // This fixture isolates production index/topology/offset forwarding.
 void IOSDrawIndexed(GLenum mode,GLsizei count,GLenum type,const void *indices,CGLMBuffer*){
  gGL->glDrawElements(mode,count,type,indices);}
 void DrawRangeElements(GLenum,GLuint,GLuint,GLsizei,GLenum,const GLvoid*,CGLMBuffer*);
} context;
METHOD
extern "C" {
void init(decltype(api.glDrawElements) f){api.glDrawElements=f;}
void draw(unsigned mode,unsigned first,unsigned last,int count,unsigned type,
 uintptr_t offset,unsigned persistentOffset){CGLMBuffer buffer;
 buffer.m_bUsingPersistentBuffer=persistentOffset!=0;
 buffer.m_nPersistentBufferStartOffset=persistentOffset;
 context.DrawRangeElements(mode,first,last,count,type,(void*)offset,&buffer);}
}
'''.replace('METHOD',method)

with tempfile.TemporaryDirectory() as folder:
    path=Path(folder)
    (path/'bridge.cpp').write_text(source)
    subprocess.run(['c++','-std=c++17','-shared','-fPIC','-O2',str(path/'bridge.cpp'),
                    '-o',str(path/'bridge.so')],check=True)
    bridge=c.CDLL(str(path/'bridge.so'))
    callback=c.CFUNCTYPE(None,gl.U,gl.I,gl.U,gl.P)
    draws=[]
    @callback
    def draw(mode,count,kind,offset):
        draws.append((mode,count,kind,offset))
        gl.fn('glDrawElements',None,[gl.U,gl.I,gl.U,gl.P])(mode,count,kind,offset)
    bridge.init.argtypes=[callback];bridge.init(draw)
    bridge.draw.argtypes=[gl.U,gl.U,gl.U,gl.I,gl.U,c.c_size_t,gl.U]

    vertex=gl.shader(0x8B31,'#version 300 es\nlayout(location=0)in vec2 p;'
                     'void main(){gl_Position=vec4(p,0.,1.);}')
    fragment=gl.shader(0x8B30,'#version 300 es\nprecision highp float;'
                       'out vec4 o;void main(){o=vec4(.8,.2,.6,1.);}')
    program=gl.fn('glCreateProgram',gl.U,[])()
    attach=gl.fn('glAttachShader',None,[gl.U,gl.U])
    attach(program,vertex);attach(program,fragment)
    gl.fn('glLinkProgram',None,[gl.U])(program)
    gl.fn('glUseProgram',None,[gl.U])(program)
    buffers=(gl.U*2)()
    gl.fn('glGenBuffers',None,[gl.I,c.POINTER(gl.U)])(2,buffers)
    # Four real vertices surrounded by padding let us exercise base rebasing.
    vertices=(c.c_float*24)(0,0,0,0, -1,-1, 1,-1, -1,1, 1,1,
                            0,0,0,0,0,0,0,0,0,0,0,0)
    gl.fn('glBindBuffer',None,[gl.U,gl.U])(0x8892,buffers[0])
    gl.fn('glBufferData',None,[gl.U,c.c_ssize_t,gl.P,gl.U])(
        0x8892,c.sizeof(vertices),vertices,0x88E4)
    # Prefix and suffix indices ensure only the requested subrange is consumed.
    indices=(c.c_ushort*18)(0,0,0, 0,1,2,2,1,3, 0,1,2,3, 0,1,2,3,0)
    gl.fn('glBindBuffer',None,[gl.U,gl.U])(0x8893,buffers[1])
    gl.fn('glBufferData',None,[gl.U,c.c_ssize_t,gl.P,gl.U])(
        0x8893,c.sizeof(indices),indices,0x88E4)
    gl.fn('glEnableVertexAttribArray',None,[gl.U])(0)
    read=gl.fn('glReadPixels',None,[gl.I,gl.I,gl.I,gl.I,gl.U,gl.U,gl.P])
    clear=gl.fn('glClear',None,[gl.U])
    gl.fn('glClearColor',None,[c.c_float]*4)(0,0,0,0)
    tested=0
    for mode,count,offset in [(4,6,6),(5,4,18),(1,4,26),(4,3,6),(4,0,6)]:
        for stream,base in [(0,2),(24,-1),(8,1)]:
            effective=stream+base*8
            assert effective==16
            gl.fn('glVertexAttribPointer',None,
                [gl.U,gl.I,gl.U,c.c_ubyte,gl.I,gl.P])(0,2,0x1406,0,8,effective)
            clear(0x4000)
            gl.fn('glDrawRangeElements',None,[gl.U,gl.U,gl.U,gl.I,gl.U,gl.P])(
                mode,0,4,count,0x1403,offset)
            reference=(c.c_ubyte*8)();read(0,0,2,1,0x1908,0x1401,reference)
            clear(0x4000)
            # Persistent backing adjustment and ordinary index offsets agree.
            bridge.draw(mode,0,4,count,0x1403,offset-2,2)
            result=(c.c_ubyte*8)();read(0,0,2,1,0x1908,0x1401,result)
            assert list(result)==list(reference),(mode,count,list(result),list(reference))
            assert draws[-1]==(mode,count,0x1403,offset)
            assert gl.geterror()==0
            tested+=1
    print(f'{tested} production GLES indexed draws match range draws: offsets, signed rebasing, triangles, strips, lines, partial and empty draws')
