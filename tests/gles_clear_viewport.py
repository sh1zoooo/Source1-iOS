"""Compile unchanged GLM Clear plus Source's production clear dispatch.

The old full-target dispatch inherits a small GUI clip, while Source's existing
explicit-viewport branch does not. Exercise full/partial target selection and
40 temporal depth-only frames. Generic D3D Clear is deliberately unchanged.
"""
from pathlib import Path
import ctypes as c
import subprocess,tempfile,sys
import gles_texture_channels as gl
root=Path(sys.argv[1])
s=(root/'togles/linuxwin/glmgr.cpp').read_text()
start=s.index('void GLMContext::Clear( bool color,')
begin=s.index('{',start);depth=1;end=begin+1
while depth:
 depth+=(s[end]=='{')-(s[end]=='}');end+=1
method=s[start:end]
assert 'viewportClearBox' not in method, 'Generic D3D Clear must remain unchanged'
shader=(root/'materialsystem/shaderapidx9/shaderapidx8.cpp').read_text()
a=shader.index('void CShaderAPIDx8::ClearBuffers( bool bClearColor,')
a=shader.index('if ( mask != 0 )',a)
b=shader.index('{',a);depth=1;end=b+1
while depth:
 depth+=(shader[end]=='{')-(shader[end]=='}');end+=1
