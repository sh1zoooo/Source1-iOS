# Source 1 → iOS: current status

## Upstream and reproducibility

Baseline: [nillerusr/source-engine](https://github.com/nillerusr/source-engine),
commit `ed8209cc35c61fbd8ddff8480962a01c981eef2f`. The upstream README states that
this fork originates from the TF2 2018 leak; it is not an official Valve iOS port.
The official Source SDK 2013 does not include the complete standalone engine.

The dependency is a pinned, non-recursive git submodule. `cmake/source-files.json`
contains 539 compilation units compiled into twenty-three upstream static libraries.
A twenty-fourth static library compiles the original displacement collision implementation
in tool mode with distinct type/export names (540 total compilation units), providing
heap-owned preview trees without mixing the engine's hunk-backed class ABI. Preparation
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
- Built-in BSP room, camera and live physics bodies; Source mathlib transforms every frame.
- In-app console with real Source command tokenization, dispatch and variable updates.

All twenty-three upstream static libraries compile their manifest entries. Only the features above
are exercised. This is a **partial engine port**: original engine-only host and built-in brush world are active; rendering uses the custom Metal adapter.
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

The user’s iPhone 16e log confirms the 39-check host build and two pause/resume cycles.
For the new build: expect a textured room and two yellow physics spheres. Drag left
for movement and right for view. Repeat source_selftest, apply source_physics_impulse,
then background for 20 seconds and resume. Export the log after these actions.

## Remaining engine work

Original graphical shaderapi/materialsystem, map materials/lightmaps/PVS,
displacements, studio model drawing/animation, audio and a genuine game DLL remain.
The dedicated host runs without a fabricated server interface. The Metal adapter
is separate from the original Source rendering pipeline.

## Engine integration (v0.5)

The whole dedicated engine archive is retained, including its genuine interface registry.
Typed offline Steam entry points return null/failure; they do not implement authentication
or claim Steam services. A function-local tier0 command-line singleton fixes engine
global constructor order in a statically linked executable. Shared collision, network
data-table and IceKey implementations are enabled once for the static build.

Spatial queries now acquire the original initialized MDLCache lock. The earlier
independent-partition lock exception is removed. Connect/Init/Shutdown/Disconnect
run for actual materialsystem (shaderapiempty), physics, data/model cache, studiorender
and dedicated engine API. The v0.7 engine-only lifecycle is described below.

The app console exposes only initialized port commands while no game DLL is loaded. Engine subsystem tests execute a controlled command through original
Cbuf_AddText/Cbuf_Execute and remove the probe from the registry afterwards.

## Headless dependency graph (v0.6)

Pinned IVP dependency: 47533475e01cbff05fbc3bbe8b4edc485f292cea.
Pinned thirdparty dependency: c5b901ecef515ea068fa8b8a19ca5cd5353905cb (stb headers).
The original shaderapiempty is selected from the static registry, avoiding desktop
module loading. Headless startup still reports missing desktop shader DLLs; those
shaders and a Metal shaderapi implementation are outside this stage.

Shared tier connections are reference counted so model-cache Disconnect can still
remove its material callbacks before the final tier release. Original static console
objects are remembered across reconnects, with destroyed objects removed from the
remembered set. Physics and VTF tests run during startup, repeat testing and restart.

35 checks include three VTF contracts, two data-cache contracts, physical collision
construction, ray fraction and actual gravity simulation. These checks and the later
39-check host build are confirmed by the user’s physical iPhone log.


## Engine-only host lifecycle (v0.7)

The original filesystem QueuedLoader.cpp was already compiled but its interface
registration was dropped from the static archive. Retain the filesystem archive
and add its real IQueuedLoader to the app-system lifecycle before the engine.
Host_Init(true) uses upstream -nogamedll to avoid requiring a fabricated server
interface. Host_RunFrame advances real idle ticks; this does not load a map.

SourceFiles owns mounted paths. Host path strings remain alive until Host_Shutdown.
Game configuration queued during Host_Init is discarded because arbitrary game
commands need a real server DLL. Networking is disabled with -noip; the original
dedicated warning is expected in this engine-only harness.

The shutdown recursion guard now resets after successful shutdown. The hunk stack
terminates its reservation on shutdown rather than retaining a base that prevents
reinitialization. Command-buffer tests preserve a live host's buffer lifetime.
39 startup checks include queued loader, Host_Init and idle tick advancement.
Linux contracts exercise repeat tests, pause/resume, shutdown and restart.
The 35-check v0.6 IPA passed startup and all checks on the physical iPhone 16e
(A18, iOS 18.6.2); the v0.7.1 Host_Init / idle tick build also passed the user’s physical device run.


## Ragdoll solver validation (v0.7.1)

CreateRagdollConstraint joins two original physics bodies with three angular limits.
After a lateral impulse and one second under gravity, transformed joint anchors
must remain within 0.2 Source units while the dynamic body moves. This exercises
Havana on ARM64, a prerequisite for a physical character skeleton. It is one joint,
not a loaded or rendered character. Constraint and bodies are destroyed before
the environment, also during repeated self-tests and host restarts.


## BSP polygon preview (v0.8)

Source CMapLoadHelper reads vertices, edges, surfedges and faces from validated
BSP 19–21 sections. Polygon triangulation and the Metal adapter are port code,
not the original brush renderer. Static collision uses real IVP Polysoup APIs.
Source AngleVectors drives the camera; swept hull movement stops at obstacles.
The original VTF library serializes/reads/decodes the preview checker texture.

All file/lump bounds and polygon indices are checked before the legacy loader.
Compressed lumps and external overlays are unsupported. Failed loads preserve
previous mesh/collision. Dedicated host shutdown follows mesh/collision cleanup.
The v0.9 built-in fixture also contains brush collision, nodes, leaves, texinfo,
model and entity sections. Original CModelLoader/CM_LoadMap create its brush world;
CM floor traces, point contents and worldspawn are checked. User-map preview does
not replace this engine world. Lightmaps, PVS and Source shader rendering remain.
Simulator evidence must verify the resulting image.

Live physics uses original environments, objects and a ragdoll joint; the renderer
reads actual body pose matrices. Shapes outlive their environment objects. Default
surface properties must be parsed before contact simulation in the no-game harness.
Resources use short relative paths through a dedicated filesystem search path: legacy
BSP loading reopens files without the caller path ID, and absolute iOS container
paths interact badly with Source path normalization.
