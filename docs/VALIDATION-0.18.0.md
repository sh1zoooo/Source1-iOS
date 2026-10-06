# 0.18.0 / build 21 — BSP LDR lightmaps and bounded material compression

The preview reads original BSP LUMP_LIGHTING RGBExp32 samples and per-face
lightmap offsets, dimensions, style counts and TEXINFO luxel axes. It stages a
1024x1024 RGBA atlas, adds replicated one-pixel borders and assigns independent
lightmap UVs. Metal samples it per fragment alongside the material atlas.
Lit brush surfaces use their material colors, without the old debug palette.
Models and physics probes do not sample the world lightmap.

The generated BSP stores a synthetic baked gradient for all 42 surfaces.
It tests format loading and rendering; it is not a radiosity compiler.
Fixture UVs now use XY for horizontal faces, XZ for Y-facing walls and YZ for
X-facing walls. A triangle-area check catches collapsed texture coordinates.

All five consumed material/lighting lumps use the bounded Source streaming
LZMA decoder already used for geometry. Each decoded lump is limited to 16 MiB;
the combined consumed-lump budget is 64 MiB. Texture/luxel vectors must be finite.
Legacy CMapLoadHelper::Uncompress is bypassed for these compressed lumps.

Bounds: LDR only, first static lightstyle, lightmap dimensions up to 256x256,
one 1024x1024 atlas (maps exceeding capacity are rejected transactionally).
All style and bump sample ranges are validated, but additional styles/bump maps
are not rendered. RGBExp32 values are clamped and gamma-encoded for this LDR
adapter; HDR tone mapping, original Source shader behavior and gamma parity,
dynamic lights/shadows and the graphical materialsystem remain pending.

Local Debug runtime checks cover compressed material names, corrupted streams,
nonfinite axes, malformed lightmap offsets, preservation of the last valid
scene/texture revisions, atlas borders, model isolation, map/terrain reload and
lifecycle. Startup now includes 73 Source checks. Simulator smoke requires
147 PASS, both material uploads, lightmap upload, GPU completion and pause/resume.
The new iPhone build is not yet physically tested.
Actions run 37365664037 passed Linux runtime contracts. Its macOS job remained
queued while work continued into 0.19; GPU verification must come from the
combined build rather than be inferred from the native tests.
