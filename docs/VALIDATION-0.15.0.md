# 0.15.0 / build 18 — model VMT/VTF base texture

Adds the first MDL texture-slot name and CD texture-directory candidates. Safe
relative paths resolve a VMT via Source filesystem and original KeyValues;
VertexLitGeneric/UnlitGeneric `$basetexture` resolves a VTF. Original VTF header
and payload decoding precede RGBA upload. The Metal fragment chooses map/model
slots via a flat vertex tag. Texture upload happens only when revision changes.

Generated fixture texture has cyan/orange stripes. Map retains its checker.
Missing or rejected optional materials fall back to checker. Model parse failure
preserves existing geometry, pixels and upload revision. Reads are bounded before
file allocation: model companions 32 MiB, VMT 64 KiB, VTF 16 MiB. Texture dimension
limit is 2048x2048, one frame, face and depth. KeyValues nesting is limited to 16.

Local Debug runtime passed with material resolution, fallback on traversal paths,
pixel restoration, invalid model preservation, weighted pose and animation tests.
An additional test checks raw Quaternion64/Vector48 animation against known pose.
67 startup checks expected; simulator expects 135 PASS plus GPU texture upload.

Boundaries: only first material slot, no shader-feature parity, normal/specular/
env maps, alpha, multi-material batches, skins, Patch/Proxies or BSP materials.
Full animation sequences, external ANI and IK remain pending. Actions pending.
