# Material and mobile resource fixes 0.41

This increment addresses failures visible in the user's iPhone screenshots. It is not a complete ClientMod graphics/gameplay port.

- iOS now executes the original filesystem's case-insensitive fallback for open/stat/chmod. Previously that fallback was compiled only for Linux/BSD, so the mixed-case directory resolver added in 0.39 could never run on iOS. Startup search-path self-tests now read an actual mixed-case folder through a lowercase Source request on all platforms.
- 4096-pixel VTF inputs can use their supplied mip levels to decode at most 2048 pixels. File/resource bounds remain enforced. No 4K atlas allocation is introduced.
- Additional terrain/material shader families can resolve their base texture. This does not implement their normal mapping, blending, reflection or specular calculations.
- SKY/SKY2D/NODRAW/HINT/SKIP surfaces are excluded from the visible BSP mesh. Tool sky textures no longer render as tiled blue walls. A real skybox pass is still pending; the clear background is visible instead.
- Native HUD labels shrink to fit their imported bounds instead of truncating health/ammo values. This does not yet replace the native labels with the complete ClientMod HUD or solve every overlap with touch controls.
- Missing world materials now log their material name; unsupported shader names also appear in diagnostics.

Validation: native build and final 8/8 CTest checks passed, including the cross-platform startup case test. Apple build and simulator checks are pending. Tests cover terrain albedo, a mipmapped 4K VTF, filtering all three tested tool-surface flags and restoring normal map geometry. No physical-device visual comparison is claimed.

The supplied cache is not present in this restored local workspace, so the specific floor/AK materials shown in the screenshots have not been identified or re-tested against the archive. These changes remove verified loader defects; they do not establish that every checkerboard is fixed. Arbitrary-map compatibility remains unfinished, and the existing two-map practice allowlist is unchanged.
