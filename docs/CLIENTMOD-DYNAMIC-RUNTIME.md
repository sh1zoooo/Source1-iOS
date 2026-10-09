# Original dynamic module integration

2026-10-09: native Linux strict linking passed for the original graphical
engine, client, GameUI, VGUI2, VGUI surface, materialsystem, shader API,
ToGLES, launcher, input, video services, standard shaders, filesystem, datacache,
studio renderer and vphysics. Each DLL has its own
interface registry; tier0/vstdlib and SDL are shared dependencies. Original
appframework, datamodel, sound/model/render helpers and FreeType are included.

Sixteen original interface queries passed with RTLD_NOW, including VClient017,
VEngineClient014, GameUI011, VGUI_Surface030, VMaterialSystem081 and ShaderDLL004.
Unknown-interface rejection passed. The 64-bit SDL contact slot test also passed.
These checks establish linking and factory availability, not a rendered game.

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
Full client lifecycle, original server/content compatibility, sound codecs,
iOS bundle loading and Play wiring are pending. UIKit/EAGL portability is under
ARM64 CI verification. The first graphical ARM64 attempt failed on desktop GL/Carbon
headers and a missing BSP header; the next adaptation addresses those errors; no on-device original shader/HUD or server connection
has been demonstrated. No application version bump accompanies these checks.
