"""Compile production GLState and verify suppression, raw-state exclusions and pixels.

Pass the generated clientmod-port directory after geometry_state_write_patch.py.
"""
import ctypes as c
from pathlib import Path
import subprocess
import sys
import tempfile
import re
import gles_texture_channels as gl

root=Path(sys.argv[1])
header=(root/'graphics-compat/togles/linuxwin/glmgr.h').read_text()
# Keep the policy's coherence assumption reviewable: no generated source may
# mutate an eligible state directly behind its GLState value.
raw_setters = re.compile(r'\bgGL->(?:glFrontFace|glPolygonOffset|glDepthRangef|glBlendFunc|'
                         r'glBlendEquation|glBlendColor|glDepthFunc|glStencilFuncSeparate|'
                         r'glClearDepthf|glClearStencil)\s*\(')
for directory in ('togles', 'appframework', 'materialsystem'):
    for path in (root/directory).rglob('*.cpp'):
        assert not raw_setters.search(path.read_text()), f'Untracked eligible state mutation: {path}'
def block(marker):
    start=header.index(marker);begin=header.index('{',start);end=begin+1;depth=1
    while depth:
        depth+=(header[end]=='{')-(header[end]=='}');end+=1
    return header[start:end]
types=header[header.index('#define\tEQ('):header.index('#undef EQ')+len('#undef EQ')]
policy=header[header.index('template<class T> struct IOSCoalesceGLState'):header.index('#endif',header.index('template<class T> struct IOSCoalesceGLState'))]
state=block('template<typename T> class GLState')+';'
assert 'IOSCoalesceGLState<GLDepthMask_t>' not in policy
assert 'IOSCoalesceGLState<GLScissorBox_t>' not in policy
assert 'IOSCoalesceGLState<GLColorMaskSingle_t>' not in policy
assert 'IOSCoalesceGLState<GLViewportBox_t>' not in policy
assert 'IOSCoalesceGLState<GLClearColor_t>' not in policy
selected=['GLDepthFunc_t','GLBlendFactor_t','GLBlendEquation_t','GLViewportDepthRange_t','GLColorMaskSingle_t']
setters='\n'.join(block('FORCEINLINE void GLContextSet( '+t+' *src )')+'\n'+
                  block('FORCEINLINE void GLContextGetDefault( '+t+' *dst )') for t in selected)
source='''#include <cstring>
#define SOURCE_IOS 1
#define FORCEINLINE inline
using uint=unsigned;using GLenum=unsigned;using GLint=int;using GLsizei=int;
using GLfloat=float;using GLclampf=float;using GLuint=unsigned;
enum{GL_GEQUAL=0x206,GL_ONE=1,GL_ZERO=0,GL_FUNC_ADD=0x8006};
using Callback=void(*)(int,float,float,float,float);
Callback call;
struct GL {
 void glDepthFunc(unsigned v){call(0,v,0,0,0);}
 void glBlendFunc(unsigned a,unsigned b){call(1,a,b,0,0);}
 void glBlendEquation(unsigned v){call(2,v,0,0,0);}
 void glDepthRangef(float a,float b){call(3,a,b,0,0);}
 void glColorMask(unsigned char r,unsigned char g,unsigned char b,unsigned char a){call(4,r,g,b,a);}
} api;GL *gGL=&api;
struct Stats{unsigned long long submitted=0,skipped=0;} stats;
Stats &IOSStateWrites(){return stats;}
TYPES
template<class T> void GLContextGet(T*);
SETTERS
POLICY
STATE
GLState<GLDepthFunc_t> *depth;
GLState<GLBlendFactor_t> *blend;
GLState<GLBlendEquation_t> *equation;
GLState<GLViewportDepthRange_t> *range;
GLState<GLColorMaskSingle_t> *mask;
extern "C" {
void init(Callback fn){call=fn;depth=new GLState<GLDepthFunc_t>;
 blend=new GLState<GLBlendFactor_t>;equation=new GLState<GLBlendEquation_t>;
 range=new GLState<GLViewportDepthRange_t>;mask=new GLState<GLColorMaskSingle_t>;}
void apply(unsigned func,unsigned src,unsigned dst,unsigned eq,float nearValue,float farValue){
 GLDepthFunc_t a{func};depth->Write(&a);GLBlendFactor_t b{src,dst};blend->Write(&b);
 GLBlendEquation_t e{eq};equation->Write(&e);GLViewportDepthRange_t r{nearValue,farValue};range->Write(&r);
 GLColorMaskSingle_t m{1,1,1,1};mask->Write(&m);}
void force(){depth->Flush();blend->Flush();equation->Flush();range->Flush();mask->Flush();}
unsigned long long submitted(){return stats.submitted;}
unsigned long long skipped(){return stats.skipped;}
}
'''.replace('TYPES',types).replace('SETTERS',setters).replace('POLICY',policy).replace('STATE',state)

