"""GLES render equivalence for the production uniform cache and streaming maps.

Run from the repository root; requires a C++ compiler and surfaceless Mesa EGL.
"""
import ctypes as c
from pathlib import Path
import subprocess
import tempfile
import gles_texture_channels as gl

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as folder:
    library = str(Path(folder) / 'cache.so')
    subprocess.run(['c++', '-std=c++17', '-shared', '-fPIC', '-O2',
        '-I' + str(root / 'src/source'),
        str(root / 'tests/ios_uniform_cache_gles_bridge.cpp'), '-o', library], check=True)
    bridge = c.CDLL(library)
    bridge.make_cache.restype = gl.P
    cache = bridge.make_cache()
    callback_type = c.CFUNCTYPE(None, gl.I, gl.I, c.POINTER(c.c_float))
    bridge.update_cache.argtypes = [gl.P, gl.I, gl.I, c.POINTER(c.c_float), callback_type]
    bridge.update_cache.restype = c.c_bool
    bridge.free_cache.argtypes = [gl.P]

    vertex = gl.shader(0x8B31, '#version 300 es\nlayout(location=0)in vec2 p;'
        'void main(){gl_Position=vec4(p,0.,1.);}')
    fragment = gl.shader(0x8B30, '#version 300 es\nprecision highp float;'
        'uniform vec4 pc[4];out vec4 o;void main(){o=pc[0]+pc[1]+pc[2]+pc[3];}')
    program = gl.fn('glCreateProgram', gl.U, [])()
    attach = gl.fn('glAttachShader', None, [gl.U, gl.U])
    attach(program, vertex); attach(program, fragment)
    gl.fn('glLinkProgram', None, [gl.U])(program)
    gl.fn('glUseProgram', None, [gl.U])(program)
    location = gl.fn('glGetUniformLocation', gl.I, [gl.U, c.c_char_p])(program, b'pc[0]')
    assert location >= 0
    uploads = []
    @callback_type
    def upload(offset, count, data):
        uploads.append((offset, count))
        gl.fn('glUniform4fv', None, [gl.I, gl.I, c.POINTER(c.c_float)])(
            location + offset, count, data)

    values = (c.c_float * 16)(.2, .3, .4, .5)
    buffer = gl.U()
    gl.fn('glGenBuffers', None, [gl.I, c.POINTER(gl.U)])(1, c.byref(buffer))
    gl.fn('glBindBuffer', None, [gl.U, gl.U])(0x8892, buffer)
    vertices = (c.c_float * 6)(-1, -1, 3, -1, -1, 3)
    for frame in range(100):
        if frame % 25 == 0:
            values[8] = (frame // 25) * .1
        bridge.update_cache(cache, 0, 4, values, upload)
        offset = (frame % 8) * 32
        if offset == 0:
            gl.fn('glBufferData', None, [gl.U, c.c_ssize_t, gl.P, gl.U])(
                0x8892, 256, None, 0x88E0)
        # DISCARD at the start, NOOVERWRITE for subsequent disjoint regions.
        flags = 0x2 | 0x10 | (0x8 if offset == 0 else 0x20)
        address = gl.fn('glMapBufferRange', gl.P,
            [gl.U, c.c_ssize_t, c.c_ssize_t, gl.U])(0x8892, offset, 32, flags)
        assert address
        c.memmove(address, vertices, c.sizeof(vertices))
        gl.fn('glFlushMappedBufferRange', None,
            [gl.U, c.c_ssize_t, c.c_ssize_t])(0x8892, 0, c.sizeof(vertices))
        assert gl.fn('glUnmapBuffer', c.c_ubyte, [gl.U])(0x8892)
        gl.fn('glVertexAttribPointer', None,
            [gl.U, gl.I, gl.U, c.c_ubyte, gl.I, gl.P])(0, 2, 0x1406, 0, 8, offset)
        gl.fn('glEnableVertexAttribArray', None, [gl.U])(0)
        gl.fn('glDrawArrays', None, [gl.U, gl.I, gl.I])(4, 0, 3)
        result = (c.c_ubyte * 8)()
        gl.fn('glReadPixels', None,
            [gl.I, gl.I, gl.I, gl.I, gl.U, gl.U, gl.P])(0, 0, 2, 1, 0x1908, 0x1401, result)
        expected = [(.2 + values[8]) * 255, .3 * 255, .4 * 255, .5 * 255]
        assert all(abs(result[i] - expected[i % 4]) <= 1 for i in range(8)), list(result)
        assert gl.geterror() == 0
    assert uploads == [(0, 4), (2, 1), (2, 1), (2, 1)], uploads
    bridge.free_cache(cache)
    print('100 GLES frames: unchanged pixels; 4 uniform uploads; orphan/NOOVERWRITE/partial flush passed')
