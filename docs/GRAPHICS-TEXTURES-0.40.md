# Graphics texture fidelity — 0.40

Graphics is now the next development priority. This increment fixes texture detail lost in the Metal adapter before shader evaluation.

The previous BSP and multi-material studio atlases always resampled imported textures to 64×64 pixels per material. Adaptive atlas tiles now preserve up to 512×512, retaining the existing material indices, UV convention, skin remapping and animation geometry. Low-resolution fixtures remain 64×64. Single-material studio textures already retain their decoded resolution and are unchanged.

Atlas growth repacks earlier materials without changing their slots. The maximum texture dimension is 8192, there are at most 512 materials, and the atlas budget is 64 MiB. If growth would exceed the budget, all tiles shrink together. Source VTF inputs remain bounded to 2048×2048, and alpha bytes survive atlas resampling. This does not enable transparent blending or alpha testing in the renderer.

Removed the diagnostic face-color palette from surfaces without lightmaps; the fallback now uses neutral directional shading. Existing baked LDR/HDR lightmaps remain active.

## Validation

- 8/8 native tests pass, including a new atlas test for one-pixel detail, alpha, material preservation, second-row packing, malformed image rejection and memory-budget shrinkage.
- On the user's actual ClientMod cache, the AWP atlas is now 4608×512 (9 MiB), compared with 576×64 (144 KiB): 64 times as many texels per tile. `mobile_probe` completed zoom, purchases, weapon slots, drop and team respawns with the new textures.
- The first standalone cache invocation returned zero before the probe completion marker. A subsequent invocation completed `MOBILE PASS`; completion markers, not exit status alone, are required for manual cache checks.
- iOS device compilation and simulator GPU/runtime checks are pending publication of this increment. No physical-iPhone or before/after screenshot comparison is claimed.

## Next graphics work

This is not the full original Source graphical material system. Next work requires transporting VMT shader parameters, normal maps, model lighting/Phong and environment reflections into a Metal material path, followed by alpha testing/transparency, sky rendering and material-specific effects. The ClientMod cache supplies assets; it does not contain the modified client/shader source code. No full-port percentage is claimed.
