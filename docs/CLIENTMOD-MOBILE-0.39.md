# ClientMod mobile resources — 0.39

This increment reads the user's imported cache at runtime; no ClientMod assets are redistributed in this repository.

- `cfg/touch.cfg` supplies button names, actions, positions, opacity, movement zones and look sensitivity. Duplicate names replace earlier definitions, as in the original configuration. The supplied cache resolves 22 records, including two gesture regions and 20 icon buttons.
- Icons resolve VMT base textures first, then the adjacent VTF used by the touch configuration when a VMT points to an absent Android logo path. The Source filesystem now resolves mixed-case directory components, including `RBTouch`.
- Supported actions: fire, alternate fire/zoom, jump, crouch, reload, use, drop, weapon slots, inspection when a matching sequence exists, team selection, buying and exiting. Settings enables dragging the buttons; tapping settings again ends editing. Positions persist in app preferences. Chat/voice require a network client and are not implemented. The score control displays local practice information.
- The buy catalogue reads the imported team-specific `resource/ui/buy*.res` commands. Prices, team restrictions, inventory and money use the original server's weapon data and purchase handler. Offline practice permits buying away from buy zones and starts with $16000. The supplied T catalogue has 24 items. The native list does not reproduce the original VGUI artwork or layout.
- HUD health, armor, ammo and money panel positions come from `scripts/hudlayout.res`. Values are live server values. Font/icon fidelity and remaining HUD panels are pending.
- AWP secondary fire exports the server FOV and hides the viewmodel while zoomed. The crosshair is a temporary native marker, not the full original scope graphic.
- `awp_lego_2` and `aim_map_csgo` pass local start, rendering, movement, shutdown and restart checks. Other installed maps remain visible with compatibility pending. Mirage exceeds the current prop geometry budget; Office still fails during physics simulation; the other tested large maps need additional adapter work.
- Lightmaps grow vertically from 1024 to at most 8192 texels, with UV rescaling and a 32 MiB atlas cap. Geometry and displacement parsing retain finite-value, range and memory bounds; legacy displacement vectors and blending alpha no longer assume unit vectors or byte-range alpha.

## Validation

Manual integration tests require the user-supplied content under `DOCUMENTS/Source1IOS/content/{cm,cstrike,hl2,platform}`:

```
build/mobile_probe DOCUMENTS
build/map_probe DOCUMENTS aim_map_csgo
```

The mobile probe checks imported icons/catalogue/HUD, AWP zoom, AK price/account/inventory, repeat/invalid/wrong-team purchases, pistol purchase, slots, drop and team respawns with a CT rifle purchase. The native contract suite also checks reading a mixed-case loose directory. Simulator smoke creates a fresh device and opens Simulator before launching, following the previous cold-launch timeouts. iOS build and smoke results were checked separately from native probes.

On 2026-10-08, [workflow 37764731493](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37764731493) passed both jobs for code commit `400c7f8e9eccb4602a68086e67ca6679a67930a2` (PR merge build `062f221293a85b1e2e32291c6bb662f541ef76a8`). Device ARM64 and simulator builds passed; simulator GPU/runtime checks and diagnostics/menu UI checks passed, with screenshots captured. Native CI passed 7/7 contracts. User-cache gameplay controls were verified by the Linux integration probes; physical iPhone validation of this increment remains pending.

[Download the Source1IOS artifact containing `Source1IOS-unsigned.ipa`](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37764731493/artifacts/11544483559). This is the 0.39 / build 43 app package. The separate `CStrike-ARM64-compile` artifact contains libraries, not an installable app. Artifact ZIP: 6,260,259 bytes; SHA256 `df4d72b033d95d3559421335d538f1e40ba3b4de373b9199713a812ba72fb752`.

## Still pending

Original graphical Source shaders and a full ClientMod client DLL are not active. The renderer remains the Metal adapter over imported BSP/MDL/VMT/VTF data. The archive contains standard CS:S player bodies and replacement weapon viewmodels, with no separate CS:GO agent pack. No complete-port percentage is claimed.
