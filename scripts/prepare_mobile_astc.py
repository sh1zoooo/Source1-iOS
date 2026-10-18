#!/usr/bin/env python3
"""Add ClientMod 2.0 VTF formats without renumbering the Source 1 ABI."""
from pathlib import Path
import shutil
import sys

root = Path(sys.argv[1])

def replace(path, old, new, count=1):
    file = root / path
    text = file.read_text()
    if text.count(old) != count:
        raise RuntimeError(f'ASTC patch context changed: {path}: {old[:90]}')
    file.write_text(text.replace(old, new))

shutil.copy2(Path(__file__).parents[1] / 'src/source/MobileASTC.hpp',
             root / 'public/bitmap/mobile_astc.h')
replace('public/bitmap/imageformat.h', '\tNUM_IMAGE_FORMATS\n',
        '''\t// On-disk ClientMod 2.0 IDs; 41/42 and 44..46 are reserved.
\tIMAGE_FORMAT_ASTC_4x4 = 43,
\tIMAGE_FORMAT_ASTC_6x6 = 47,
\tNUM_IMAGE_FORMATS
''')
replace('public/bitmap/imageformat.h', '\t\tD3DFMT_UNKNOWN\n',
        '''\t\tD3DFMT_ASTC_4x4 = 0x41535434,
\t\tD3DFMT_ASTC_6x6 = 0x41535436,
\t\tD3DFMT_UNKNOWN
''')
# D3DFMT_UNKNOWN is also part of the ABI: keep its original value (last enum
# before this insertion is ATI1N's FOURCC, not a sequential image-format ID).
replace('public/bitmap/imageformat.h', '\t\tD3DFMT_UNKNOWN\n',
        '\t\tD3DFMT_UNKNOWN = D3DFMT_ATI_1N + 1\n')
for path in ('bitmap/imageformat.cpp', 'bitmap/colorconversion.cpp', 'vtf/vtf.cpp'):
    f = root / path
    f.write_text('#include "bitmap/mobile_astc.h"\n' + f.read_text())
f = root / 'bitmap/imageformat.cpp'
s = f.read_text()
anchor = '\n};\n'
pos = s.index(anchor, s.index('g_ImageFormatInfo[]'))
s = s[:pos] + '''
    { "RESERVED_41", 0, 0, 0, 0, 0, false },
    { "RESERVED_42", 0, 0, 0, 0, 0, false },
    { "ASTC_4x4", 0, 8, 8, 8, 8, true },
    { "RESERVED_44", 0, 0, 0, 0, 0, false },
    { "RESERVED_45", 0, 0, 0, 0, 0, false },
    { "RESERVED_46", 0, 0, 0, 0, 0, false },
    { "ASTC_6x6", 0, 8, 8, 8, 8, true },
''' + s[pos:]
f.write_text(s)
replace('bitmap/imageformat.cpp', '\tif ( !mipmap )\n\t{',
        '''\tif ( !mipmap && MobileASTC::isFormat(imageFormat) )
        return MobileASTC::bytes(width, height, depth, imageFormat);
\tif ( !mipmap )
\t{''')
f = root / 'bitmap/imageformat.cpp'
s = f.read_text(); p = s.index('ImageFormat D3DFormatToImageFormat(')
q = s.index('{', s.index('switch', p)) + 1
s = s[:q] + '\n    case D3DFMT_ASTC_4x4: return IMAGE_FORMAT_ASTC_4x4;\n    case D3DFMT_ASTC_6x6: return IMAGE_FORMAT_ASTC_6x6;' + s[q:]
p = s.index('D3DFORMAT ImageFormatToD3DFormat(')
q = s.index('{', s.index('switch', p)) + 1
s = s[:q] + '\n    case IMAGE_FORMAT_ASTC_4x4: return D3DFMT_ASTC_4x4;\n    case IMAGE_FORMAT_ASTC_6x6: return D3DFMT_ASTC_6x6;' + s[q:]
f.write_text(s)
replace('bitmap/colorconversion.cpp', '\t// HDRFIXME: WE NEED A BIGGER INTERMEDIATE FORMAT!!!!!',
        '''    if (MobileASTC::isFormat(srcImageFormat)) {
        const int block = MobileASTC::block(srcImageFormat);
        const int required = MobileASTC::bytes(width, height, 1, srcImageFormat);
        if (!required || !src || !dst) return false;
        const int row = ((width + block - 1) / block) * 16;
        const int rows = (height + block - 1) / block;
        if (srcStride && srcStride < row) return false;
        CUtlMemory<uint8> packed;
        const uint8 *input = src;
        if (srcStride && srcStride != row) {
            packed.EnsureCapacity(required);
            for (int y = 0; y < rows; ++y)
                memcpy(packed.Base() + y * row, src + y * srcStride, row);
            input = packed.Base();
        }
        if (srcImageFormat == dstImageFormat) {
            if (dstStride && dstStride < row) return false;
            for (int y = 0; y < rows; ++y)
                memcpy(dst + y * (dstStride ? dstStride : row), input + y * row, row);
            return true;
        }
        if (MobileASTC::isFormat(dstImageFormat)) return false; // no encoder
        const uint64_t pixelBytes = uint64_t(width) * height * 4;
        if (pixelBytes > INT_MAX) return false;
        CUtlMemory<uint8> decoded;
        decoded.EnsureCapacity(int(pixelBytes));
        if (!MobileASTC::decodeBGRA(input, required, width, height, srcImageFormat,
                                   decoded.Base(), size_t(pixelBytes))) return false;
        return ConvertImageFormat(decoded.Base(), IMAGE_FORMAT_BGRA8888, dst,
                                  dstImageFormat, width, height, 0, dstStride);
    }
\t// HDRFIXME: WE NEED A BIGGER INTERMEDIATE FORMAT!!!!!''')
replace('vtf/vtf.cpp', '( header.imageFormat >= NUM_IMAGE_FORMATS )',
        '''( header.imageFormat >= NUM_IMAGE_FORMATS ) ||
         (header.imageFormat > IMAGE_FORMAT_DXT5_RUNTIME && !MobileASTC::isFormat(header.imageFormat))''')
replace('vtf/vtf.cpp',
        '\tm_nMipCount = ComputeMipCount();\n\n\tm_nFinestMipmapLevel = 0;',
        '''\tm_nMipCount = ComputeMipCount();
    if (MobileASTC::isFormat(m_Format)) {
        // CM2 ships single-mip UI textures. Do not allocate/decode undefined
        // lower mips using the legacy DXT backward-compatibility convention.
        if (header.numMipLevels < 1 || header.numMipLevels > m_nMipCount) return false;
        m_nMipCount = header.numMipLevels;
    }
\tm_nFinestMipmapLevel = 0;''')
replace('vtf/vtf.cpp', '\treturn ImageLoader::SizeInBytes( m_Format ) * nWidth;',
        '''    if (MobileASTC::isFormat(m_Format))
        return MobileASTC::bytes(nWidth, 1, 1, m_Format);
\treturn ImageLoader::SizeInBytes( m_Format ) * nWidth;''')
replace('vtf/vtf.cpp', '\treturn ImageLoader::SizeInBytes( m_Format ) * nWidth * nHeight;',
        '''    if (MobileASTC::isFormat(m_Format))
        return MobileASTC::bytes(nWidth, nHeight, 1, m_Format);
\treturn ImageLoader::SizeInBytes( m_Format ) * nWidth * nHeight;''')
print('Prepared ClientMod 2.0 ASTC VTF and bitmap formats')
