# 0.34.0 / build 38 — Source bodygroup selection

The confirmed baseline is 0.33.1 (`150735b`): workflow
[37575059340](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37575059340)
passed Linux 5/5 and ARM64 device/simulator builds, startup/GPU/lifecycle checks.
The milestone estimate remains ~92% of the minimal demonstration, not full CS:S.

## Behavior

- Each MDL bodypart selects `(body / base) % nummodels`, matching the pinned
  upstream `studiorender/r_studio.cpp` implementation. Default body zero selects
  one alternative per bodypart instead of drawing their union.
- All alternatives are bounds-checked and share the existing total 65,536 triangle
  budget. Empty alternatives are valid when the whole model contains geometry.
  Invalid multi-model selection bases, companion ranges and counts are rejected.
- Selection retains skin family, material references, bones and animation clips.
  BSP `prop_dynamic`/`prop_dynamic_override` honor integer `body` in 0–65535.
  Their preview remains bind pose and visual only, without game DLL/entity I/O.
- Per-map cache counts every alternative against its geometry budget and retains
  shared materials. Three instances of one fixture test default/alternate/blank
  bodies together with skin remapping and physics reset.
- Native startup has four additional contracts for default versus union,
  blank/alternate skins and animation, atomic rejection, and base/modulo packing.
  Simulator smoke requires 256 PASS (2×118 startup +20 live checks), revision 10,
  existing texture/GPU/lifecycle evidence and both live entity contracts.

## Validation

Linux Debug build and all five CTest suites passed locally (runtime, entity,
static prop, PHY and studio bounds contracts). Studio mutations now
cover 72,000 inputs across MDL44/48/49, embedded/external animation,
multiple materials and body alternatives. Entity mutations cover 30,000 inputs
including nonzero bodies and malformed selections. This is bounded mutation
coverage, not exhaustive fuzzing or proof of leak freedom.

The native runtime fixture retains its original total path length when TMPDIR is
longer than `/tmp`; this avoids exceeding upstream legacy MAX_PATH buffers merely
because the sandbox supplies a longer writable temporary directory.

## Verified CI and artifact

Implementation commit: `0864f9e21584619d1c48d8e8cc193924cf9ab7eb`.
[Workflow 37576240688](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37576240688)
passed Linux 5/5, iPhone ARM64 and simulator builds plus simulator startup.
The PR workflow builds merge ref `0d96ea13b2291b6e2f18051bc96c118865154e71`;
artifact ID is `11462528197` (3,211,833 bytes).

The downloaded archive passed `scripts/verify_artifact.py`: version 0.34.0,
build 38, bundle ID, 256 PASS, GPU map revision 10, pause/resume, all existing
PHY/HDR/MDL44/48/material/skin/VPK/entity markers and PNG dimensions 1170×2532.
IPA SHA256 matches the included manifest:
`55aa0f0d15e02a54da6ef79464a908f4097d14a1e19636d194ae20470d61045e`.
All four bodygroup contracts pass in both startup/selftest sets; three entity
instances stage from three candidates, including the blank body.

The screenshot was inspected: textured room, physics spheres and studio/static
models are visible; the status shows bodygroups and 118 startup PASS. The final
screenshot shows the existing skin/HDR/PHY scene, not the temporary body fixture.
No new physical iPhone run or real ClientMod model with body alternatives has
been verified in this stage.
