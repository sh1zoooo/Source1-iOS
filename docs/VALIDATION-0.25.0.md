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

Local original-engine integration and ASan mutation checks passed. iPhone ARM64,
simulator GPU and independent downloaded artifact verification are pending.

Reproduce artifact checks with `python3 scripts/verify_artifact.py <artifact.zip>
--version 0.25.0 --build 28 --passes 181 --revision 6 --require-static-props`.
