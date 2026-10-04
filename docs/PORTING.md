# Source 1 → iOS

## Milestone 0: host foundation

This repository contains our own C++ host services and an Objective-C++ UIKit/Metal
application. **It does not contain or run the Source engine yet.** The triangle
validates the iOS graphics path, not Source's material system.

The host has a sandboxed document directory, flushed logs, a monotonic frame timer,
and foreground/background handling. No game assets, Steam login, networking,
dynamic plugin loading, or upstream Valve code are bundled.

## Source baseline decision

The official [Source SDK 2013](https://github.com/ValveSoftware/source-sdk-2013)
is a reference for shared code and game-side interfaces. Its README describes HL2,
HL2:DM and TF2 game code; it is **not a complete engine-source release**.
It cannot supply every engine module needed for a standalone port.

Before adding upstream code, choose a repository and pin a commit. Preserve its
license and record the included modules. Review `tier0`, `tier1`, `mathlib`,
`filesystem`, `engine`, `materialsystem` and `shaderapidx9` availability.
Start with a small static module before attempting an engine boot.

Initial review of SDK `src/public/tier0/platform.h`:

- ARM architecture detection exists, but the initial 64-bit macro checks x86_64
  and Windows 64-bit. iOS ARM64 needs a consistent pointer-width audit.
- POSIX / OSX selection does not model iOS explicitly. macOS APIs cannot all be
  enabled on iOS simply by defining OSX.
- Desktop DLL loading assumptions need static linking / interface registration.
- Source rendering interfaces and shaders require separate integration work.

## Next milestones

1. Select and pin upstream; compile a real shared Source module on iOS ARM64.
2. Static module registry, platform services, file access and module startup.
3. Engine initialization and clean shutdown without a game client.
4. Material / renderer integration, generated test scene and camera input.
5. Audio, input and optional user-provided game resources.

Track code compiled, simulator behavior and physical iPhone behavior separately.
Simulator graphics does not prove correctness or performance on the iPhone 16e.

## Device test for milestone 0

After externally signing and installing the IPA:

1. Launch: expect a colored triangle and `C++ host + Metal ready`.
2. Rotate the device: geometry should retain its proportions.
3. Background for 20 seconds, then reopen: drawing should resume.
4. Tap **Share diagnostic log**, and send the log with the iOS version and any
   crash details. The log should contain pause/resume entries.

Logs are overwritten on launch; export the current session before restarting.
