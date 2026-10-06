# 0.21.0 / build 24 — loose content directories

Adds `source_content_mount`/`source_content_unmount` for unpacked resource folders
under Source1IOS/content. Safe relative names, canonical containment, symlink
rejection, depth/entry budgets, mount count and legacy auto-ZIP rejection are
checked before Source AddSearchPath. Mounts append to GAME and preserve the
app's existing default write path. Cleanup removes mounts on shutdown.

Startup detects the first available game folder (`cm`, `cstrike_clientmod`,
`cstrike`) and shared `hl2`/`platform`. This only adds asset search paths; no
imported gameinfo/configs or Android binary modules are executed. See
[content instructions](CONTENT-IMPORT.md) for archive and format limitations.

Local Debug contracts passed: load BSP and MDL/VVD/VTX with a distinct VMT/VTF
from a synthetic cm tree, duplicate mount, unmount, missing/traversal paths,
symlink roots/children, legacy ZIP rejection, recovery and auto-mount after
restart. This does not validate the full ClientMod archive.

Simulator smoke creates a known BSP in content/smoke, mounts it, loads it through
Source filesystem and unmounts it after geometry is copied. It requires 155
PASS, the content log markers and map revision 5 completion on GPU together
with external ANI and the 17-material grid. Physical-phone verification remains
at 0.17; the new content path is verified automatically below.

## Verified artifact

Commit `1131877ad0cfbcc1ce6982f223a382e2364fe529`,
[run 37415338772](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37415338772)
passed Linux Debug runtime/mutation contracts, ARM64 device build and simulator
smoke. Artifact 11390079833 was downloaded independently: IPA version 0.21.0,
build 24; 155 PASS, no FAIL; mount/load/unmount of content/smoke; completed GPU
map revision 5; external ANI and pause/resume. The screenshot shows textured
brushes, floor, the posed striped model and physics probes. This is synthetic
content in the simulator, not a full ClientMod cache or physical-device test.
IPA SHA256 matches the manifest:
`e23e181e3fd271e3b4c3a82f89810a07bdb2fd4881a3c60a043ab3c915057e58`.

## Physical iPhone observation

The user's 2026-10-06 log and two screenshots show 77 PASS on iPhone 16e/A18,
iOS 18.6.2, base/model/LDR lightmap uploads, GPU revision 2, pause/resume,
orientation resize and different embedded model poses. They do not exercise
external ANI or loose content import on the phone.
