# ClientMod 2.0 ASTC resources — 0.46.25 / build 75

The 2.0 resource pack uses VTF image IDs 43 (ASTC 4x4) and 47 (ASTC 6x6).
0.46.24 rejected those IDs before reading the texture, including the menu
background. The existing Source image and D3D enum values remain unchanged.

This change adds format metadata, rounded block sizes, mip/frame/cubemap layout,
block-row copying and explicit BGRA/RGBA conversions. ASTC VTFs use their actual
file mip count; some 2.0 UI textures contain only one mip. Legacy formats retain
the existing reader behavior. Unsupported reserved IDs are still rejected.

ASTC remains compressed through the material system and ToGLES. Each current GL
context's advertised compressed-format list selects the upload:

* Native format present: original blocks go to glCompressedTexImage2D.
* Format absent: decode once per upload, upload RGBA8 or SRGB8_ALPHA8, release
  temporary pixels. No ASTC decoding occurs in the draw loop.

The user's A18 GLES extension log does not advertise ASTC. Hardware support via
Metal alone does not prove that the EAGL context exposes it. The runtime logs
the selected path for the first eight uploads. This is a compatibility change;
an FPS gain on the device has not been measured.

ASTC staging does not use PBO mapping. Both native and fallback uploads preserve
the previous unpack-buffer binding; fallback also preserves unpack pixel state.
ASTC objects bypass the desktop DXT garbage-data texture recycling path.
The fallback is Arm's Apache-2.0-licensed astcenc 5.0.0, pinned in
third_party/astcenc and built in decompression-only mode. Its license is bundled
in the IPA. Per-thread 4x4/6x6 contexts reuse block tables across uploads.

Validation includes synthetic ASTC constant-color blocks, alpha/channel checks,
6x6 ceiling division, block-row pitch, VTF mip/frame/cube round trips and mip
skipping. Actual 2.0 VTFs were extracted from the supplied ZIP/VPK and checked
against the ZIP CRC, then read and converted by the production VTF/bitmap code:

* cm background01_widescreen: 2048x2048, format 43, one mip.
* hl2 background01_widescreen: 512x512, format 47, ten mips.
* hl2 debugluxelsnoalpha: 16x16, format 43, five mips.

The GLES test compiles the production upload functions and verifies fallback
pixels/sRGB/alpha, mip uploads, unpack/PBO restoration, rejected lengths and
context changes. Native dispatch is checked with an advertised-format API
fixture; a native ASTC upload on the user's iPhone still needs device validation.

This adds support for the observed resource formats. It does not replace the
embedded ClientMod 1.4 game code with the 2.0 game binary or establish complete
2.0 gameplay/network compatibility.
