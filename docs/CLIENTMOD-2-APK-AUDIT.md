# ClientMod Android 2.0 APK versus the iOS runtime

Inspected 2026-10-19. User-supplied `app-release.apk`, SHA256
`fe8f5fdd8fd70a0908e4eedb7ef17f18a88dd8b6e2463e41919a9c856b228364`.
No Android code was executed. String presence establishes binary contents,
not successful gameplay or complete implementation of a feature.

## Actual APK contents

* Package `com.valvesoftware.source.clientmod`, versionName `2.0`, versionCode
  `1217`. Contains 27 native libraries for each of ARM64 and ARMv7, not merely
  an updater. The game SDLActivity requests sensor landscape.
* `libSDL2.so` identifies itself as `SDL-2.32.10-g8ccb45b`.
* `libclient.so` contains `cm_skinchanger_dialog`, `cm_loadout*`,
  `cl_show_player_legs`, `cl_ragdoll_force`, `cl_ragdoll_fade_time`,
  `cl_radar_always_centered`, `cl_crosshairgap` and loadout dialog paths.
* `libengine.so` contains `cl_forcepreload`, `bancmid`, `removecmid`,
  `writecmid`, and the banned-CMID configuration path.
* ASTC 12x12 and ETC2 format names occur in the rendering libraries. Their
  actual native upload paths still need source or runtime validation.
* ELF ARM64 shared objects depend on Android system libraries (`libandroid`,
  `liblog`, Android libc, etc.). They cannot be used as iOS Mach-O dylibs by
  changing filenames or placing them in an IPA.

## Current iOS baseline (0.46.26)

| Feature | Evidence/status |
| --- | --- |
| Radar/crosshair customization | Older options exist; not the full new option set |
| Inventory, addons, live gloves/agent changes | Not implemented in the pinned public client |
| First-person legs | `cl_show_player_legs` absent |
| Ragdoll fade/force options | Absent in the compiled CS:S client paths |
| Full `cl_forcepreload` | Absent |
| ASTC | VTF IDs 43/47, 4x4 and 6x6 only; native upload if advertised, CPU RGBA fallback otherwise |
| ETC2 / all ASTC block sizes | Not implemented in the added iOS compatibility path |
| Claimed 4–6x memory reduction | Cannot be claimed when this driver requires RGBA decoding |
| SIMD/NEON | Existing Source SIMD uses sse2neon on ARM; not the new 2.0 math rewrite |
| SDL | Pinned 2.0.17, not 2.32.10 |
| Pause | Standard Source `sv_pausable` exists |
| CMID | Not implemented |
| Downloads | Existing network limit is 26 bits, approximately 64 MiB, not 256 MiB |
| New knife/movement/crouched hitbox/audio/CVar/map-limit fixes | Equivalence to the listed 2.0 changes not established |
| Friction | Our persistent contact-normal fix is separate; it does not prove the reported constant-0.5 material-friction issue is fixed |

The iOS version uses the old public ClientMod game code with selected iOS,
rendering and resource adaptations. Importing the 2.0 content does not transfer
the 2.0 gameplay or engine implementation.

## Source search

The updater points to `clientmod-android/updates`; at inspection it contains
only a README and no published releases. The public `clientmod-android/news`
repository is empty. Credits identify AndraMidoxXx/cherrybtw and ndke_01/dmk95.
Public cherrybtw repositories include a Source fork and prebuilt ClientMod
libraries, but the inspected public branches do not establish a source revision
corresponding to this 2.0 APK. The supplied ksagameng2 Source fork's inspected
CS:S player and image-format files also lack the identifying new options and
ASTC/ETC2 definitions. Do not assume a similarly named fork is the APK's source.
A corresponding source snapshot is still needed for a faithful rebuild; selected
features could instead be reimplemented separately in the existing port.

## Immediately reusable assets

`assets/extras_dir.vpk` is a self-contained VPK v2 with 93 files. It contains
30 original touch materials and 30 original touch textures, missing from the
separate content archive. All 30 textures are RGBA8888 256x256 VTF 7.5, supported
by the current runtime. The touch-only compatibility ZIP preserves their bytes;
each file was checked against its VPK CRC and the ZIP was round-trip verified.
No shaders, control configuration or gameplay libraries are included.

Online play remains unverified. Existing launch arguments still include
`-noip`; server integration work is not a completed feature. No target IP was
provided, and external UDP probes from the execution environment fail with
`Network is unreachable`.
