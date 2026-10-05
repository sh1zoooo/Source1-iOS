# 0.10.0 / build 13 — BSP compatibility

Code: `c0b8b0274ce713ca97dbe79a9103200e6a50ac5d`.

## Scope

Original Source `CLZMAStream` now decodes imported geometry lumps with bounded
input/output and a dictionary cap. Only vertex, edge, surfedge and face lumps are
decoded; unused lumps remain ignored by this polygon preview. Raw geometry still
uses `CMapLoadHelper`. Faces accept versions 0 and original `LUMP_FACES_VERSION=1`.
This is not graphical materialsystem startup, a game server, or CS:S gameplay.

The independent test stream was encoded using Python stdlib `lzma`, raw LZMA1,
lc=3/lp=0/pb=2, dictionary=1 MiB. It contains the fixture's 672 vertex bytes in a
141-byte Source-header stream. The test compares every decoded byte against the
original fixture, rather than merely checking decoder success.

## Native checks

Local Linux probe linked real Source archives, including Debug IVP. Runtime
contracts exited 0 with no `: FAIL` or assertion failure. Checks include:

- Compressed map (faces version 1) imports and matches the uncompressed mesh and
  initial physics pose exactly.
- Damaged LZMA payload is rejected; the existing scene is unchanged.
- Truncated headers, mismatched sizes, invalid properties, oversized dictionaries
  and oversized decoded output are rejected before an unsafe allocation/read.
- Compression metadata in an unused lump does not prevent polygon preview.
- Original physics, lifecycle, camera collision, host shutdown/restart contracts
  remain active.

GitHub run 37277353747: full Linux Debug runtime contracts passed; iOS ARM64
app built and packaged; simulator startup, GPU completion, 105 PASS entries
(two sets of 52 checks + runtime check), and background/resume passed.
Artifact: 11330739486.

Additional native soak: three restarts with 10,000 frames each exited 0, but
reported 99 IVP `recalc_mindist: Endless Loop` warnings. Maximum RSS increased
17,332 → 18,100 → 18,868 KiB; this is not proof of leak-free restarts. Contact
solver rescue warnings remain an open physics defect, not a validated ragdoll.

## Device check requested

On iPhone, install build 13 and verify startup's 52 PASS checks and GPU completion.
Enter `source_selftest`, press Go, verify keyboard dismissal and another 52 PASS.
Load an actual CS:S `.bsp` using `source_bsp_load maps/<actual filename>.bsp`.
If rejected, export the new log; it reports filename and header/compression reason.
The actual user dust file has not been supplied or validated. `.bpz`, ZIP and RAR
are not accepted as BSP. A successful import is geometry preview with checker
texture and polygon collision, not a running CS:S map/match.

Physical build-13 validation: pending user's device test.