selector=shader[a:end]
assert 'iOS viewport clear: sample=' in selector
start=selector.index('#ifdef SOURCE_IOS');end=selector.index('#endif',start)+6
old=selector[:start]+selector[end:]
source=r'''
#include <dlfcn.h>
#include <cassert>
#include <cstddef>
#include <algorithm>
#define MAX std::max
#define MIN std::min
using GLint=int;using GLenum=unsigned;using GLfloat=float;using GLsizei=int;using GLboolean=unsigned char;using uint=unsigned;
constexpr unsigned GL_COLOR_BUFFER_BIT=0x4000,GL_DEPTH_BUFFER_BIT=0x100,GL_STENCIL_BUFFER_BIT=0x400;
#define GLM_FUNC
#define GLMDEBUG 0
#define SOURCE_IOS 1
struct GLScissorBox_t {int x,y,width,height;};
struct GLViewportBox_t {int x,y,width,height;unsigned widthheight;};
struct GLScissorEnable_t {bool enable;};
struct GLClearColor_t {float r,g,b,a;};
struct GLClearDepth_t {float d;};
struct GLClearStencil_t {int i;};
struct GLDepthMask_t {bool mask;};
struct GLStencilWriteMask_t {int mask;};
struct GLColorMaskSingle_t {int r,g,b,a;};
struct GL {void(*glClear)(unsigned);void(*glClearColor)(float,float,float,float);
 void(*glClearDepthf)(float);void(*glClearStencil)(int);void(*glDepthMask)(unsigned char);
 void(*glStencilMask)(unsigned);void(*glColorMask)(unsigned char,unsigned char,unsigned char,unsigned char);
 void(*glScissor)(int,int,int,int);void(*glViewport)(int,int,int,int);
 void(*glEnable)(unsigned);void(*glDisable)(unsigned);} api,*gGL=&api;
void set(const GLClearColor_t &v){api.glClearColor(v.r,v.g,v.b,v.a);}
void set(const GLClearDepth_t &v){api.glClearDepthf(v.d);}
void set(const GLClearStencil_t &v){api.glClearStencil(v.i);}
void set(const GLDepthMask_t &v){api.glDepthMask(v.mask);}
void set(const GLStencilWriteMask_t &v){api.glStencilMask(v.mask);}
void set(const GLColorMaskSingle_t &v){api.glColorMask(v.r!=0,v.g!=0,v.b!=0,v.a!=0);}
void set(const GLScissorEnable_t &v){(v.enable?api.glEnable:api.glDisable)(0x0C11);}
void set(const GLScissorBox_t &v){api.glScissor(v.x,v.y,v.width,v.height);}
void set(const GLViewportBox_t &v){api.glViewport(v.x,v.y,v.width,v.height);}
template<class T> struct State {T data;void Read(T *v,int){*v=data;}
 void Write(const T *v){data=*v;set(data);} const T& GetData(){return data;}};
struct GLMContext {unsigned m_nBatchCounter=0;
 State<GLClearColor_t> m_ClearColor;State<GLClearDepth_t> m_ClearDepth;State<GLClearStencil_t> m_ClearStencil;
 State<GLDepthMask_t> m_DepthMask;State<GLStencilWriteMask_t> m_StencilWriteMask;
 State<GLColorMaskSingle_t> m_ColorMaskSingle;State<GLScissorEnable_t> m_ScissorEnable;
 State<GLScissorBox_t> m_ScissorBox;State<GLViewportBox_t> m_ViewportBox;
 void Clear(bool,unsigned long,bool,float,bool,unsigned,GLScissorBox_t*);
} context;
METHOD
using DWORD=unsigned;
struct D3DRECT {int x1,y1,x2,y2;};
struct Desired {struct {unsigned X,Y,Width,Height;} m_Viewport;unsigned m_RenderState[1];} m_DesiredState;
constexpr unsigned D3DRS_SCISSORTESTENABLE=0;
#define RECORD_COMMAND(...)
#define RECORD_INT(...)
#define RECORD_STRUCT(...)
#define RECORD_FLOAT(...)
#define DX8_CLEAR 0
unsigned warningCalls=0;void Warning(const char*,...){++warningCalls;}
struct DeviceBridge {void Clear(unsigned count,const D3DRECT *rect,unsigned mask,unsigned color,float z,unsigned stencil){
 GLScissorBox_t box;
 if(count)box={rect->x1,rect->y1,rect->x2-rect->x1,rect->y2-rect->y1};
 context.Clear(mask&1,color,mask&2,z,mask&4,stencil,count?&box:nullptr);
}} device;
DeviceBridge* Dx9Device(){return &device;}
void dispatch(bool depthOnly,int renderTargetWidth,int renderTargetHeight){
 DWORD mask=depthOnly?2:7;unsigned clearColor=0xFF0000FF;float depth=1.f;
 D3DRECT clear={(int)m_DesiredState.m_Viewport.X,(int)m_DesiredState.m_Viewport.Y,
 (int)(m_DesiredState.m_Viewport.X+m_DesiredState.m_Viewport.Width),
 (int)(m_DesiredState.m_Viewport.Y+m_DesiredState.m_Viewport.Height)};
 DISPATCH
}
extern "C" {
void init(void *(*loader)(const char*)){
#define LOAD(fn) api.fn=(decltype(api.fn))loader(#fn);assert(api.fn);
LOAD(glClear) LOAD(glClearColor) LOAD(glClearDepthf) LOAD(glClearStencil)
LOAD(glDepthMask) LOAD(glStencilMask) LOAD(glColorMask) LOAD(glScissor)
LOAD(glViewport) LOAD(glEnable) LOAD(glDisable)
}
void configure(int x,int y,int w,int h,int sx,int sy,int sw,int sh,bool enabled){
 m_DesiredState.m_Viewport={(unsigned)x,(unsigned)y,(unsigned)w,(unsigned)h};
 m_DesiredState.m_RenderState[0]=enabled;
 GLViewportBox_t vp={x,y,w,h,0};context.m_ViewportBox.Write(&vp);
 GLScissorBox_t sc={sx,sy,sw,sh};context.m_ScissorBox.Write(&sc);
 GLScissorEnable_t en={enabled};context.m_ScissorEnable.Write(&en);
 GLDepthMask_t dm={false};context.m_DepthMask.Write(&dm);
 GLStencilWriteMask_t sm={0};context.m_StencilWriteMask.Write(&sm);
 GLColorMaskSingle_t cm={0,1,0,1};context.m_ColorMaskSingle.Write(&cm);
}
void clear(bool box){GLScissorBox_t rect={3,2,2,2};
 if(box)context.Clear(true,0xFF0000FF,true,1.f,true,0, &rect);
 else dispatch(false,m_DesiredState.m_Viewport.Width,m_DesiredState.m_Viewport.Height);}
void partialClear(){dispatch(false,8,8);}
void genericNullClear(){context.Clear(true,0xFF0000FF,true,1.f,true,0,nullptr);}
unsigned warningCount(){return warningCalls;}
void depthClear(){dispatch(true,m_DesiredState.m_Viewport.Width,m_DesiredState.m_Viewport.Height);}
}
'''
# Full-size RGBA + depth/stencil test FBO.
tex=gl.U();gl.gen(1,c.byref(tex));gl.bind(0x0DE1,tex)
gl.image(0x0DE1,0,0x8058,8,8,0,0x1908,0x1401,None)
fbo=gl.U();gl.fn('glGenFramebuffers',None,[gl.I,c.POINTER(gl.U)])(1,c.byref(fbo))
gl.fn('glBindFramebuffer',None,[gl.U,gl.U])(0x8D40,fbo)
gl.fn('glFramebufferTexture2D',None,[gl.U,gl.U,gl.U,gl.U,gl.I])(0x8D40,0x8CE0,0x0DE1,tex,0)
rbo=gl.U();gl.fn('glGenRenderbuffers',None,[gl.I,c.POINTER(gl.U)])(1,c.byref(rbo))
gl.fn('glBindRenderbuffer',None,[gl.U,gl.U])(0x8D41,rbo)
gl.fn('glRenderbufferStorage',None,[gl.U,gl.U,gl.I,gl.I])(0x8D41,0x88F0,8,8)
gl.fn('glFramebufferRenderbuffer',None,[gl.U,gl.U,gl.U,gl.U])(0x8D40,0x821A,0x8D41,rbo)
assert gl.fn('glCheckFramebufferStatus',gl.U,[gl.U])(0x8D40)==0x8CD5
clear=gl.fn('glClear',None,[gl.U]);clearcolor=gl.fn('glClearColor',None,[c.c_float]*4)
colorMask=gl.fn('glColorMask',None,[c.c_ubyte]*4)
depthMask=gl.fn('glDepthMask',None,[c.c_ubyte]);stencilMask=gl.fn('glStencilMask',None,[gl.U])
enable=gl.fn('glEnable',None,[gl.U]);disable=gl.fn('glDisable',None,[gl.U])
def initialize():
 disable(0x0C11);disable(0x0B71);disable(0x0B90);disable(0x0BE2)
 colorMask(1,1,1,1);depthMask(1);stencilMask(0xFF)
 clearcolor(1,0,0,1)
 gl.fn('glClearDepthf',None,[c.c_float])(.2)
 gl.fn('glClearStencil',None,[gl.I])(0x55)
 clear(0x4500)
