#!/usr/bin/env python3
"""Keep ASTC compressed through Source's D3D compatibility and GLES upload."""
from pathlib import Path
import sys
root = Path(sys.argv[1])
def replace(path, old, new, count=1):
    p = root / path
    text = p.read_text()
    if text.count(old) != count:
        raise RuntimeError(f'ASTC graphics patch context changed: {path}: {old[:90]}')
    p.write_text(text.replace(old, new))

for path in ('materialsystem/ctexture.cpp', 'materialsystem/shaderapidx9/colorformatdx8.cpp',
             'togles/linuxwin/cglmtex.cpp', 'togles/linuxwin/glmgr.cpp'):
    p = root / path
    p.write_text('#include "bitmap/mobile_astc.h"\n' + p.read_text())
replace('materialsystem/ctexture.cpp',
        'ImageFormat CTexture::ComputeActualFormat( ImageFormat srcFormat )\n{',
        '''ImageFormat CTexture::ComputeActualFormat( ImageFormat srcFormat )
{
    // The GLES upload owns native capability detection and the CPU fallback.
    if (MobileASTC::isFormat(srcFormat)) return srcFormat;''')
replace('materialsystem/shaderapidx9/colorformatdx8.cpp', '\tswitch(fmt)\n\t{',
        '''\tswitch(fmt)
\t{
    case IMAGE_FORMAT_ASTC_4x4:
    case IMAGE_FORMAT_ASTC_6x6:
        return (!isRenderTarget && !bIsVertexTexture) ?
            ImageLoader::ImageFormatToD3DFormat(fmt) : D3DFMT_UNKNOWN;''')
replace('togles/linuxwin/dxabstract.cpp', '\t\t\t\t\t\tcase D3DFMT_DXT1:',
        '''                        case D3DFMT_ASTC_4x4:
                        case D3DFMT_ASTC_6x6:
                            // Sampled textures only: no runtime ASTC encoder.
                            legalUsage = D3DUSAGE_QUERY_FILTER | D3DUSAGE_QUERY_SRGBREAD;
                            break;
\t\t\t\t\t\tcase D3DFMT_DXT1:''')
replace('togles/linuxwin/cglmtex.cpp', 'const GLMTexFormatDesc g_formatDescTable[] = \n{',
        '''const GLMTexFormatDesc g_formatDescTable[] =
{
    { "_ASTC4x4", D3DFMT_ASTC_4x4, 0x93B0, 0x93D0, GL_RGBA, GL_UNSIGNED_BYTE, 4, 16 },
    { "_ASTC6x6", D3DFMT_ASTC_6x6, 0x93B4, 0x93D4, GL_RGBA, GL_UNSIGNED_BYTE, 6, 16 },''')
# 6x6 blocks must round UP; power-of-two dimensions aren't multiples of six.
replace('togles/linuxwin/cglmtex.cpp', '(storage_x / formatDesc->m_chunkSize )',
        '((storage_x + formatDesc->m_chunkSize - 1) / formatDesc->m_chunkSize )')
replace('togles/linuxwin/cglmtex.cpp', '(storage_y / formatDesc->m_chunkSize )',
        '((storage_y + formatDesc->m_chunkSize - 1) / formatDesc->m_chunkSize )')
replace('togles/linuxwin/cglmtex.cpp', '(m_layout->m_slices[sliceIndex].m_xSize / format->m_chunkSize)',
        '((m_layout->m_slices[sliceIndex].m_xSize + format->m_chunkSize - 1) / format->m_chunkSize)')
replace('togles/linuxwin/cglmtex.cpp', '(m_layout->m_slices[sliceIndex].m_ySize / format->m_chunkSize)',
        '((m_layout->m_slices[sliceIndex].m_ySize + format->m_chunkSize - 1) / format->m_chunkSize)')
# CPU fallback needs an actual pointer, not a mapped PBO offset. ASTC textures
# in the resource pack are immutable; use ordinary bounded staging for them.
replace('togles/linuxwin/cglmtex.cpp', 'if( m_layout->m_key.m_texFlags & kGLMTexDynamic )',
        '''if( (m_layout->m_key.m_texFlags & kGLMTexDynamic) &&
        MobileASTC::fromGL(m_layout->m_format->m_glIntFormat) < 0 )''')
# The mapping decision in Lock must match PBO creation.
replace('togles/linuxwin/cglmtex.cpp', 'if( m_layout->m_key.m_texFlags & kGLMTexDynamic && m_mapped )',
        'if( m_pbo && m_mapped )')
replace('togles/linuxwin/cglmtex.cpp', 'if (!m_backing && !(m_layout->m_key.m_texFlags & kGLMTexDynamic))',
        'if (!m_backing && !m_pbo)')
replace('togles/linuxwin/cglmtex.cpp',
        'if ( (m_layout->m_key.m_texFlags & kGLMTexDynamic) || (params->m_readonly && copyout) )',
        'if ( m_pbo || (params->m_readonly && copyout) )')
# Do not recycle ASTC objects through the desktop DXT garbage-data shrinker.
replace('togles/linuxwin/glmgr.cpp',
        'if ( texBind != GL_TEXTURE_2D || !gl_batch_tex_destroys.GetBool() )',
        '''if ( MobileASTC::fromGL(pLayout->m_format->m_glIntFormat) >= 0 ||
         texBind != GL_TEXTURE_2D || !gl_batch_tex_destroys.GetBool() )''')
