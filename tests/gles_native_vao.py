"""Pixel/state tests of the generated production IOSDrawIndexed and real VAOs."""
import ctypes as c
from pathlib import Path
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).parent.parent/'tests'))
import gles_texture_channels as gl

root=Path(sys.argv[1])
text=(root/'togles/linuxwin/glmgr.cpp').read_text()
start=text.index('void GLMContext::IOSDrawIndexed(')
begin=text.index('{',start); depth=1; end=begin+1
while depth:
    depth+=(text[end]=='{')-(text[end]=='}'); end+=1
method=text[start:end]
source=r'''
#include "IOSNativeVAOCache.hpp"
#include <cstdint>
#define SOURCE_IOS 1
using GLenum=unsigned;using GLuint=unsigned;using GLsizei=int;
using GLboolean=unsigned char;using uintptr_t=std::uintptr_t;
enum {kGLMVertexBuffer=0,kGLMIndexBuffer=1};
double Plat_FloatTime(){return 0;}
template<class...T> void Warning(T...){}
struct GL {
void(*glGenVertexArrays)(GLsizei,GLuint*);
void(*glDeleteVertexArrays)(GLsizei,const GLuint*);
void(*glBindVertexArray)(GLuint);
void(*glBindBuffer)(GLenum,GLuint);
void(*glVertexAttribPointer)(GLuint,int,GLenum,GLboolean,GLsizei,const void*);
void(*glEnableVertexAttribArray)(GLuint);
void(*glDrawElements)(GLenum,GLsizei,GLenum,const void*);
bool m_bHave_GL_EXT_buffer_storage=false;
} api;
GL *gGL=&api;
struct CGLMBuffer {bool m_bPseudo=false,m_bUsingPersistentBuffer=false;};
struct GLMContext {
struct VertexAttribs_t {
GLuint m_iosBuffer=0,m_stride=0,m_datatype=0,m_nCompCount=0;
const void *m_pPtr=nullptr;GLboolean m_normalized=0;
} m_boundVertexAttribs[16];
unsigned m_lastKnownVertexAttribMask=0,m_nBoundGLBuffer[2]={};
IOSNativeVAOCache<> m_iosVertexArrays;
double m_iosVertexArrayReportTime=0;
void IOSDrawIndexed(GLenum,GLsizei,GLenum,const void*,CGLMBuffer*);
} context;
@@METHOD@@
extern "C" {
void init(void **f) {
api.glGenVertexArrays=reinterpret_cast<decltype(api.glGenVertexArrays)>(f[0]);
api.glDeleteVertexArrays=reinterpret_cast<decltype(api.glDeleteVertexArrays)>(f[1]);
api.glBindVertexArray=reinterpret_cast<decltype(api.glBindVertexArray)>(f[2]);
api.glBindBuffer=reinterpret_cast<decltype(api.glBindBuffer)>(f[3]);
api.glVertexAttribPointer=reinterpret_cast<decltype(api.glVertexAttribPointer)>(f[4]);
api.glEnableVertexAttribArray=reinterpret_cast<decltype(api.glEnableVertexAttribArray)>(f[5]);
api.glDrawElements=reinterpret_cast<decltype(api.glDrawElements)>(f[6]);
}
void attr(unsigned i,unsigned b,unsigned stride,unsigned type,unsigned components,
          uintptr_t offset,unsigned normalized) {
auto &a=context.m_boundVertexAttribs[i];a.m_iosBuffer=b;a.m_stride=stride;
a.m_datatype=type;a.m_nCompCount=components;a.m_pPtr=(void*)offset;a.m_normalized=normalized;
}
void bindings(unsigned array,unsigned element,unsigned mask) {
context.m_nBoundGLBuffer[0]=array;context.m_nBoundGLBuffer[1]=element;
context.m_lastKnownVertexAttribMask=mask;
}
void draw(unsigned mode,int count,uintptr_t offset) {
CGLMBuffer ib;context.IOSDrawIndexed(mode,count,0x1403,(void*)offset,&ib);
}
void invalidate(unsigned name){context.m_iosVertexArrays.invalidateBuffer(api,name);}
void clear(){context.m_iosVertexArrays.clear(api);}
unsigned long long stat(unsigned i){const auto &s=context.m_iosVertexArrays.counters;
return i==0?s.hits:i==1?s.misses:i==2?s.evictions:s.invalidations;}
}
'''.replace('@@METHOD@@',method)

