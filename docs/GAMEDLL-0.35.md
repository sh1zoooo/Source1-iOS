# 0.35.0 / build 39: embedded original CS:S GameDLL

`SOURCE_BUILD_CSTRIKE=ON` plus `SOURCE_LINK_CSTRIKE=ON` links the pinned original
CS:S server into the executable. The original static interface factory resolves
IServerGameDLL, IServerGameEnts and IServerGameClients; Host_Init calls real
DLLInit. Networking is disabled. No replacement game-interface implementations
are provided.

The monolithic build owns 13 common SDK implementations in the engine instead
of duplicating them in the game archive: 548 server objects, 10 particles,
5 DMX, 5 choreo, 2 sound emitter and 1 scene cache. Compile-only configuration
still has 561 server and 3 sound-emitter objects. DLL-local globals and the two
unrelated CPhysicsSpring classes retain separate names and types. GNU resolves
all archives in one rescan group; Apple force-loads the registration archives.

The app mounts authored offline-demo resource descriptions after user content:
a default surface, empty sound manifests and a minimal HLTV status schema.
These allow DLLInit, not a playable CS:S level. Steam-account reporting and
third-party desktop DLL plugins are unavailable in this offline embedded build.
The genuine plugin-helper interface remains available.

Startup adds four checks: successful original DLLInit, 196 server classes
including CCSPlayer/CAK47, original player limits and game tick interval.
Total: 122 startup PASS. Shutdown calls Host_Disconnect and original
SV_ShutdownGameDLL before Host_Shutdown; SendTables, game systems, navigation
and DLL interfaces are cleaned up. Static game-system registration avoids
duplicate singleton entries across host restarts.

Local Linux Debug validation: final 6/6 CTest passed, including repeated
host startup/shutdown and the four existing sanitizer mutation suites.
CI 37603005915 also passed Linux 6/6, ARM64 IPA and iOS simulator. The dedicated game contract also
uses original CGameServer::SpawnServer to load the authored test BSP. This is
not LevelInit or SV_ActivateServer. A diagnostic attempt at LevelInit identified
the missing world PHYSCOLLIDE lump; it is not counted as a passing check.

Remaining game-level work: cooked world collision, original CS:S entity
initialization and activation, actual game frames, player/client integration,
resources and linkage to the Metal presentation. The existing BSP camera and
visual model entities continue using the bounded preview adapter. Full Source
shader rendering, networking and audio remain separate porting work.

## Final validation

- Commit b0cb02ddf1307bedba16fd2d451f38a1463d244c; CI merge ref
  0d422f7becc6560ddd5c60db3c487a94f16c2c23.
- Workflow 37603005915: Linux job 112731672331 and iOS job 112731672073 passed.
- Simulator smoke requires exactly 264 PASS (two startup sets of 122 plus
  20 scene/runtime checks), original GameDLL initialization, GPU revision 10,
  camera spawn, textures, HDR/PHY/skins/bodygroups and pause/resume. Its step passed.
- Independently downloaded ARM64 compilation artifact 11473996886: ZIP integrity,
  all 571 Mach-O MH_OBJECT CPU_TYPE_ARM64 members, counts, licenses and manifest
  matched. ZIP SHA256 db8e9abaafc7202f05260c3e876f042564cf794aa40781666d0ff9626bf57902.
- [IPA 0.35.0/build 39 and simulator diagnostics](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37603005915/artifacts/11473993544)
  are in artifact 11473993544 (6,134,565 bytes). GitHub reports outer ZIP SHA256
  f4711dfced7ad6cc52cdd7dfac1ebc790d6e70415d813f3bd13484f554c86504.
- The workspace disconnected during the final IPA/diagnostics download; the IPA
  and screenshot were not independently inspected. The CI checks above passed.
  No new physical iPhone run has been performed.

The **minimal demo implementation milestone is complete (100%)** under the
existing criterion: genuine engine, authored BSP world, adapted materials/Metal
rendering and camera controls. This is not a percentage of full CS:S readiness
or certification of real-cache compatibility. Active GameDLL LevelInit, player,
client, full Source shaders and game content remain a separate stage.
