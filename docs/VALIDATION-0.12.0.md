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

## GitHub Actions evidence

Run `37307565789`, code `74bc248d7b3a5da3dcb860f4998c57e6e156df59`:
Linux Debug runtime contracts, iPhone ARM64 build and simulator build all passed.
The simulator log contains exactly 121 PASS entries (two 60-check sets plus the
runtime contract), no FAIL, completed Metal GPU frame, terrain load and pause/resume.
The screenshot was visually inspected: the enlarged green studio fixture is fully
visible in the checker-textured room beside both live physics bodies.

Artifact `11344089914` was downloaded and inspected. Info.plist is 0.12.0/build 15;
the IPA SHA256 agrees with `SHA256SUMS.txt`:
`92dcc389ac30a8bd1018f657533160d5da4cb0c843f56de10690ba8bda9aa3ab`.

Physical iPhone validation: user log confirms 60 startup checks, A18 GPU frame
completion and portrait-to-landscape drawable change on iOS 18.6.2. The supplied
landscape screenshot shows the studio fixture and both physics bodies. This log
does not include a foreground/background cycle.
