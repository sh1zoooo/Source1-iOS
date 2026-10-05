# 0.11.0 / build 14 — displacement geometry and camera collision

## Implemented

- Original `CCoreDispInfo::CreateWithoutLOD` generates full-resolution terrain
  triangles from validated BSP parent quads and DISPINFO/DISP_VERTS/DISP_TRIS.
- Powers 2, 3 and 4 are checked against original Source limits. Raw and LZMA
  geometry sections use the same bounded import preflight.
- Original `CDispCollTree` ray and swept-hull routines are compiled in tool mode,
  with separate names, heap-backed aligned vectors, and matching headers. This
  avoids instantiating the engine's hunk-backed ABI from the preview adapter.
- Imported terrain collision trees are staged before scene replacement and
  reclaimed on rejection, map reset and shutdown.
- Camera hull movement combines ordinary polygon IVP with Source displacement
  sweeps. The engine CM world remains the built-in room, not an imported game map.
- `source_bsp_terrain` loads a generated hill fixture. `source_bsp_reset` restores
  the original room. The terrain fixture has 114 triangles versus the room's 84.

## Native evidence

Local Debug-IVP probe: runtime contracts exited 0, no FAIL/assertion failure.
55 startup checks include terrain powers 2–4, invalid metadata rejection, and
original Source ray + hull hits at a 32-unit summit. End-to-end imported terrain
adds exactly 90 draw vertices; invalid power 31 is rejected without replacing it.
Compressed BSP import and lifecycle/physics/camera regression checks still pass.

## Boundaries and open defects

The test quad uses Source's clockwise displacement parent ordering. Geometry
uses full-resolution triangles; there is no neighbor LOD/seam stitching, material
blending, alpha shading, map lightmaps or displacement surface-physics flag parity.

An exploratory generic IVP triangle-soup trace against an isolated sloped patch
did not hit at the correct height. Camera terrain collision is therefore checked
using Source's specialized displacement tree. Dynamic IVP bodies on terrain are
not validated; no claim of working terrain ragdolls is made. Contact-rescue
warnings and restart RSS growth recorded in VALIDATION-0.10.0 remain open.

GitHub full Debug / iOS ARM64 / simulator GPU terrain checks: pending.
Physical iPhone validation: pending; user is currently unavailable to test.

Full CS:S is not running: client host, graphical shaders/materials, models,
animation, sound/UI and compatible game client/server modules remain unported.
