# 0.13.0 / build 16 — weighted studio pose

MDL bones provide parent indices, local bind quaternion/position and poseToBone.
VVD influences provide up to three bone indices and normalized positive weights.
The loader rejects out-of-range weights, nonfinite values and invalid parent
ordering before skinning. Source mathlib composes local-to-world bone transforms
and inverse bind matrices; CPU weighted vertex/normal skinning drives Metal geometry.

The two-bone fixture bends its upper vertices while root vertices remain fixed.
Imported models retain their default bind pose. Fixture movement is procedural,
not decoded from animation sequences. Ragdoll physics is not yet driving bones.

Checks cover inverse-bind identity, a 90-degree child rotation with an unchanged
root, a 50/50 two-bone blend, invalid bones/weights, and live finite pose changes.
63 startup checks; simulator expects 127 PASS after repeat tests/runtime contracts.

Linux Debug runtime contracts passed locally and in GitHub Actions. Run
`37331218202`, code `b9b566993d099c80e31a74a5b730649e44097724`: iPhone ARM64 and
simulator builds passed. Downloaded simulator log has 127 PASS, zero FAIL,
completed GPU frame and pause/resume. The screenshot was inspected and shows
the green model deformed from its original bind shape.

Downloaded artifact `11354283413` contains IPA 0.13.0/build 16. Its SHA256 matches
the included checksum:
`fa02d49e4f85df77acb0eaac26a114b26ff2ebe110051452b21ffbcee7269547`.

Physical iPhone validation of 0.13 is pending. Version 0.12 static geometry has
been confirmed by the user's A18/iOS 18.6.2 log and landscape screenshot.
