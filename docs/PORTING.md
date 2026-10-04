# Source 1 → iOS: current status

## Upstream and reproducibility

Baseline: [nillerusr/source-engine](https://github.com/nillerusr/source-engine),
commit `ed8209cc35c61fbd8ddff8480962a01c981eef2f`. The upstream README states that
this fork originates from the TF2 2018 leak; it is not an official Valve iOS port.
The official Source SDK 2013 does not include the complete standalone engine.

The dependency is a pinned, non-recursive git submodule. `cmake/source-files.json`
contains the 101 source files compiled into four static libraries. Preparation
copies the selected source directories into the build tree, verifies the commit,
and checks each patch anchor before applying it. Upstream files remain unchanged.
Source license and third-party notices are bundled in the IPA.

## Implemented / executed

- Real tier0 allocator, command line, CPU information, monotonic clock and threading code.
- Real tier1 interface registry, CRC32, bitbuf, KeyValues and ConVar/ConCommand.
- Real mathlib initialization, AngleMatrix and VectorTransform.
- Real vstdlib CVar IAppSystem acquired as `VEngineCvar004`, connected, initialized,
  disconnected and shut down. KeyValues' vstdlib backend is also exercised.
- Native UIKit lifecycle and a custom Metal test backend with a depth buffer.
- Generated rotating cube; CPU transforms use Source mathlib every frame.
- In-app console with real Source command tokenization, dispatch and variable updates.

All four static libraries compile their manifest entries. Only the features above
are exercised. This is a **partial core-library port**, not a running engine host.
No fabricated CreateInterface or substitute CRC / KeyValues implementation is used.

## Platform changes

- Enable Darwin/POSIX compatibility plus an explicit `SOURCE_IOS` target marker.
- Preserve ARM64 pointer width and SSE-to-NEON support already present upstream.
- Remove the legacy nullptr macro for modern C++.
- Add the missing execinfo include for backtrace declarations.
- Use CLOCK_MONOTONIC for the ARM nanosecond counter; report its actual 1 GHz counter
  frequency on iOS rather than treating unavailable CPU frequency as timer frequency.
- Correct fractional-second subtraction in the Linux timer.
- Statically link modules, disable desktop malloc hooks and replacement global new/delete.
- Restore host-owned console commands after static module disconnect/reconnect.

## Validation

Linux tests exercise known-answer CRC32, a mixed-width bitbuf round trip, KeyValues
parsing, a 90-degree vector transform, allocation, monotonic time, real console
variable registration/update, host restart and foreground/background behavior.

Actions must compile both iPhone ARM64 and the simulator. The simulator smoke
requires `Source core initialized: tier0/tier1/mathlib/vstdlib` and the first Metal
frame, with no failed Source self-tests. A screenshot and runtime log are saved.
A passing simulator does not prove that this new module integration works on A18.

## Physical-device check

1. Install the newly signed IPA. Expect a rotating cube and core self-tests PASS.
2. Execute `source_selftest`, then `ios_rotation_speed 0` and `ios_rotation_speed 30`.
3. Rotate the device, background it for 20 seconds and resume.
4. Export the diagnostic log. It must include the Source upstream revision,
   VEngineCvar004 initialization and PASS for all seven checks.

## Remaining engine work

The `engine` module, original filesystem, materialsystem / shaderapi, BSP world
loading, audio, networking and a game client/server are not linked.
Next: port the original filesystem and app-system dependency graph, then attempt
headless engine startup. Materialsystem integration is a separate stage; the Metal
cube is not evidence that original Source materials or maps can render.
