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

GitHub Actions and physical-device validation pending.
