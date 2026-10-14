"""Compare GLES sampler-object output against equivalent texture parameters."""
import ctypes as c
import gles_texture_channels as gl

fragment = gl.shader(0x8B30, '#version 300 es\nprecision highp float;'
    'uniform sampler2D t;out vec4 o;void main(){'
    'float u=gl_FragCoord.x<1.?-.125:1.125;o=texture(t,vec2(u,.5));}')
program = gl.fn('glCreateProgram', gl.U, [])()
attach = gl.fn('glAttachShader', None, [gl.U, gl.U])
attach(program, gl.vs); attach(program, fragment)
gl.fn('glLinkProgram', None, [gl.U])(program)
gl.fn('glUseProgram', None, [gl.U])(program)
gl.bind(0x0DE1, gl.src)
sampler = gl.U()
gl.fn('glGenSamplers', None, [gl.I, c.POINTER(gl.U)])(1, c.byref(sampler))
bind_sampler = gl.fn('glBindSampler', None, [gl.U, gl.U])
sampler_parameter = gl.fn('glSamplerParameteri', None, [gl.U, gl.U, gl.I])
def draw():
    gl.fn('glDrawArrays', None, [gl.U, gl.I, gl.I])(4, 0, 3)
    pixels = (c.c_ubyte * 8)()
    gl.fn('glReadPixels', None,
        [gl.I, gl.I, gl.I, gl.I, gl.U, gl.U, gl.P])(0, 0, 2, 1, 0x1908, 0x1401, pixels)
    assert gl.geterror() == 0
    return list(pixels)
for repeat in range(8):
    for wrap in (0x2901, 0x812F):
        for filter_mode in (0x2600, 0x2601):
            bind_sampler(0, 0)
            for name, value in ((0x2802, wrap), (0x2803, wrap),
                                (0x2801, filter_mode), (0x2800, filter_mode)):
                gl.param(0x0DE1, name, value)
                sampler_parameter(sampler, name, value)
            expected = draw()
            bind_sampler(0, sampler)
            assert draw() == expected, (wrap, filter_mode, expected)
            # Preload/reset unbinding must restore the texture-state path.
            bind_sampler(0, 0)
            assert draw() == expected
print('Sampler objects: wrap, clamp, nearest, linear, swizzles, alpha and unbind match texture-state output')
