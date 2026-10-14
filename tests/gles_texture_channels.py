import ctypes as c
E=c.CDLL('libEGL.so.1')
E.eglGetProcAddress.argtypes=[c.c_char_p];E.eglGetProcAddress.restype=c.c_void_p
def fn(name,result,args):
    p=E.eglGetProcAddress(name.encode());assert p,name
    return c.CFUNCTYPE(result,*args)(p)
I=c.c_int;U=c.c_uint;P=c.c_void_p
getdisplay=fn('eglGetPlatformDisplayEXT',P,[U,P,c.POINTER(I)])
d=getdisplay(0x31DD,None,None);assert d
init=fn('eglInitialize',U,[P,c.POINTER(I),c.POINTER(I)])
a=I();b=I();assert init(d,c.byref(a),c.byref(b))
assert fn('eglBindAPI',U,[U])(0x30A0)
attrs=(I*15)(0x3033,1,0x3040,0x40,0x3024,8,0x3023,8,0x3022,8,0x3021,8,0x3038,0,0)
config=P();n=I();assert fn('eglChooseConfig',U,[P,c.POINTER(I),c.POINTER(P),I,c.POINTER(I)])(d,attrs,c.byref(config),1,c.byref(n)) and n.value
ctxattrs=(I*3)(0x3098,3,0x3038)
ctx=fn('eglCreateContext',P,[P,P,P,c.POINTER(I)])(d,config,None,ctxattrs);assert ctx
assert fn('eglMakeCurrent',U,[P,P,P,P])(d,None,None,ctx)
geterror=fn('glGetError',U,[])
gen=fn('glGenTextures',None,[I,c.POINTER(U)]);bind=fn('glBindTexture',None,[U,U])
param=fn('glTexParameteri',None,[U,U,I])
image=fn('glTexImage2D',None,[U,I,I,I,I,I,U,U,P])
src=U();gen(1,c.byref(src));bind(0x0DE1,src)
param(0x0DE1,0x2801,0x2600);param(0x0DE1,0x2800,0x2600)
pixels=(c.c_ubyte*8)(0,0,255,17,255,0,0,99)
image(0x0DE1,0,0x8058,2,1,0,0x1908,0x1401,pixels)
param(0x0DE1,0x8E42,0x1905);param(0x0DE1,0x8E44,0x1903)
out=U();gen(1,c.byref(out));bind(0x0DE1,out)
image(0x0DE1,0,0x8058,2,1,0,0x1908,0x1401,None)
fbo=U();fn('glGenFramebuffers',None,[I,c.POINTER(U)])(1,c.byref(fbo))
fn('glBindFramebuffer',None,[U,U])(0x8D40,fbo)
fn('glFramebufferTexture2D',None,[U,U,U,U,I])(0x8D40,0x8CE0,0x0DE1,out,0)
assert fn('glCheckFramebufferStatus',U,[U])(0x8D40)==0x8CD5
def shader(kind,code):
    s=fn('glCreateShader',U,[U])(kind);raw=c.c_char_p(code.encode())
    fn('glShaderSource',None,[U,I,c.POINTER(c.c_char_p),P])(s,1,c.byref(raw),None)
    fn('glCompileShader',None,[U])(s);ok=I()
    fn('glGetShaderiv',None,[U,U,c.POINTER(I)])(s,0x8B81,c.byref(ok));assert ok.value
    return s
vs=shader(0x8B31,'#version 300 es\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.,1.);}')
ps=shader(0x8B30,'#version 300 es\nprecision highp float;uniform sampler2D t;out vec4 o;void main(){o=texelFetch(t,ivec2(gl_FragCoord.xy),0);}')
prog=fn('glCreateProgram',U,[])()
attach=fn('glAttachShader',None,[U,U]);attach(prog,vs);attach(prog,ps)
fn('glLinkProgram',None,[U])(prog);fn('glUseProgram',None,[U])(prog)
fn('glViewport',None,[I,I,I,I])(0,0,2,1)
bind(0x0DE1,src)
def check(label):
    fn('glDrawArrays',None,[U,I,I])(4,0,3)
    result=(c.c_ubyte*8)()
    fn('glReadPixels',None,[I,I,I,I,U,U,P])(0,0,2,1,0x1908,0x1401,result)
    assert list(result)==[255,0,0,17,0,0,255,99],(label,list(result))
    assert geterror()==0
    print(label,'passed: red/blue/alpha')
check('CPU upload')
pbo=U();fn('glGenBuffers',None,[I,c.POINTER(U)])(1,c.byref(pbo))
bindbuf=fn('glBindBuffer',None,[U,U]);bindbuf(0x88EC,pbo)
fn('glBufferData',None,[U,c.c_ssize_t,P,U])(0x88EC,8,pixels,0x88E8)
image(0x0DE1,0,0x8058,2,1,0,0x1908,0x1401,None)
bindbuf(0x88EC,0)
check('PBO upload')
