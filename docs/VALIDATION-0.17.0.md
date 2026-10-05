# 0.17.0 / build 20 — bounded BSP multi-material atlas

The BSP preview now preserves per-surface TEXINFO/TEXDATA material selection.
Up to 16 distinct safe material names are resolved through VMT/VTF, resampled
into bounded 64x64 atlas tiles and selected per vertex. Duplicate names share a
slot; additional names deliberately fall back to slot zero. The Metal fragment
wraps local UVs inside a clamped atlas tile, preventing repeat sampling from
crossing into a neighboring material.

The generated BSP alternates `debug/debugempty` brick/mortar and
`debug/debugblue` grid surfaces. Tests require a 128x64 two-slot atlas, distinct
tile pixels, both material IDs in render vertices, import/reload behavior and
preservation on rejected map loads. Model texture remains a separate slot.

Local Debug runtime contracts pass with 69 startup checks. Simulator smoke
expects 139 PASS, both BSP/studio uploads, GPU completion and lifecycle checks.

Boundaries: 16 preview material slots, fixed 64x64 resampling, base textures
only. Lightmaps, HDR data, alpha, detail/bump/env maps, proxies and the original
graphical materialsystem remain pending.

## Verified Actions artifact

Commit `2017521b017c2c99716fd42f646171e5e21d8b49`,
[run 37354477235](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37354477235)
passed Linux runtime contracts, iPhone ARM64 build, simulator build/startup,
139 PASS, both BSP/studio uploads, GPU completion and pause/resume. Downloaded
artifact 11364222411 was checked independently. Screenshot shows blue grid and
brown brick surfaces together with the separately textured posed model.
IPA SHA256 matches its manifest:
`abfca004935c73f74a192d7837122279efbc4339a5adeb629b6a36994cb63a09`.
Physical-phone confirmation remains at 0.15; 0.17 is simulator-verified.
