"""Compile production buffer hints/counters; validate GLES storage and byte accuracy."""
from pathlib import Path
import ctypes as c
import subprocess,tempfile,sys
import gles_texture_channels as gl
root=Path(sys.argv[1])
s=(root/'togles/linuxwin/cglmbuffer.cpp').read_text()
start=s.index('\t\tswitch (m_type)',s.index('CGLMBuffer::CGLMBuffer('))
end=s.index('\n\t\tgGL->glBufferData(',start)
switch=s[start:end]
source=r'''
#include <cassert>
using GLenum=unsigned;
constexpr unsigned GL_STATIC_DRAW=0x88E4,GL_DYNAMIC_DRAW=0x88E8,GL_STREAM_DRAW=0x88E0;
enum { kGLMVertexBuffer,kGLMIndexBuffer,kGLMUniformBuffer,kGLMPixelBuffer };
#define Assert assert
#define DXABSTRACT_BREAK_ON_ERROR() assert(false)
extern "C" unsigned hintFor(int m_type,bool m_bDynamic){GLenum hint=GL_STREAM_DRAW;SWITCH return hint;}
'''.replace('SWITCH',switch)
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp); (p/'hint.cpp').write_text(source)
 subprocess.run(['c++','-std=c++17','-DSOURCE_IOS=1','-shared','-fPIC','-Wall','-Wextra','-Werror',str(p/'hint.cpp'),'-o',str(p/'hint.so')],check=True)
 hints=c.CDLL(str(p/'hint.so')).hintFor;hints.argtypes=[c.c_int,c.c_bool];hints.restype=c.c_uint
 assert [hints(i,False) for i in range(4)]==[0x88E4,0x88E4,0x88E8,0x88E0]
 assert all(hints(i,True)==0x88E8 for i in range(4))
 gen=gl.fn('glGenBuffers',None,[gl.I,c.POINTER(gl.U)])
 bind=gl.fn('glBindBuffer',None,[gl.U,gl.U])
 data=gl.fn('glBufferData',None,[gl.U,c.c_ssize_t,gl.P,gl.U])
 parameter=gl.fn('glGetBufferParameteriv',None,[gl.U,gl.U,c.POINTER(gl.I)])
 mapping=gl.fn('glMapBufferRange',gl.P,[gl.U,c.c_ssize_t,c.c_ssize_t,gl.U])
 unmap=gl.fn('glUnmapBuffer',c.c_ubyte,[gl.U])
 for kind,target in [(0,0x8892),(1,0x8893)]:
  for dynamic in [False,True]:
   b=gl.U();gen(1,c.byref(b));bind(target,b)
   expected=bytes(range(64));array=(c.c_ubyte*64).from_buffer_copy(expected)
   data(target,64,array,hints(kind,dynamic))
   usage=gl.I();parameter(target,0x8765,c.byref(usage))
   assert usage.value==hints(kind,dynamic)
   ptr=mapping(target,0,64,1) # MAP_READ_BIT
   assert ptr and c.string_at(ptr,64)==expected
   assert unmap(target) and gl.geterror()==0
 # Compile actual counter header against narrow platform logging stubs.
 inc=p/'tier0';inc.mkdir();(inc/'platform.h').write_text('')
 (inc/'dbg.h').write_text('inline void Warning(const char*,...) {}\n')
 (p/'stats.cpp').write_text('''#include "IOSRenderWorkStats.hpp"
#include <cassert>
int main(){IOSRenderWorkStats s;s.draw(4,12,5,12,true);s.draw(5,8,0,7,false);
s.draw(1,4,0,3,false);s.draw(5,1,0,0,false);
assert(s.draws==4&&s.indices==25&&s.primitives==12&&s.indexRange==21&&s.dynamicIndexDraws==1);
s.write(false,false,64);s.write(false,true,32);s.write(true,false,16);s.write(true,true,8);
s.orphan(4096);assert(s.writeBytes[0][0]==64&&s.writeBytes[0][1]==32&&s.writeBytes[1][0]==16&&s.writeBytes[1][1]==8&&s.orphanBytes==4096);
s.report(10);s.report(14);assert(s.draws==4);s.report(15);assert(s.draws==0&&s.reported==15&&s.orphans==0);}
''')
 subprocess.run(['c++','-std=c++17','-DSOURCE_IOS=1','-Wall','-Wextra','-Werror','-I'+str(p),'-Isrc/source',str(p/'stats.cpp'),'-o',str(p/'stats')],check=True)
 subprocess.run([str(p/'stats')],check=True)
print('Production static/dynamic usage hints preserve GLES bytes; submitted work counters/reset passed')
