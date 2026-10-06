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
at 0.17; the new content path is being checked automatically.
