# 0.12.0 / build 15 — static Source studio geometry

## Implemented

- Safe MDL v49 + VVD v4 + DX90 VTX v7 companion loading with matching checksum.
- Root LOD 0 vertex reconstruction, including VVD fixup tables.
- VTX triangle lists, triangle strips and standard/v49 extended records.
- Original MDLCache header lookup plus a separate bounded geometry parser.
- Static model triangles rendered by the existing Metal preview adapter.
- Console commands `source_model_load models/name.mdl` and `source_model_reset`.

## Local evidence

Linux Debug runtime contracts exit 0. Sixty startup checks include the generated
12-triangle studio fixture, fixup/triangle-strip expansion, malformed companion
rejection, MDLCache lookup and staged Metal geometry. Runtime tests reject a
checksum-mismatched model without replacing the currently visible geometry.

## Boundaries

Only static LOD 0 geometry is used. There is no bone pose, weighted skinning,
animation, flex, material selection, VMT/VTF binding, PHY collision or original
graphical studiorender. A compatible imported model is placed at a fixed point
inside the test room. Version 48 and non-DX90 companion variants are rejected.

GitHub Actions and physical iPhone validation: pending.