def pixels():
 p=(c.c_ubyte*256)();gl.fn('glReadPixels',None,[gl.I,gl.I,gl.I,gl.I,gl.U,gl.U,gl.P])(0,0,8,8,0x1908,0x1401,p)
 assert gl.geterror()==0
 return [tuple(p[i:i+4]) for i in range(0,256,4)]
def region_matches(values,inside,outside,rect):
 x,y,w,h=rect
 return all(v==(inside if x<=i%8<x+w and y<=i//8<y+h else outside) for i,v in enumerate(values))
# Opaque fullscreen shader at depth .5 verifies depth clearing by drawing over
# old .2 depth only where it was reset to 1.0. Stencil tests separately verify
# replacement 0 versus old .55 without unsupported GLES depth readbacks.
v=gl.shader(0x8B31,'#version 300 es\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.,1.);}')
f=gl.shader(0x8B30,'#version 300 es\nprecision highp float;out vec4 o;void main(){o=vec4(0,1,0,1);}')
prog=gl.fn('glCreateProgram',gl.U,[])();attach=gl.fn('glAttachShader',None,[gl.U,gl.U]);attach(prog,v);attach(prog,f)
gl.fn('glLinkProgram',None,[gl.U])(prog);gl.fn('glUseProgram',None,[gl.U])(prog)
def probe(depth=False,stencil=False):
 disable(0x0C11);colorMask(1,1,1,1);depthMask(0);stencilMask(0)
 gl.fn('glViewport',None,[gl.I]*4)(0,0,8,8)
 (enable if depth else disable)(0x0B71)
 gl.fn('glDepthFunc',None,[gl.U])(0x0201) # LESS
 (enable if stencil else disable)(0x0B90)
 gl.fn('glStencilFunc',None,[gl.U,gl.I,gl.U])(0x0202,0,0xFF) # EQUAL
 gl.fn('glDrawArrays',None,[gl.U,gl.I,gl.I])(4,0,3)
 return pixels()
# A depth-only temporal clear is the normal world-render case. Draw a far
# opaque floor then a near moving weapon rectangle; stale depth rejects next
# frame's floor at all previous weapon positions and leaves a visible trail.
tv=gl.shader(0x8B31,'#version 300 es\nprecision highp float;uniform float z;'
 'void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,z,1.);}')
tf=gl.shader(0x8B30,'#version 300 es\nprecision highp float;uniform vec4 tint;'
 'out vec4 o;void main(){o=tint;}')
temporal=gl.fn('glCreateProgram',gl.U,[])();attach(temporal,tv);attach(temporal,tf)
gl.fn('glLinkProgram',None,[gl.U])(temporal)
loc=gl.fn('glGetUniformLocation',gl.I,[gl.U,c.c_char_p])
zloc=loc(temporal,b'z');tintloc=loc(temporal,b'tint')
uniformZ=gl.fn('glUniform1f',None,[gl.I,c.c_float])
uniformTint=gl.fn('glUniform4f',None,[gl.I,c.c_float,c.c_float,c.c_float,c.c_float])
viewport=gl.fn('glViewport',None,[gl.I]*4)
draw=gl.fn('glDrawArrays',None,[gl.U,gl.I,gl.I])

def temporal_depth_only(bridge,label):
 initialize()
 gl.fn('glClearDepthf',None,[c.c_float])(1.)
 clear(0x100)
 gl.fn('glUseProgram',None,[gl.U])(temporal)
 positions=[(1,1),(4,1),(4,4),(1,4),(3,3)] * 8
 ever=set()
 for frame,(x,y) in enumerate(positions):
  # Simulate the previous frame's small HUD clip and disabled write masks.
  bridge.configure(0,0,8,8,0,0,1,1,True)
  bridge.depthClear()
  # Production Clear must not change clipping state for subsequent HUD work.
  assert gl.fn('glIsEnabled',c.c_ubyte,[gl.U])(0x0C11)
  disable(0x0C11);disable(0x0B90);enable(0x0B71)
  gl.fn('glDepthFunc',None,[gl.U])(0x0201)
  depthMask(1);colorMask(1,1,1,1)
  viewport(0,0,8,8);uniformZ(zloc,.6);uniformTint(tintloc,1,0,0,1)
  draw(4,0,3) # floor: depth .8, opaque red
  viewport(x,y,2,2);uniformZ(zloc,-.6);uniformTint(tintloc,0,0,1,1)
  draw(4,0,3) # weapon: depth .2, opaque blue
  current={(px,py) for px in range(x,x+2) for py in range(y,y+2)}
  ever |= current
  visible=current if label=='fixed' else ever
  actual=pixels()
  expected=[(0,0,255,255) if (i%8,i//8) in visible else (255,0,0,255) for i in range(64)]
  assert actual==expected,(label,frame,'depth-only temporal ghosts')
  if frame>=1:
   blue=sum(pixel==(0,0,255,255) for pixel in actual)
   assert blue==4 if label=='fixed' else blue>4
 assert len(ever)>4

with tempfile.TemporaryDirectory() as tmp:
 for label,body in [('old',old),('fixed',selector)]:
  gl.fn('glUseProgram',None,[gl.U])(prog)
  path=Path(tmp);src=path/(label+'.cpp');lib=path/(label+'.so');src.write_text(source.replace('METHOD',method).replace('DISPATCH',body))
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-sign-compare','-shared','-fPIC',str(src),'-ldl','-o',str(lib)],check=True)
  bridge=c.CDLL(str(lib))
  loader_type=c.CFUNCTYPE(gl.P,c.c_char_p)
  loader=loader_type(lambda name:gl.E.eglGetProcAddress(name))
  bridge.init.argtypes=[loader_type];bridge.init(loader)
  bridge.configure.argtypes=[gl.I]*8+[c.c_bool];bridge.clear.argtypes=[c.c_bool]
  for scissor in [False,True]:
   initialize();bridge.configure(2,1,4,5,0,0,1,1,scissor);bridge.clear(False)
   expected=(2,1,4,5) if label=='fixed' else ((0,0,1,1) if scissor else (0,0,8,8))
   assert region_matches(pixels(),(0,0,255,255),(255,0,0,255),expected),(label,scissor,'color')
   # Clear restores all write masks and the previous scissor state/rectangle.
   get=gl.fn('glGetIntegerv',None,[gl.U,c.POINTER(gl.I)])
   clip=(gl.I*4)();get(0x0C10,clip);assert list(clip)==[0,0,1,1]
   assert bool(gl.fn('glIsEnabled',c.c_ubyte,[gl.U])(0x0C11))==scissor
   mask=(c.c_ubyte*4)();gl.fn('glGetBooleanv',None,[gl.U,gl.P])(0x0C23,mask);assert list(mask)==[0,1,0,1]
   dm=c.c_ubyte();gl.fn('glGetBooleanv',None,[gl.U,gl.P])(0x0B72,c.byref(dm));assert dm.value==0
   sm=gl.I();get(0x0B98,c.byref(sm));assert sm.value&0xFF==0
   # Geometry probes must match the same region as the color clear.
   assert region_matches(probe(depth=True),(0,255,0,255),(255,0,0,255),expected),(label,scissor,'depth')
   initialize();bridge.configure(2,1,4,5,0,0,1,1,scissor);bridge.clear(False)
   assert region_matches(probe(stencil=True),(0,255,0,255),(255,0,0,255),expected),(label,scissor,'stencil')
  # Source's existing partial-target dispatch remains explicit viewport clearing.
  initialize();bridge.configure(2,1,4,5,0,0,1,1,True);bridge.partialClear()
  assert region_matches(pixels(),(0,0,255,255),(255,0,0,255),(2,1,4,5))
  # Generic D3D Clear with NULL box still honors its enabled clip (unchanged).
  initialize();bridge.configure(2,1,4,5,0,0,1,1,True);bridge.genericNullClear()
  assert region_matches(pixels(),(0,0,255,255),(255,0,0,255),(0,0,1,1))
  # Existing explicit rectangle clear remains exactly 3,2,2,2.
  initialize();bridge.configure(2,1,4,5,0,0,1,1,True);bridge.clear(True)
  assert region_matches(pixels(),(0,0,255,255),(255,0,0,255),(3,2,2,2))
  temporal_depth_only(bridge,label)
  # Alternating targets/viewports cannot create unbounded diagnostic output.
  for i in range(48):
   bridge.configure(i%4,0,2,2,0,0,1,1,True);bridge.clear(False)
  assert bridge.warningCount()==(32 if label=='fixed' else 0)

print('Production Source clear dispatch: full/partial viewports, masks, color/depth/stencil, 40 temporal depth-only frames passed; generic D3D Clear unchanged')
