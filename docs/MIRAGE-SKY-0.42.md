# Mirage and skybox 0.42

- Enable `de_mirage_go` in the practice menu and game-start gate. Resources must already be installed in the ClientMod cache.
- Increase the combined world/static-prop vertex ceiling from 1.5 million to 3 million. This addresses the previously observed Mirage prop-instance budget rejection; it can increase memory use and does not establish stable gameplay on this map.
- Read a bounded, validated `worldspawn.skyname` from BSP entities. Unsafe names reject atomically.
- Load the six skybox material/VTF faces using Source's MakeSkyVec orientation and face order. Render the direction cube at far depth, without translation, world lightmaps or collision geometry.
- Missing skybox faces use a light blue gradient. The Metal clear background is also light blue for maps without a usable skyname. SKY/NODRAW tool surfaces remain hidden.

Local native build and 8/8 CTest checks passed, including skyname path validation, skybox geometry/material slots/far depth, invariance under camera translation and clearing the sky when changing maps. [Workflow 37789412555](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37789412555) also passed native CI, ARM64 iPhone compilation, simulator compilation and simulator startup/runtime/Metal GPU checks for code commit `246a6b11afcc9e9245bfe2516941f45082cf99b7` (PR merge build `2127ca4a170c39e54c75e0638793aa99caa087ea`). The simulator smoke used its built-in scenes; the six-face sky fixture was verified by native tests, not a physical-device screenshot.

The complete ClientMod cache is absent from this workspace: loading, movement and restart on the actual `de_mirage_go` BSP have not been re-tested. This is experimental Mirage enablement with the known geometry limit addressed, not a claim of map compatibility verification. This change implements the 2D six-face skybox, not Source's additional 3D sky scene, fog or HDR sky shaders.

[Download 0.42 / build 46 IPA](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37789412555/artifacts/11557085103). Extract `Source1IOS-unsigned.ipa` from the Source1IOS artifact. Artifact ZIP: 6,265,499 bytes; SHA256 `63ee0fa08029e4f38cfa30a9498b343d40cf11e4bf6c293d3408516e4dd92910`.
