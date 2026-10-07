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
iOS CI is pending. The dedicated game contract also
uses original CGameServer::SpawnServer to load the authored test BSP. This is
not LevelInit or SV_ActivateServer. A diagnostic attempt at LevelInit identified
the missing world PHYSCOLLIDE lump; it is not counted as a passing check.

Remaining game-level work: cooked world collision, original CS:S entity
initialization and activation, actual game frames, player/client integration,
resources and linkage to the Metal presentation. The existing BSP camera and
visual model entities continue using the bounded preview adapter. Full Source
shader rendering, networking and audio remain separate porting work.

ARM64 IPA and iOS simulator validation are pending. No new physical iPhone run
has been performed. The previous ~92% estimate belongs to the minimal visual
demo, not full CS:S, and is not raised by compilation alone.
