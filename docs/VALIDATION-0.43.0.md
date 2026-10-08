# 0.43.0 / build 47

Changes address the actual iPhone `de_mirage_go` log ending in cached prop budget
exceeded. Reuse material slots by canonical VMT path across BSP/static/dynamic
models, allow a bounded eight-million-vertex CPU scene, and cull prop instance
ranges outside the active camera before GPU vertex expansion. Real Mirage has
1,422 rendered prop instances and 1,160,313 prop triangles. Invalid absolute
studio material directory candidates are skipped while valid relative paths
remain usable; malformed string/offset bounds are still rejected.

For active gameplay, use the collision world already loaded by the original
GameDLL. Do not rebuild the whole render BSP as one preview IVP polysoup: its
16-bit vector overflows on Mirage. Third-person camera traces use engine BSP
collision plus imported prop collision. Fixture previews retain their original
independent physics tests.

Export and render live unowned weapons and grenade projectile transforms,
skins and bodygroups. Flashbang projectile class is handled explicitly.
Advance the server viewmodel's animation cycle, use its actual model/bodygroup,
and retain sub-tick input presses until usercmd dispatch; pausing clears both
held and pending inputs. Restore four readable HUD panels even when imported
hudlayout is absent. Flash overlay follows the server blind state. Smoke uses
adapter billboards at the real server smoke entity; it is not the original
ClientMod particle renderer.

APK extras importer and inspection findings: ANDROID-CLIENTMOD-1.4.md.
No APK binaries or game assets are redistributed in this repository.

Validation:
- Native build succeeds.
- CTest: 8/8 pass, including studio malformed-input coverage.
- Full user-owned cache plus APK extras: WORLD GAMEPLAY PASS. Dropped weapon
  server entity and additional world geometry checked. HE, flashbang and smoke
  bought and thrown via a tap shorter than a server tick; projectile presence
  checked for each. Real animation cycles advance.
- APK configuration imports 32 buttons, 30 icons and four HUD panels. Hidden
  button flags and show/hide commands follow the imported weapon-slot layout.
  Inventory next/previous switching and IN_SPEED walking pass the live probe.
- Mirage: MAP PASS after movement (339 units), shutdown and a second map load.
  Both gameplay benchmark cycles pass movement, wall collision, crouch, jump,
  firing, reload, viewmodel poses and pause checks.
- Continuous offline practice suppresses automatic round cleanup and clears
  the initial restart timer after explicit player spawn. Ordinary match round
  restarts remain pending: Mirage brush entities fail during that cleanup.
- Final Apple device compilation and simulator smoke checks are pending.

This is still a dedicated original CS:S GameDLL with a Metal renderer adapter.
The original graphical client, Android GameUI and ClientMod shaders are not
fully ported. Actual iPhone appearance/performance require a device run.
