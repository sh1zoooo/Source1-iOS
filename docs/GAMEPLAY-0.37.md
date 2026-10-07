# CS:S offline practice — 0.37 / build 41

This stage connects UIKit controls to the original CCSPlayer on the imported
`awp_lego_2` map. It does not bundle the ClientMod resource archive.

## What changes in the app

- Play / Stop starts and shuts down the original game level, with a local player
  and an idle opponent on opposite teams to keep the normal round active.
- Left drag moves; right drag looks. Both gestures can run simultaneously.
- Hold Jump, Duck, Fire or Reload. Original CS:S movement, player hulls,
  gravity, crouching, weapon handling and ammunition remain in the GameDLL.
- A native HUD reads health, armor, money, ammunition and velocity from the
  original player. A crosshair appears in first person.
- 1P / 3P selects the actual viewmodel or player MDL, loaded with its VVD,
  VTX and VMT/VTF. CPU skinning can consume original server SetupBones matrices;
  supported embedded animation tracks remain a fallback.
- Simulation uses the GameDLL tick interval rather than the display frame rate.
  Pause clears analog movement and held buttons; it does not replay background time.

## Import and run

Unpack the supplied archive outside the app. Copy the actual `cm`, `cstrike`,
`hl2` and `platform` folders into **Files → Source 1 iOS Lab → Source1IOS →
content**. Keep VPK directory files next to their numbered chunks. Do not copy
only the RAR: the app mounts unpacked directories and validates VPK ranges.
Restart, or press Play after copying. The map `cm/maps/awp_lego_2.bsp` and the
weapon/player resources must be present. The original new-game input lock lasts
three seconds. This practice build exposes only the verified `awp_lego_2` map.
The command equivalent is `source_game_load awp_lego_2`; exit with `source_game_stop`.
Stop practice before changing content mounts or the preview scene.

## Implementation and checks

`SourcePlayerSDK.cpp` builds with the GameDLL's private definitions and exports
plain values and bounded bone matrices; UIKit never relies on SDK class layouts.
The public IBotManager controller creates a real FL_FAKECLIENT CCSPlayer and
runs original user commands. No network client, prediction or AI navigation is
implied by the local practice adapter.

Player stress-damage code revealed an IVP friction-snapshot lifetime bug: the
surface normal was read through a solver pointer already cleared after impact
processing. The computed normal now lives with the persistent contact point.
A floor-contact regression checks finite unit normals after simulation.

Native baseline: 7/7 CTest contracts passed. The real-cache gameplay probe
completed two practice cycles, including walking, wall collision, crouch eye
height, jump/landing, ammunition consumption, reload, 1P/3P geometry and pause.
Walking matched at 60 and 120 display calls; jump height was 65.4975 units.
Final native verification also passed both original bone-pose and projected
model-frustum checks: 50 player bones and 57 AWP viewmodel bones. Crouching
changes bone rotations; firing/reloading changed the actual cache-defined AWP
magazine 5 → 4 → 5 and reserve 30 → 29. Pause cleared movement, crouch and fire. Final CI verification passed.

Reproduce with user-supplied resources:

```sh
cmake -S . -B build -DSOURCE_BUILD_CSTRIKE=ON -DSOURCE_LINK_CSTRIKE=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
build/gameplay_probe /absolute/path/to/Documents awp_lego_2
```

## Remaining scope

This is an offline adaptation of the original server player with a Metal render
adapter and native HUD. It is not the full graphical CS:S client or the exact
ClientMod UI. The provided archive contains resources, not ClientMod code.
Original shader rendering, client prediction/network play, weapon bob/camera
recoil, sound, full client HUD, death/respawn UX, bots, other maps and physical
0.37 iPhone validation remain separate work. Source server poses cannot supply
client-only visual effects. Unsupported model poses use bounded fallback data;
an unavailable model is hidden rather than replaced with the preview fixture.


## Verified iOS artifact

[CI run 37621264912](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37621264912)
passed both Linux and iOS jobs for code head
`935c6944f7ec0848977c82652392319edf50dc5e`, merge
`5488e07257328cc5c052b5e9228eb1ed02ad92e1`.

[IPA and diagnostics](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37621264912/artifacts/11483041984):
0.37.0/build 41; outer ZIP 6,150,728 bytes, SHA256
`24d30850c6db5d3dea15ca2a472f3fb10c8ba95617aced5fefd762c0e10d6c9d`.
The unsigned IPA is 5,278,988 bytes, SHA256
`9a8ce2c100a14740331b151169a7b296ada2a30f31c125a5d5096faf54f6d858`.
ZIP CRC, ARM64 Mach-O executable and Info.plist were independently checked.
The downloaded runtime log contains exactly 264 PASS, no FAIL, GPU revision 10
and pause/resume. The screenshot was inspected: all six practice controls
are visible. CI does not contain the user's game archive, so it shows the
fixture preview; actual gameplay on the archive is covered by the native probe.

The [ARM64 game compilation archive](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37621264912/artifacts/11482233339)
contains 571 verified Mach-O ARM64 object files (548 game and 23 helpers);
manifest and licenses match the checkout. ZIP SHA256
`bc1da7b8e1f7408f8ff459c6eb7b6f1522b8a6ea4737cecbb88fffc8505affa7`.
Physical 0.37 iPhone gameplay and multi-touch behavior are not yet verified.
