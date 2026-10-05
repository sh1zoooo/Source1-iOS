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

Additional local soak: 30,000 frames across three restarts and 300 switches
between the room and terrain exited 0. Every returned draw coordinate was finite.
There were 606 IVP contact-rescue warnings. Maximum RSS was 17,444 → 18,212 →
18,852 KiB; this does not establish leak-free operation or stable contact physics.

## Boundaries and open defects

The test quad uses Source's clockwise displacement parent ordering. Geometry
uses full-resolution triangles; there is no neighbor LOD/seam stitching, material
blending, alpha shading, map lightmaps or displacement surface-physics flag parity.

An exploratory generic IVP triangle-soup trace against an isolated sloped patch
did not hit at the correct height. Camera terrain collision is therefore checked
using Source's specialized displacement tree. Dynamic IVP bodies on terrain are
not validated; no claim of working terrain ragdolls is made. Contact-rescue
warnings and restart RSS growth recorded in VALIDATION-0.10.0 remain open.

GitHub run `37300762083`, code `426e774029f18a010edbc5db889121daab6ac140`:
full Linux Debug runtime contracts passed in 5.08 seconds. iOS ARM64 and simulator
builds passed. Simulator log contains exactly 111 PASS entries, no FAIL, GPU frame
completion, terrain import (114 triangles), and pause/resume. Simulator screenshot
was visually inspected: checker geometry and both yellow physics spheres render.

Artifact `11341058893` was downloaded and inspected: Info.plist is 0.11.0 / 14;
the IPA SHA256 agrees with the included checksum:
`526bc2579454ebe0ecacddeaed6cce6bb694f61cdf6fbc49006d55ff1228e135`.

Physical iPhone validation: pending; user is currently unavailable to test.

Full CS:S is not running: client host, graphical shaders/materials, models,
animation, sound/UI and compatible game client/server modules remain unported.
