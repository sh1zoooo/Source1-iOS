# 0.24.0 / build 27 — BSP static props

The preview reads the uncompressed `sprp` game lump, resolves a bounded model
dictionary and transforms MDL/VVD/DX90.VTX bind geometry with original Source
AngleMatrix/VectorTransform. Repeated instances share decoded geometry and an
atlas slot. Model textures use the existing bounded VMT/Patch/VTF resolver.
Version 4–11 records are recognized; version 10 uses the BSP 21 stride and
version 11 includes uniform scale. The fixture uses version 4, two instances
and one model type. Normal app startup selects this fixture; the simulator
exercises the baseline checks, terrain, material grid, content import and then
the prop scene with external ANI animation still active.

Budgets: 4 MiB prop payload, 1,024 dictionary entries, 65,536 leaf references,
8,192 instances, 128 loaded model types, 64 MiB aggregate companion input,
300,000 combined world/prop vertices and 512 atlas slots. Invalid metadata or
transforms reject the new map without replacing the active scene. Missing or
unsupported models and nonzero skins are skipped with diagnostics.

This is visual geometry only: no original PHY collision, prop fading, vertex
lighting, multiple model materials/skins, alpha effects or full engine static
prop manager. Game-lump compression is not yet supported. The original Source
graphical shader API and CS:S game DLL are still pending. The approximate 74%
label refers only to the minimal demonstration milestone, not a playable game.

Local verification: original-engine native runtime integration passed, including
two rendered instances, atlas slot assignment, map reset and malformed imported
game-lump offsets/version/model indices preserving the previous scene. Four
additional startup checks cover dictionaries, versions, rejected ranges and atlas
preservation. ASan/UBSan passed 10,000 deterministic payload mutations with
transactional output checks (leak detection disabled in this sandbox).

GitHub Actions iPhone ARM64, simulator GPU and artifact verification: pending.
