# 0.22.0 / build 25 — BSP spawn camera

Reads BSP entity data to place the preview camera at a player start with a
64-unit eye offset. Selection prefers info_player_start, then CT, T and
deathmatch, preserving file order within each class. Angles/angle yaw and the
legacy -1/-2 vertical values are decoded; pitch is limited to the preview's
85-degree range and roll is zero. source_camera_reset restores this map start.
No player, game entity factory, outputs or imported configs are executed.

Entity text is bounded to 1 MiB, 8192 entities, 256 key/value pairs per entity,
4096 bytes per token and 512 spawn candidates. Quotes, // comments, unquoted
pairs and one trailing NUL are supported. Malformed text, nonfinite/out-of-range
spawn numbers, entity lump versions or damaged compressed data reject the map
before changing the current scene. A map without a player start keeps the
existing test-room fallback; blocked spawn positions are not relocated yet.

Local original-engine Debug contracts passed: changed projection at a CS team
start, camera reset and rejection preservation. A separate ASan/UBSan parser
probe passed 10000 deterministic mutations: 4551 accepted, 5449 rejected, with
finite accepted positions and preserved output on rejection. Leak checking is
disabled for this bounded parser probe; it is not exhaustive fuzzing.

Startup has 80 checks. Simulator smoke requires two sets plus runtime contracts
(161 PASS), the selected spawn marker, imported BSP GPU revision 5, textures,
external ANI and pause/resume. Linux Actions contracts and ARM64 device/simulator
builds passed in run 37419416851. Its unbounded simulator-startup step did not
finish while the next stage was prepared. The combined 0.23 run supersedes that
smoke check; 0.22 itself has no independently verified simulator artifact.

The combined 0.23 simulator run passed with 167 PASS and the selected spawn
marker; [verified result](VALIDATION-0.23.0.md). Run 37419416851 was cancelled
while waiting before app startup; its two-file artifact contains IPA/SHA256,
not a successful simulator log or screenshot.