helper = r'''
// ASTC capability belongs to the current context, not to the VTF file. iOS
// drivers may omit ASTC even on hardware that supports it through another API.
static bool MobileASTCNativeFormat(GLenum format) {
    static SDL_GLContext context = NULL;
    static CUtlVector<GLint> formats;
    SDL_GLContext current = SDL_GL_GetCurrentContext();
    if (current != context || !context) {
        context = current;
        GLint count = 0;
        gGL->glGetIntegerv(0x86A2, &count); // GL_NUM_COMPRESSED_TEXTURE_FORMATS
        formats.SetCount((count > 0 && count <= 512) ? count : 0);
        if (formats.Count()) gGL->glGetIntegerv(0x86A3, formats.Base());
    }
    for (int i = 0; i < formats.Count(); ++i)
        if (GLenum(formats[i]) == format) return true;
    return false;
}

static bool MobileASTCUpload(GLenum target, GLint level, GLenum internalformat,
                            GLsizei width, GLsizei height, GLint border,
                            GLsizei imageSize, const GLvoid *data) {
    const int format = MobileASTC::fromGL(internalformat);
    if (format < 0) return false;
    const int required = MobileASTC::bytes(width, height, 1, format);
    if (!required || border != 0 || imageSize != required) {
        Warning("iOS ASTC rejected invalid upload: format=0x%x size=%dx%d bytes=%d expected=%d\n",
                internalformat, width, height, imageSize, required);
        return true;
    }
    const bool native = MobileASTCNativeFormat(internalformat);
    static unsigned reports = 0;
    if (reports++ < 8)
        Warning("iOS ASTC: format=%d block=%dx%d path=%s mip=%d size=%dx%d\n",
                format, MobileASTC::block(format), MobileASTC::block(format),
                native ? "native compressed" : "CPU decode at upload", level, width, height);
    if (native) {
        // This path always receives CPU staging, including dynamic ASTC.
        // An unrelated PBO must not reinterpret that pointer as an offset.
        GLint pbo = 0;
        gGL->glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &pbo);
        gGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        gGL->glCompressedTexImage2D(target, level, internalformat, width, height,
                                  0, imageSize, data);
        gGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        return true;
    }
    const uint64_t pixelBytes = uint64_t(width) * height * 4;
    if (pixelBytes > INT_MAX) return true;
    CUtlMemory<uint8> pixels;
    pixels.EnsureCapacity(int(pixelBytes));
    if (data) {
        if (!MobileASTC::decodeBGRA(data, imageSize, width, height, format,
                                   pixels.Base(), size_t(pixelBytes))) return true;
        // Decoder's BGRA output -> RGBA. ASTC descriptors do not use the old
        // DXT/BGR swizzle; alpha and the existing sRGB selection are preserved.
        for (size_t p = 0; p < size_t(pixelBytes); p += 4) {
            uint8 tmp = pixels[p]; pixels[p] = pixels[p + 2]; pixels[p + 2] = tmp;
        }
    } else memset(pixels.Base(), 0, size_t(pixelBytes));
    GLint pbo = 0, row = 0, skipX = 0, skipY = 0, alignment = 0;
    gGL->glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &pbo);
    gGL->glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row);
    gGL->glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipX);
    gGL->glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipY);
    gGL->glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    gGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    gGL->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gGL->glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    gGL->glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    gGL->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gGL->glTexImage2D(target, level, MobileASTC::srgbGL(internalformat) ?
                     GL_SRGB8_ALPHA8 : GL_RGBA8, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixels.Base());
    gGL->glPixelStorei(GL_UNPACK_ROW_LENGTH, row);
    gGL->glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipX);
    gGL->glPixelStorei(GL_UNPACK_SKIP_ROWS, skipY);
    gGL->glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    gGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    return true;
}
'''
p = root / 'togles/linuxwin/cglmtex.cpp'
s = p.read_text()
s = '#include "SDL.h"\n' + s
anchor = 'void CompressedTexImage2D(GLenum target, GLint level, GLenum internalformat,'
pos = s.index(anchor)
s = s[:pos] + helper + '\n' + s[pos:]
p.write_text(s)
replace('togles/linuxwin/cglmtex.cpp', '\tIOS_RENDER_TIMING("texture_decompress_upload");',
        '''\tIOS_RENDER_TIMING("texture_decompress_upload");
    if (MobileASTCUpload(target, level, internalformat, width, height, border, imageSize, data)) return;''')
replace('togles/linuxwin/cglmtex.cpp', 'if( gGL->m_bHave_GL_EXT_texture_compression_dxt1 )',
        '''if (MobileASTC::fromGL(intformat) >= 0)
                    MobileASTCUpload(target, desc->m_req.m_mip, intformat, slice->m_xSize,
                                     slice->m_ySize, 0, slice->m_storageSize, sliceAddress);
                else if( gGL->m_bHave_GL_EXT_texture_compression_dxt1 )''')
print('Prepared native ASTC GLES upload with per-context format query and CPU fallback')
