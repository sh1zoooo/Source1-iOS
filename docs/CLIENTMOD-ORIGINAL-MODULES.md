# Original ClientMod modules: compilation checkpoint

The project now pins actual ClientMod source instead of expanding the UIKit
replacement HUD or menu. `SOURCE_BUILD_CLIENTMOD=ON` builds the selected original
client and GUI modules. This is a compilation checkpoint, **not an active graphical
client in the IPA**. No new application version or full-port percentage is claimed.

## Provenance and version limits

- Source: https://github.com/jeffcwj/clientmod-android
- Pinned commit: `5f8bea18c72467601d7919ba86d57b06e5e2d677`.
- Upstream README identifies an Android fork with modifications for a Brazilian
  server by pitF. Exact equivalence to the imported Rec 1.4 distribution is not
  established. Keeping its revision explicit makes that distinction reviewable.
- The fork's Android workflow references the launcher template
  https://github.com/DMK95/Srceng-mod-launcher. Inspected revision:
  `c9f22315b4787ac782d4a354ea8ee9a27e1ad306`. Its Java launcher passes `argv`,
  `gamedir`, `gamelibdir` and its extracted `vpk` to the engine. It contains asset
  extraction and update services. Java Activities are not iOS game/HUD code.

`scripts/clientmod_manifest.py` evaluates the pinned Waf/VPC source selections for
CS:S and checks every translation unit exists. The checked-in JSON must match.

`clientmod-source-comparison.json` records the complete file-level comparison
against the pinned SDK for nine relevant modules. The public fork changes 21
client files and adds four GameUI ModMenu source/header files. Its shader C++
implementations match the SDK; the material-system difference is `CColorCorrection.cpp`.
That confirms the graphics work requires activating the original rendering path,
not simply copying a different set of shader source files.

| Archive | Selected objects on native build | Original content |
| --- | ---: | --- |
| clientmod_client | 574 | CS:S client prediction, viewmodels, weapon animation, touch, HUD, radar, buy/team/spectator menus |
| clientmod_GameUI | 77 | BasePanel, settings, multiplayer/map creation dialogs, touch options, ModMenu settings pages |
| clientmod_vgui_controls | 77 | Original panels, labels, buttons, layout, property dialogs |
| clientmod_matsys_controls | 21 | Material/model/animation panels |
| clientmod_vgui2 | 20 | Original VGUI panel, input, localization, scheme and system implementations |
| clientmod_vgui_surfacelib | 5 | Original font manager, bitmap fonts and FreeType font rendering |
| clientmod_vguimatsurface | 10 | Original material-backed VGUI drawing, texture dictionary, input and font atlas |
| clientmod_stdshader_dx9 | 104 | Original world, viewmodel, sky, water, refraction, blending and effect shader implementations |

Total: 888 source objects. iOS adds one UIKit platform-service object.
`memoverride.cpp` is excluded where upstream lists it because the monolithic
host already owns allocation. GameUI additionally compiles upstream
`ModMenu/ClientModMainMenu.cpp` and `ClientModMenuWindow.cpp`, omitted from the
fork's Waf list. They are **viewmodel/settings pages**, not proof that the entire
Rec 1.4 main menu has been recovered or activated. The fork has no call site
opening `ClientModMenuWindow` in its BasePanel.

## Adaptations

Original files remain unchanged in the submodule. A checked preparation script
copies them into the build directory and applies explicit compatibility changes:

1. The current SDK touch ABI sends event type, finger ID and floating-point
   coordinates separately. The pinned SDK's original adapter dispatches those
   into ClientMod's original `gTouch.ProcessEvent` rather than decoding the old
   fork's packed 16-bit event. The future engine/input pair must use this ABI.
2. The invalid Xbox game-rules token-paste branch uses the existing SDK port fix.
3. Steam achievement callback definitions follow their existing `NO_STEAM`
   declaration guard. Local achievement code remains selected.
4. VGUI uses its existing SDL clipboard code on iOS instead of Carbon.
   Opening URLs uses `UIApplication` on the main queue instead of desktop `fork`.
   The original file dialog's Open in Explorer action opens an iOS document
   browser at the requested directory, rather than invoking macOS `system`.
5. An absent, unused `ClientModInfo.h` include is removed. No invented replacement
   class or behavior is supplied.
6. ClientMod's original `IMatSystemSurface` extension, `DrawFilledPolygon`, is
   retained for its radar and paired surface implementation. Other public headers
   use the already patched SDK. This is not a claim of complete binary ABI parity.

`USE_SDL` matches the original graphical configuration. These archives must not
be linked casually into the current non-SDL dedicated host: graphical factory
ownership and interface layouts still require integration.

## Checks and remaining runtime work

```
cmake -S . -B build-clientmod -DSOURCE_BUILD_CLIENTMOD=ON
cmake --build build-clientmod --target clientmod_compile_check --parallel 4
python3 scripts/verify_clientmod_archives.py build-clientmod
```

Native compilation and object coverage passed for all eight archives. The first
iPhone ARM64 build identified the unsupported desktop Open in Explorer call;
the platform adaptation is included and needs repeat ARM64 validation. The VGUI
surface's unused macOS Carbon include is also excluded from the iOS build.

The native shader-registry test links the original library separately from the
dedicated engine and checks `ShaderDLL004`, shader registration and the original
world/viewmodel/sky/water/refraction families. The check passed with 127 original
shader registrations. It does not draw on a GPU. Keeping
this test separate also avoids merging globals owned by different upstream DLLs,
such as the engine and shader DLL's `mat_fullbright`.

The verifier checks every source object by filename and multiplicity, not just
archive existence or size. The separate ClientMod workflow repeats this on Linux
and builds/verifies the archives for iPhone ARM64. Its downloadable artifact is
labelled **compilation**, not IPA or playable ClientMod.

The current IPA still uses the dedicated CS:S host, custom Metal preview and
replacement UIKit UI. To activate the original modules, remaining work includes
the non-dedicated graphical engine and client loop, real shader/material backend,
SDL iOS window/input integration, activating the original VGUI surface/fonts, module
factories, audio and runtime dependency wiring. Device checks must then cover
actual imported maps, translucency, viewmodel animation, grenade effects,
dropped-weapon collision and frame times. Archive coverage cannot validate those
behaviors or prove support for every map.