with tempfile.TemporaryDirectory(prefix='ios-native-vao-') as folder:
    p=Path(folder);(p/'bridge.cpp').write_text(source)
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-shared','-fPIC','-O2',
        '-I',str(Path(__file__).parent.parent/'src/source'),str(p/'bridge.cpp'),'-o',str(p/'bridge.so')],check=True)
    b=c.CDLL(str(p/'bridge.so'))
    names=['glGenVertexArrays','glDeleteVertexArrays','glBindVertexArray','glBindBuffer',
           'glVertexAttribPointer','glEnableVertexAttribArray','glDrawElements']
    funcs=(gl.P*len(names))(*(gl.E.eglGetProcAddress(n.encode()) for n in names))
    b.init.argtypes=[c.POINTER(gl.P)];b.init(funcs)
    b.attr.argtypes=[gl.U,gl.U,gl.U,gl.U,gl.U,c.c_size_t,gl.U]
    b.bindings.argtypes=[gl.U,gl.U,gl.U];b.draw.argtypes=[gl.U,gl.I,c.c_size_t]
    b.invalidate.argtypes=[gl.U];b.stat.argtypes=[gl.U];b.stat.restype=c.c_ulonglong
    vs=gl.shader(0x8B31,'#version 300 es\nlayout(location=0)in vec2 p;'
        'layout(location=1)in vec4 c;out vec4 v;void main(){v=c;gl_Position=vec4(p,0,1);}')
    fs=gl.shader(0x8B30,'#version 300 es\nprecision highp float;in vec4 v;out vec4 o;void main(){o=v;}')
    program=gl.fn('glCreateProgram',gl.U,[])()
    for s in (vs,fs):gl.fn('glAttachShader',None,[gl.U,gl.U])(program,s)
    gl.fn('glLinkProgram',None,[gl.U])(program);gl.fn('glUseProgram',None,[gl.U])(program)
    bind=gl.fn('glBindBuffer',None,[gl.U,gl.U]);data=gl.fn('glBufferData',None,[gl.U,c.c_ssize_t,gl.P,gl.U])
    vertexptr=gl.fn('glVertexAttribPointer',None,[gl.U,gl.I,gl.U,c.c_ubyte,gl.I,gl.P])
    enable=gl.fn('glEnableVertexAttribArray',None,[gl.U]);disable=gl.fn('glDisableVertexAttribArray',None,[gl.U])
    buffers=(gl.U*4)();gl.fn('glGenBuffers',None,[gl.I,c.POINTER(gl.U)])(4,buffers)
    positions=(c.c_float*(8*320))(*([-1,-1,1,-1,-1,1,1,1]*320))
    bind(0x8892,buffers[0]);data(0x8892,c.sizeof(positions),positions,0x88E4)
    colors=(c.c_ubyte*16)(*([204,51,153,255]*4))
    bind(0x8892,buffers[1]);data(0x8892,c.sizeof(colors),colors,0x88E4)
    indices=(c.c_ushort*9)(0,0,0,0,1,2,2,1,3)
    bind(0x8893,buffers[2]);data(0x8893,c.sizeof(indices),indices,0x88E4)
    # Keep an unrelated ARRAY_BUFFER bound to detect global-binding leakage.
    bind(0x8892,buffers[3]);data(0x8892,4,None,0x88E4)
    clear=gl.fn('glClear',None,[gl.U]);gl.fn('glClearColor',None,[c.c_float]*4)(0,0,0,0)
    draw=gl.fn('glDrawElements',None,[gl.U,gl.I,gl.U,gl.P])
    get=gl.fn('glGetIntegerv',None,[gl.U,c.POINTER(gl.I)])
    read=gl.fn('glReadPixels',None,[gl.I,gl.I,gl.I,gl.I,gl.U,gl.U,gl.P])
    def query(token):
        value=gl.I();get(token,c.byref(value));return value.value
    def setup(offset=0,mask=3):
        bind(0x8892,buffers[0]);vertexptr(0,2,0x1406,0,8,offset);enable(0)
        b.attr(0,buffers[0],8,0x1406,2,offset,0)
        if mask&2:
            bind(0x8892,buffers[1]);vertexptr(1,4,0x1401,1,4,None);enable(1)
            b.attr(1,buffers[1],4,0x1401,4,0,1)
        else:
            disable(1);gl.fn('glVertexAttrib4f',None,[gl.U]+[c.c_float]*4)(1,.25,.75,.5,1)
        bind(0x8893,buffers[2]);bind(0x8892,buffers[3])
        b.bindings(buffers[3],buffers[2],mask)
    def pixels():
        out=(c.c_ubyte*8)();read(0,0,2,1,0x1908,0x1401,out);return bytes(out)
    tested=0
    def compare(mode=4,count=6):
        global tested
        clear(0x4000);draw(mode,count,0x1403,6);reference=pixels()
        clear(0x4000);b.draw(mode,count,6);actual=pixels()
        assert actual==reference,(actual,reference)
        assert query(0x85B5)==0 # VAO binding.
        assert query(0x8894)==buffers[3] # ARRAY_BUFFER.
        assert query(0x8895)==buffers[2] # Canonical VAO's ELEMENT binding.
        assert gl.geterror()==0
        tested+=1
    setup();compare();compare();compare();assert b.stat(0)>=1
    # Simulate preload changing canonical attributes between regular batches.
    setup(32,1);compare();compare();setup(0,3);compare()
    # Bounded cache eviction: every distinct capture still preserves pixels.
    for i in range(300):setup(i*32);compare();compare()
    assert b.stat(2)>0
    before=b.stat(1);b.invalidate(buffers[0]);setup();compare();assert b.stat(1)>before
    assert b.stat(3)>0
    # Delete after invalidation, then recreate an object (possibly same GLuint).
    old=gl.U(buffers[0]);gl.fn('glDeleteBuffers',None,[gl.I,c.POINTER(gl.U)])(1,c.byref(old))
    fresh=gl.U();gl.fn('glGenBuffers',None,[gl.I,c.POINTER(gl.U)])(1,c.byref(fresh));buffers[0]=fresh.value
    bind(0x8892,buffers[0]);data(0x8892,c.sizeof(positions),positions,0x88E4)
    setup();compare();b.clear();compare();b.clear()
    assert query(0x85B5)==0 and gl.geterror()==0
    print(f'{tested} production native VAO pixel/state cases passed: hits, misses, formats, canonical preload changes, eviction, buffer deletion/recreation and teardown.')
