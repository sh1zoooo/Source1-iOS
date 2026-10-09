# Original dynamic module integration

2026-10-09: native Linux strict linking passed for the original graphical
engine, client, GameUI, VGUI2, VGUI surface, materialsystem, shader API,
ToGLES, launcher, input, video services, standard shaders, filesystem, datacache,
studio renderer, vphysics, original server, sound emitter and scene cache. Each DLL has its own
interface registry; tier0/vstdlib and SDL are shared dependencies. Original
appframework, datamodel, sound/model/render helpers and FreeType are included.

Nineteen original interface queries passed with RTLD_NOW, including VClient017,
VEngineClient014, GameUI011, VGUI_Surface030, VMaterialSystem081 and ShaderDLL004.
Unknown-interface rejection passed. The 64-bit SDL contact slot test also passed.
These checks establish linking and factory availability, not a rendered game.

The native `clientmod_module_lifecycle` test also connects and initializes the
original cvar, filesystem, input, physics, sound emitter and scene-cache systems,
creates/simulates/destroys an original physics environment and shuts systems down
in reverse order. SDL events with a 64-bit finger ID reach the original input
module; after shutdown its event watcher is removed. This does not test graphical
initialization, client/server gameplay or sound playback.

Platform adaptations preserve original drawing, shader and UI implementations.
They address the SDK touch ABI, missing upstream settings handler, misplaced
NO_STEAM guard around a virtual method, private bzip2 inline linkage and SDL
platform integration. Steam offline ABI glue provides no identity, callbacks,
authentication or networking. Dedicated-preview-only material/model edits are
excluded from the graphical modules.

The optional strict profile is SOURCE_BUILD_CLIENTMOD_RUNTIME=ON, together with
SOURCE_BUILD_CLIENTMOD_GRAPHICS, SOURCE_BUILD_CLIENTMOD and SOURCE_BUILD_CSTRIKE.
The `clientmod_runtime_link_check` target links all graphical DLLs without
unresolved symbols. Factory tests are named `clientmod_*_factory`.

The existing IPA still launches the dedicated CS:S host plus Metal preview.
An optional `SOURCE_CLIENTMOD_IOS_APP=ON` application now enters the original
`LauncherMain` through SDL's UIKit entry point. It bundles the separate modules
and sets their lookup directory independently of `Documents/Source1IOS/content`.
It does not link the preview runtime. Original menus/HUD/game loop are owned by
the original modules. This path remains under ARM64 build verification and has
not been demonstrated on a device; it is not enabled in the released IPA workflow.
The content root must already contain the imported `cm`, `cstrike`, `hl2` and
`platform` directories. Its initial launch uses `-noip` for offline verification.

The eight-module original compilation workflow passed for ARM64 at commit
88b919909e1d647fae877047bf5a658f0fe2861a. The larger graphical archive profile also
passed ARM64 CI at 35b51cb4283c58ff0dce5eaf2d2d474dcdaf7dfb after fixing video-service
platform selection and SDL launcher declarations. The full dynamic iOS application
has its own CI job and writes startup diagnostics to `Source1IOS/ClientMod-native.log`.
Full graphical/client/server lifecycle,
content compatibility and sound codecs remain unverified. No on-device original
shader/HUD or server connection has been demonstrated. No application version bump
accompanies these checks.
