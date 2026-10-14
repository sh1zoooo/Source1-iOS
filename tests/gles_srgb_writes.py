"""Exercise the prepared translator's actual sRGB suffix on a GLES context.

Usage: python3 tests/gles_srgb_writes.py <prepared>/togles/linuxwin/dx9asmtogl2.cpp
Requires Mesa EGL with surfaceless GLES 3 support.
"""
import ast
import ctypes as c
from pathlib import Path
import re
import sys
import gles_texture_channels as gl

source = Path(sys.argv[1]).read_text()
start = source.index('// sRGB Write suffix')
end = source.index('if( !gGL->m_bHave_GL_QCOM_alpha_test', start)
suffix = ''.join(ast.literal_eval(s) for s in re.findall(
    r'StrcatToALUCode\(\s*("(?:[^"\\]|\\.)*")\s*\);', source[start:end]))
assert 'max(gl_FragData[0].xyz' in suffix
suffix = suffix.replace('gl_FragData[0]', 'o')
fragment = gl.shader(0x8B30, '#version 300 es\nprecision highp float;'
    'uniform float flSRGBWrite;out vec4 o;void main(){'
    'o=gl_FragCoord.x<1.?vec4(.05,.25,.5,.4):vec4(0.,0.,0.,1.);'
    + suffix + '}')
program = gl.fn('glCreateProgram', gl.U, [])()
attach = gl.fn('glAttachShader', None, [gl.U, gl.U])
attach(program, gl.vs)
attach(program, fragment)
gl.fn('glLinkProgram', None, [gl.U])(program)
gl.fn('glUseProgram', None, [gl.U])(program)
location = gl.fn('glGetUniformLocation', gl.I, [gl.U, c.c_char_p])(
    program, b'flSRGBWrite')
assert location >= 0
for enabled, expected in ((0., [13, 64, 128]), (1., [65, 136, 186]),
                          (0., [13, 64, 128]), (1., [65, 136, 186])):
    gl.fn('glUniform1f', None, [gl.I, c.c_float])(location, enabled)
    gl.fn('glDrawArrays', None, [gl.U, gl.I, gl.I])(4, 0, 3)
    pixels = (c.c_ubyte * 8)()
    gl.fn('glReadPixels', None,
          [gl.I, gl.I, gl.I, gl.I, gl.U, gl.U, gl.P])(
              0, 0, 2, 1, 0x1908, 0x1401, pixels)
    assert all(abs(pixels[i] - expected[i]) <= 1 for i in range(3)), list(pixels)
    assert pixels[3] == 102 and list(pixels[4:]) == [0, 0, 0, 255], list(pixels)
    assert gl.geterror() == 0
print('Prepared shader suffix: sRGB on/off, shadow detail, black and alpha passed')
