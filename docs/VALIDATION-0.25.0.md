# 0.25.0 / build 28 — MDL48/49 preview and visible prop fixture

The bounded studio preview now accepts versions 48 and 49. Serialized layouts
were compared against Valve's [Source SDK 2013 studio.h](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/public/studio.h)
and the pinned upstream's Studio_ConvertStudioHdrToNewVersion compatibility
path. Compile-time checks require disk sizes: header 408, bone 216, model 148,
mesh 116, animation description 100 and vertex 48 bytes. Earlier versions and
newer unknown versions remain rejected. No generic conversion routine is called
on unvalidated input; all existing count, range, checksum and pose limits remain.

Generated version-48 fixtures retain the same validated geometry and simple
embedded/external ANI tracks. Normal app startup shows the embedded MDL48
model. Simulator smoke loads and animates the external MDL48 model after the
static-prop map. Three new startup checks exercise embedded/external clips and
unknown-version rejection. ASan passed 12,000 deterministic mutations across
both MDL versions and embedded/external tracks; leak detection disabled.

The 0.24 portrait screenshot exposed a fixture-placement issue: both static
objects were off-view or occluded. The fixture now places them near the camera;
native runtime checks that both projected centroids are inside a 0.46 aspect
viewport. This is a demo placement fix, not a change to map-supplied transforms.

This does not establish full compatibility with actual CS:S/ClientMod assets:
real assets have not been supplied for testing. Materials remain first-slot,
skins/IK/delta/blend/sectioned sequences and original graphical shader API are
still incomplete. MDL48 static props use the same bounded parser. Approximate
76% applies to the minimal demonstration only; CS:S is not playable yet.

Local original-engine integration and ASan mutation checks passed. Actions
[37425496670](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37425496670),
functional commit `ebf18f010e6b60aa9612bd0c5a396f525263093b`, passed all four
Linux CTest sets and iPhone ARM64/simulator builds. Attempt 2 passed simulator
GPU and lifecycle verification: 181 PASS, no FAIL, static props, MDL48 external
ANI playback, GPU map revision 6 and pause/resume.

Downloaded successful artifact **11395675357** was independently inspected:
version 0.25.0/build 28, ARM64 Mach-O executable, SHA256
`9eba817a1569f540ad7d0e67dc3f14d75b596937caccd1c1c572a65478e811bc`.
The portrait screenshot visibly contains both striped static objects in the
foreground and the animated studio model behind them. No physical iPhone test
of this version is claimed; simulator output does not substitute for one.

Actions run 37425496670, attempt 1: four Linux CTest sets and both iOS builds
passed. `simctl launch` timed out after 120 seconds before the script resolved
the app container; no runtime log or screenshot was captured. This is not GPU
validation and does not establish whether the application started. The failed
iOS job passed on a fresh runner in attempt 2. A following diagnostics-only
change resolves the log path before launch; `tests/smoke_launch_diagnostics.py`
passed locally with mocked simctl to verify log preservation without concealing
the timeout. This separate script test is not engine/GPU validation and the
diagnostic-order change was not in the functional commit tested by attempt 2.

Reproduce artifact checks with `python3 scripts/verify_artifact.py <artifact.zip>
--version 0.25.0 --build 28 --passes 181 --revision 6 --require-static-props --require-mdl48`.
