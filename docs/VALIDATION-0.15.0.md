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
VTF resource dictionary ranges and aggregate auxiliary allocation budget are
validated before invoking the legacy decoder (which otherwise allocates chunks
before validating their payload). A forged INT_MAX chunk falls back safely and
restoring the texture recovers the original pixels.

Local Debug runtime passed with material resolution, fallback on traversal paths,
pixel restoration, invalid model preservation, weighted pose and animation tests.
An additional test checks raw Quaternion64/Vector48 animation against known pose.
67 startup checks; simulator produced 135 PASS plus GPU texture upload.
An additional local AddressSanitizer probe instrumented SourceStudio.cpp and ran
3000 deterministic fixture mutations (seed 0x510510, rotating MDL/VVD/VTX files,
truncation every fifth case, otherwise 1–4 random bit flips): 1283 accepted,
1717 rejected, no ASan bounds errors. Accepted models were sampled and skinned
with finite output checks. Upstream static libraries were not instrumented;
LeakSanitizer was disabled because this sandbox lacks /proc task enumeration.
This is a limited mutation probe, not exhaustive fuzzing or leak validation.
A separate attempt to instrument the whole local integration runtime terminated
before its startup log, with nested ASan DEADLYSIGNAL reporting and no usable
stack; removing the probe's custom signal handler did not resolve it. Its cause
is not established and it is not counted as a passed sanitizer run. Ordinary
Debug runtime checks passed independently.

Boundaries: only first material slot, no shader-feature parity, normal/specular/
env maps, alpha, multi-material batches, skins, Patch/Proxies or BSP materials.
Full animation sequences, external ANI and IK remain pending.

## Verified Actions artifact

Commit: `be94943014ff7ab4a096d9f12342a48928edb8e2`.
[Run 37349031337](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37349031337)
passed Linux Debug runtime contracts, iPhone ARM64 build, simulator build and
startup smoke. Downloaded artifact 11362138591 was checked independently:
135 PASS, no FAIL, studio texture uploaded to Metal, completed GPU frame,
Host paused/resumed. Screenshot shows the striped model with animated pose and
displacement terrain; screenshot alone does not demonstrate smooth motion.
IPA plist confirms 0.15.0/build 18, iPhoneOS and arm64. SHA256 matches manifest:
`6c14ea79351f2100b446d2b53bc2f37337a0583dd5d12180c290937dc9d94910`.
Physical-phone confirmation applies to 0.13 only, not this artifact.
