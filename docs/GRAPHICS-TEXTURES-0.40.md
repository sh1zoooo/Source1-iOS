# Graphics texture fidelity — 0.40

Graphics is now the next development priority. This increment fixes texture detail lost in the Metal adapter before shader evaluation.

The previous BSP and multi-material studio atlases always resampled imported textures to 64×64 pixels per material. Adaptive atlas tiles now preserve up to 512×512, retaining the existing material indices, UV convention, skin remapping and animation geometry. Low-resolution fixtures remain 64×64. Single-material studio textures already retain their decoded resolution and are unchanged.

Atlas growth repacks earlier materials without changing their slots. The maximum texture dimension is 8192, there are at most 512 materials, and the atlas budget is 64 MiB. If growth would exceed the budget, all tiles shrink together. Source VTF inputs remain bounded to 2048×2048, and alpha bytes survive atlas resampling. This does not enable transparent blending or alpha testing in the renderer.

Removed the diagnostic face-color palette from surfaces without lightmaps; the fallback now uses neutral directional shading. Existing baked LDR/HDR lightmaps remain active.

## Validation

- 8/8 native tests pass, including a new atlas test for one-pixel detail, alpha, material preservation, second-row packing, malformed image rejection and memory-budget shrinkage.
- On the user's actual ClientMod cache, the AWP atlas is now 4608×512 (9 MiB), compared with 576×64 (144 KiB): 64 times as many texels per tile. `mobile_probe` completed zoom, purchases, weapon slots, drop and team respawns with the new textures.
- Of four ordinary cache invocations, two returned zero before the completion marker and two completed `MOBILE PASS`. Three additional invocations with exit diagnostics completed successfully without reproducing the early exit. Its cause remains unresolved; completion markers, not exit status alone, are required for manual cache checks. This is an experimental build.
- [Workflow 37772021643](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37772021643) passed native CI (8/8 tests), device ARM64 compilation, archive verification and simulator compilation for code commit `5e82391bfb2b62d33303d499e2e5c27107639781` (PR merge build `f79eba40f9b2aca382b14415c150283d94c00b0f`). Simulator verification failed before app launch: `simctl install` timed out after 120 seconds. No Metal startup, physical-iPhone or before/after screenshot comparison is claimed for 0.40.

[Download the experimental 0.40 / build 44 IPA artifact](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37772021643/artifacts/11549485241). Extract `Source1IOS-unsigned.ipa`; the separate `CStrike-ARM64-compile` artifact is not an app. Artifact ZIP: 5,304,587 bytes, SHA256 `0169dbe830da9d30a0389bcba235c2d139b91c304487b5125a8c6e49218a89be`.

## ClientMod material inputs confirmed

CRC-verified VMT entries in the supplied `Skinpack` cache confirm that AWP requests Phong shading, an exponent texture and `env_cubemap` reflections. The glove material requests a normal map, exponent texture, Phong/Fresnel tint and rim lighting; the same glove definition also appears in `cm_resources`. These inputs are present in the cache but the current renderer only reads the base texture. The next shader work should read the imported parameters rather than hard-code cosmetic highlights.

## Next graphics work

This is not the full original Source graphical material system. Next work requires transporting VMT shader parameters, normal maps, model lighting/Phong and environment reflections into a Metal material path, followed by alpha testing/transparency, sky rendering and material-specific effects. The ClientMod cache supplies assets; it does not contain the modified client/shader source code. No full-port percentage is claimed.
