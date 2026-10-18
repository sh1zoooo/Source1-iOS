"""Compile the production ASTC uploader; check fallback pixels and native dispatch."""
import ctypes as c
from pathlib import Path
import math
import subprocess
import sys
import tempfile
import gles_texture_channels as gl

root = Path(sys.argv[1])
s = (root/'togles/linuxwin/cglmtex.cpp').read_text()
helper = s[s.index('static bool MobileASTCNativeFormat('):s.index('\nvoid CompressedTexImage2D(')]
source = r'''
#include "MobileASTC.hpp"
#include <vector>
#include <cstring>
#include <cstdarg>
using GLenum=unsigned;using GLint=int;using GLsizei=int;using GLvoid=void;using uint8=unsigned char;
using SDL_GLContext=void*;SDL_GLContext currentContext=(void*)1;
SDL_GLContext SDL_GL_GetCurrentContext(){return currentContext;}
template<class T>struct CUtlVector{std::vector<T> v;void SetCount(int n){v.resize(n);}int Count(){return int(v.size());}T*Base(){return v.data();}T&operator[](int n){return v[n];}};
template<class T>struct CUtlMemory{std::vector<T> v;void EnsureCapacity(int n){v.resize(n);}T*Base(){return v.data();}T&operator[](size_t n){return v[n];}};
void Warning(const char*,...){}
constexpr unsigned GL_PIXEL_UNPACK_BUFFER_BINDING=0x88EF,GL_PIXEL_UNPACK_BUFFER=0x88EC;
constexpr unsigned GL_UNPACK_ROW_LENGTH=0xCF2,GL_UNPACK_SKIP_PIXELS=0xCF4,GL_UNPACK_SKIP_ROWS=0xCF3,GL_UNPACK_ALIGNMENT=0xCF5;
constexpr unsigned GL_RGBA=0x1908,GL_RGBA8=0x8058,GL_SRGB8_ALPHA8=0x8C43,GL_UNSIGNED_BYTE=0x1401;
struct API {
 void(*glGetIntegerv)(unsigned,int*);
 void(*glCompressedTexImage2D)(unsigned,int,unsigned,int,int,int,int,const void*);
 void(*glBindBuffer)(unsigned,unsigned);
 void(*glPixelStorei)(unsigned,int);
 void(*glTexImage2D)(unsigned,int,int,int,int,int,unsigned,unsigned,const void*);
}api,*gGL=&api;
HELPER
extern "C" void init(void**fns){
 api.glGetIntegerv=(decltype(api.glGetIntegerv))fns[0];
 api.glCompressedTexImage2D=(decltype(api.glCompressedTexImage2D))fns[1];
 api.glBindBuffer=(decltype(api.glBindBuffer))fns[2];api.glPixelStorei=(decltype(api.glPixelStorei))fns[3];
 api.glTexImage2D=(decltype(api.glTexImage2D))fns[4];}
extern "C" bool upload(unsigned target,int mip,unsigned format,int w,int h,int size,const void*data){
 return MobileASTCUpload(target,mip,format,w,h,0,size,data);}
extern "C" void context(unsigned long long value){currentContext=(void*)value;}
'''.replace('HELPER',helper)
with tempfile.TemporaryDirectory() as folder:
    p=Path(folder);(p/'bridge.cpp').write_text(source)
    subprocess.run(['c++','-std=c++17','-O2','-fPIC','-shared','-Isrc/source',
        '-Ithird_party/astc_decoder',str(p/'bridge.cpp'),'src/source/MobileASTC.cpp',
        'third_party/astc_decoder/astc.cpp','-o',str(p/'bridge.so')],check=True)
    bridge=c.CDLL(str(p/'bridge.so'))
    get=gl.fn('glGetIntegerv',None,[gl.U,c.POINTER(gl.I)])
    forced=False;calls=[];queries=[]
    @c.CFUNCTYPE(None,gl.U,c.POINTER(gl.I))
    def query(name,values):
        if name in (0x86A2,0x86A3):
            queries.append(name)
            formats=[0x93B0,0x93D0,0x93B4,0x93D4] if forced else []
            if name==0x86A2:values[0]=len(formats)
            else:
                for i,f in enumerate(formats):values[i]=f
        else:get(name,values)
    @c.CFUNCTYPE(None,gl.U,gl.I,gl.U,gl.I,gl.I,gl.I,gl.I,gl.P)
    def compressed(target,mip,fmt,w,h,border,size,data):
        bound=gl.I();get(0x88EF,c.byref(bound));assert bound.value==0
        calls.append((target,mip,fmt,w,h,border,c.string_at(data,size) if data else None))
    bindbuf=gl.fn('glBindBuffer',None,[gl.U,gl.U])
    store=gl.fn('glPixelStorei',None,[gl.U,gl.I])
    image=gl.fn('glTexImage2D',None,[gl.U,gl.I,gl.I,gl.I,gl.I,gl.I,gl.U,gl.U,gl.P])
    funcs=(gl.P*5)(*[c.cast(f,gl.P).value for f in [query,compressed,bindbuf,store,image]])
    bridge.init.argtypes=[c.POINTER(gl.P)];bridge.init(funcs)
    bridge.upload.argtypes=[gl.U,gl.I,gl.U,gl.I,gl.I,gl.I,gl.P];bridge.upload.restype=c.c_bool
    bridge.context.argtypes=[c.c_ulonglong]
    gen=gl.fn('glGenTextures',None,[gl.I,c.POINTER(gl.U)])
    bind=gl.fn('glBindTexture',None,[gl.U,gl.U]);param=gl.fn('glTexParameteri',None,[gl.U,gl.U,gl.I])
    fbo=gl.U();gl.fn('glGenFramebuffers',None,[gl.I,c.POINTER(gl.U)])(1,c.byref(fbo))
    gl.fn('glBindFramebuffer',None,[gl.U,gl.U])(0x8D40,fbo)
    target=gl.U();gen(1,c.byref(target));bind(0xDE1,target)
    image(0xDE1,0,0x8058,2,1,0,0x1908,0x1401,None)
    gl.fn('glFramebufferTexture2D',None,[gl.U,gl.U,gl.U,gl.U,gl.I])(0x8D40,0x8CE0,0xDE1,target,0)
    block=bytes([0xfc,0xfd,255,255,255,255,255,255,17,17,93,93,201,201,127,127])
    payload=(c.c_ubyte*16).from_buffer_copy(block)
    src=gl.U();gen(1,c.byref(src));bind(0xDE1,src)
    param(0xDE1,0x2801,0x2600);param(0xDE1,0x2800,0x2600)
    gl.fn('glViewport',None,[gl.I]*4)(0,0,2,1)
    gl.fn('glUseProgram',None,[gl.U])(gl.prog)
    pbo=gl.U();gl.fn('glGenBuffers',None,[gl.I,c.POINTER(gl.U)])(1,c.byref(pbo))
    bindbuf(0x88EC,pbo)
    gl.fn('glBufferData',None,[gl.U,c.c_ssize_t,gl.P,gl.U])(0x88EC,1024,None,0x88E8)
    # Non-default unpack state and an unrelated bound PBO must be restored.
    for name,value in [(0xCF2,9),(0xCF4,3),(0xCF3,2),(0xCF5,8)]:store(name,value)
    def state():
        result=[]
        for name in [0x88EF,0xCF2,0xCF4,0xCF3,0xCF5]:
            v=gl.I();get(name,c.byref(v));result.append(v.value)
        return result
    before=state()
    for fmt in [0x93B0,0x93B4,0x93D0,0x93D4]:
        assert bridge.upload(0xDE1,0,fmt,2,1,16,payload)
        assert state()==before
        assert not calls and gl.geterror()==0
        gl.fn('glDrawArrays',None,[gl.U,gl.I,gl.I])(4,0,3)
        result=(c.c_ubyte*8)();gl.fn('glReadPixels',None,[gl.I]*4+[gl.U,gl.U,gl.P])(0,0,2,1,0x1908,0x1401,result)
        def linear(v):return round(((v/255)/12.92 if v/255<=0.04045 else ((v/255+0.055)/1.055)**2.4)*255)
        expected=[linear(v) for v in [17,93,201]]+[127] if fmt in [0x93D0,0x93D4] else [17,93,201,127]
        assert all(abs(a-b)<=1 for a,b in zip(result,expected*2)),(hex(fmt),list(result),expected)
        assert gl.geterror()==0
    assert queries==[0x86A2],queries
    # Invalid byte count is rejected without issuing a texture upload.
    assert bridge.upload(0xDE1,0,0x93B0,2,1,15,payload) and not calls
    # On a context that advertises ASTC, the exact payload goes straight to GL.
    forced=True;bridge.context(2)
    for fmt in [0x93B0,0x93B4,0x93D0,0x93D4]:
        assert bridge.upload(0xDE1,0,fmt,2,1,16,payload)
        assert calls[-1]==(0xDE1,0,fmt,2,1,0,block)
    assert state()==before
    assert queries==[0x86A2,0x86A2,0x86A3],queries
    # Return to an unsupported context: cached formats must not leak across it.
    forced=False;bridge.context(3)
    assert bridge.upload(0xDE1,1,0x93B4,1,1,16,payload)
    assert len(calls)==4 and state()==before and gl.geterror()==0
    bindbuf(0x88EC,0)
print('ASTC production uploader: native dispatch, GLES fallback RGBA/alpha/sRGB, mip upload, context changes and unpack/PBO restoration passed')