with tempfile.TemporaryDirectory() as folder:
    path=Path(folder);(path/'bridge.cpp').write_text(source)
    subprocess.run(['c++','-std=c++17','-shared','-fPIC','-O2',str(path/'bridge.cpp'),'-o',str(path/'bridge.so')],check=True)
    bridge=c.CDLL(str(path/'bridge.so'))
    callback=c.CFUNCTYPE(None,gl.I,*([c.c_float]*4));calls=[]
    @callback
    def submit(kind,a,b,d,e):
        calls.append(kind)
        if kind==0: gl.fn('glDepthFunc',None,[gl.U])(int(a))
        elif kind==1: gl.fn('glBlendFunc',None,[gl.U,gl.U])(int(a),int(b))
        elif kind==2: gl.fn('glBlendEquation',None,[gl.U])(int(a))
        elif kind==3: gl.fn('glDepthRangef',None,[c.c_float,c.c_float])(a,b)
        else: gl.fn('glColorMask',None,[c.c_ubyte]*4)(int(a),int(b),int(d),int(e))
    bridge.init.argtypes=[callback];bridge.init(submit)
    bridge.apply.argtypes=[gl.U,gl.U,gl.U,gl.U,c.c_float,c.c_float]
    bridge.submitted.restype=c.c_ulonglong;bridge.skipped.restype=c.c_ulonglong
    args=(0x206,1,0,0x8006,0.,1.)
    # Equal constructor defaults must still submit on the first Write.
    before=len(calls);bridge.apply(*args);assert len(calls)-before==5
    for _ in range(1000):bridge.apply(*args)
    assert bridge.skipped()==4000
    assert calls.count(0)==2 and calls.count(1)==2 and calls.count(2)==2 and calls.count(3)==2
    # Raw color-mask mutation is intentionally outside the eligibility policy.
    gl.fn('glColorMask',None,[c.c_ubyte]*4)(0,0,0,0)
    bridge.apply(*args)
    actual=(c.c_ubyte*4)();gl.fn('glGetBooleanv',None,[gl.U,gl.P])(0x0C23,actual)
    assert list(actual)==[1,1,1,1]
    # Explicit Flush remains a forced restoration even with unchanged data.
    gl.fn('glDepthFunc',None,[gl.U])(0x200)
    before=len(calls);bridge.force();assert len(calls)-before==5
    depth=gl.I();gl.fn('glGetIntegerv',None,[gl.U,gl.P])(0x0B74,c.byref(depth));assert depth.value==0x206

    vertex=gl.shader(0x8B31,'#version 300 es\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.,1.);}')
    fragment=gl.shader(0x8B30,'#version 300 es\nprecision highp float;out vec4 o;void main(){o=vec4(.8,.4,.2,.5);}')
    program=gl.fn('glCreateProgram',gl.U,[])();attach=gl.fn('glAttachShader',None,[gl.U,gl.U])
    attach(program,vertex);attach(program,fragment);gl.fn('glLinkProgram',None,[gl.U])(program);gl.fn('glUseProgram',None,[gl.U])(program)
    gl.fn('glEnable',None,[gl.U])(0x0BE2)
    gl.fn('glDisable',None,[gl.U])(0x0B71)
    for factors,eq in [((0x302,0x303),0x8006),((1,0),0x8006),((1,1),0x800A)]:
        def frame(cached):
            gl.fn('glClearColor',None,[c.c_float]*4)(.1,.2,.3,.4)
            gl.fn('glClear',None,[gl.U])(0x4000)
            if cached:bridge.apply(0x201,*factors,eq,.1,.9)
            else:
                gl.fn('glDepthFunc',None,[gl.U])(0x201)
                gl.fn('glBlendFunc',None,[gl.U,gl.U])(*factors)
                gl.fn('glBlendEquation',None,[gl.U])(eq)
                gl.fn('glDepthRangef',None,[c.c_float,c.c_float])(.1,.9)
            gl.fn('glDrawArrays',None,[gl.U,gl.I,gl.I])(4,0,3)
            pixels=(c.c_ubyte*8)();gl.fn('glReadPixels',None,[gl.I,gl.I,gl.I,gl.I,gl.U,gl.U,gl.P])(0,0,2,1,0x1908,0x1401,pixels)
            assert gl.geterror()==0
            return list(pixels)
        reference=frame(False)
        assert frame(True)==reference
        assert frame(True)==reference
    assert bridge.submitted()>0 and bridge.skipped()>4000
    print('Production GLState: 4000 redundant safe writes skipped; first writes/forced restores/raw-mask exclusion and blend pixels passed')
