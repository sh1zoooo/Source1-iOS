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
Final original-bone-pose and iOS CI verification are in progress.

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
