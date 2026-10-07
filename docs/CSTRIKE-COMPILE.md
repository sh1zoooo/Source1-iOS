# Original CS:S server compilation stage

This is a separate stage on top of the verified 0.34.0 prototype. The application
version remains 0.34.0 / build 38: these archives are not linked into the app.
The ~92% estimate still refers only to the minimal demonstration prototype.

## Scope

`SOURCE_BUILD_CSTRIKE=ON` adds `source_cstrike_server` with all 561 translation
units selected by the pinned upstream Waf/VPC configuration for `cstrike`:
`server_base.vpc`, `server_cstrike.vpc` and `nav_mesh.vpc`. This includes original
CS:S players, weapons, game rules, bots, hostages and navigation, rather than
preview entity replacements. Linux and Apple selections are checked for equality.

Five supporting archives compile another 24 units: particles (10), dmxloader (5),
choreoobjects (5), soundemittersystem (3) and scenefilecache (1). Their source lists
follow the respective upstream Waf scripts; the memoverride allocator replacement
is intentionally excluded, as in the existing static engine port.

Source revision: `ed8209cc35c61fbd8ddff8480962a01c981eef2f`.
Only copied build-tree files are patched; pinned submodules remain unchanged.
Audited game compatibility changes:

- Match Steam context Init/Clear guards to the existing `NO_STEAM` declarations.
- Use ordinary class-specific game-rules registration names instead of the old
  Xbox static macro that tries to concatenate an unexpanded function macro.
- Follow upstream Waf in removing `PROTECTED_THINGS_ENABLE`; retain original
  game definitions, with explicit offline `NO_STEAM` / `DISABLE_STEAM`.
- Refuse account-based vote initiation and omit Steam-ID-keyed purchase reports
  offline; local money statistics remain enabled. These callers previously used
  `GetSteamID` even though its declaration is excluded by `NO_STEAM`.

The default remains OFF. CI enables this stage for Linux and the iPhone ARM64
device build; simulator smoke continues to test the existing app independently.

## Reproduce

```sh
git submodule update --init --depth 1
git -C third_party/source submodule update --init --depth 1 thirdparty ivp
python3 scripts/cstrike_manifest.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSOURCE_BUILD_CSTRIKE=ON
cmake --build build --target cstrike_compile_check --parallel 4
python3 scripts/verify_cstrike_archives.py build
```

The manifest check reruns the pinned original VPC reader. `--write` explicitly
regenerates the reviewed list; configure fails if the selected list has drifted.
Archive verification checks the exact object count in all six archives. This
detects accidentally omitted translation units; it is not a linker or runtime test.

## Remaining integration

GameDLL is not initialized, and the engine still uses `-nogamedll`. The new
archives retain unresolved references by design. Before connecting them to Host:

1. Resolve ownership of globals, send-table helpers and studio implementations
   shared by desktop DLLs; preserve distinct engine and game interface factories.
2. Link support modules and expose genuine sound-emitter and scene-cache systems.
3. Exercise GameDLL Init/Shutdown rollback, reconnects and original server frame
   callbacks, with explicit failures for unavailable Steam services.
4. Supply real game content, start a map and verify actual player/entity simulation.

The client game DLL, original graphics pipeline, player input, networking and
physical iPhone game execution remain separate work.

## Validation

Linux GCC 13 Debug: all six archives compiled successfully; archive verification
passed 561/561 server objects and 24/24 supporting objects. The final full Linux
Debug rebuild and all five CTest suites passed with the option enabled.
iOS ARM64 CI validation is pending.
No playable CS:S or new device game run is implied by archive compilation.

A Linux `nm -g --defined-only` audit found 226 strong symbol names shared between
the existing engine archives and these game/support archives. Many are common
helpers (send tables, collision utilities, studio code) that can have one owner.
Others require deliberate isolation: engine `sv_cheats` is a ConVar object while
the game uses a pointer; `modelinfo`, `physprops`, `registry` and `developer` also
have desktop DLL ownership assumptions. This is a symbol inventory, not a claim
that every overlap would be a final linker error. Simply force-loading the game
archive into the current executable is not a validated integration strategy.
