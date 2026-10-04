# Source 1 → iOS: current status

## Upstream and reproducibility

Baseline: [nillerusr/source-engine](https://github.com/nillerusr/source-engine),
commit `ed8209cc35c61fbd8ddff8480962a01c981eef2f`. The upstream README states that
this fork originates from the TF2 2018 leak; it is not an official Valve iOS port.
The official Source SDK 2013 does not include the complete standalone engine.

The dependency is a pinned, non-recursive git submodule. `cmake/source-files.json`
contains the 304 compilation units compiled into eleven upstream static libraries. Preparation
copies the selected source directories into the build tree, verifies the commit,
and checks each patch anchor before applying it. Upstream files remain unchanged.
Source license and third-party notices are bundled in the IPA.

## Implemented / executed

- Real tier0 allocator, command line, CPU information, monotonic clock and threading code.
- Real tier1 interface registry, CRC32, bitbuf, KeyValues and ConVar/ConCommand.
- Real mathlib initialization, AngleMatrix and VectorTransform.
- Real vstdlib CVar IAppSystem acquired as `VEngineCvar004`, connected, initialized,
  disconnected and shut down. KeyValues' vstdlib backend is also exercised.
- Original CAppSystemGroup starts cvar and filesystem via static factories,
  connects dependencies and shuts down in reverse order. No fake engine host.
- Original filesystem_stdio (`VFileSystem022`) and vpklib, plus tier2.cpp,
  fileutils.cpp and utlstreambuffer.cpp. Tier2 mesh/render helpers are linked as engine dependencies.
- GAME and DEFAULT_WRITE_PATH mounted at Documents/Source1IOS/game; tests use
  an isolated PORT_TEST path and explicitly mounted VPK fixtures.
- Native UIKit lifecycle and a custom Metal test backend with a depth buffer.
- Generated rotating cube; CPU transforms use Source mathlib every frame.
- In-app console with real Source command tokenization, dispatch and variable updates.

All eleven upstream static libraries compile their manifest entries. Only the features above
are exercised. This is a **partial core and filesystem port**, not a running engine host.
No fabricated CreateInterface or substitute CRC / KeyValues implementation is used.

## Platform changes

- Enable Darwin/POSIX compatibility plus an explicit `SOURCE_IOS` target marker.
- Preserve ARM64 pointer width and SSE-to-NEON support already present upstream.
- Remove the legacy nullptr macro for modern C++.
- Remove the C++17-incompatible MD5 register specifier.
- Add the missing execinfo include for backtrace declarations.
- Use CLOCK_MONOTONIC for the ARM nanosecond counter; report its actual 1 GHz counter
  frequency on iOS rather than treating unavailable CPU frequency as timer frequency.
- Correct fractional-second subtraction in the Linux timer.
- Statically link modules, disable desktop malloc hooks and replacement global new/delete.
- Expose the stdio module’s original factory for static linking.
- Correct upstream embedded VPK v1 offsets: remember the parsed header size
  instead of always adding the 28-byte v2 header. Both versions are tested.
- Enable CMPXCHG16B for Linux x86_64 lock-free file-tracker queues.
- Serialize diagnostic log writes from Source I/O workers.
- Track successfully connected/initialized app-systems for partial-start rollback.
- Restore the parent app-system factory after shutdown.
- Restore host-owned console commands after static module disconnect/reconnect.

## Validation

Linux tests exercise known-answer CRC32, a mixed-width bitbuf round trip, KeyValues
parsing, a 90-degree vector transform, allocation, monotonic time, real console
variable registration/update, filesystem write/read/seek, path isolation,
KeyValues file serialization, asynchronous read and independent VPK v1/v2
embedded-data fixtures, appframework interface lookup, reverse shutdown,
connection/init failure rollback, parent-factory restoration, host restart and foreground/background behavior.

Actions must compile both iPhone ARM64 and the simulator. The simulator smoke
requires `Source core initialized: tier0/tier1/mathlib/vstdlib` and the first Metal
frame and `Source filesystem initialized: filesystem_stdio/vpklib`,
and `Source appframework initialized: CAppSystemGroup`, with no failed self-tests. A screenshot and runtime log are saved.
A passing simulator does not prove that this new module integration works on A18.

## Physical-device check

1. Install the newly signed IPA. Expect a rotating cube and core self-tests PASS.
2. Execute `source_app_selftest`, `source_fs_selftest`, `source_selftest`, then `ios_rotation_speed 0` and `ios_rotation_speed 30`.
3. Rotate the device, background it for 20 seconds and resume.
4. Export the diagnostic log. It must include the Source upstream revision,
   VEngineCvar004 initialization and PASS for all twenty-six checks.

## Remaining engine work

The dedicated `engine` module is statically linked (169 compilation units), with tier3 and bitmap dependencies. Engine command buffering and independent spatial partitions are exercised; full Host_Init, materialsystem / shaderapi, BSP world loading, audio, networking and game client/server execution remain pending.
Next: port the dedicated engine dependency graph and engine platform code, then attempt
headless engine startup. `engine/sys_dll2.cpp::CModAppSystemGroup::Create` still
loads a game server module even in server-only mode; full host startup needs that
interface or a deliberately scoped engine initialization path. Upstream's engine
build also links appframework, tier3, datamodel, bitmap/vtf and several desktop
libraries. No stub server or fabricated engine interface has been added. Materialsystem integration is a separate stage; the Metal
cube is not evidence that original Source materials or maps can render.

## Engine integration (v0.5)

The whole dedicated engine archive is retained, including its genuine interface registry.
Typed offline Steam entry points return null/failure; they do not implement authentication
or claim Steam services. A function-local tier0 command-line singleton fixes engine
global constructor order in a statically linked executable. Shared collision, network
data-table and IceKey implementations are enabled once for the static build.

Spatial queries acquire the original MDLCache lock when the cache exists. Bounds-only
independent partitions without query callbacks can operate before MDLCache startup;
queries against the global partition or any callback-bearing partition still require it.
The app does not call dedicated API Connect/ModInit or Host_Init yet.

The app console exposes only initialized port commands while engine host initialization
is pending. Engine subsystem tests execute a controlled command through original
Cbuf_AddText/Cbuf_Execute and remove the probe from the registry afterwards.
