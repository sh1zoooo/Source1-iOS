# 0.19.0 / build 22 — bounded external ANI blocks

Simple animation descriptors can now resolve their external ANI companion.
The parser validates the MDL block table (up to 1024 blocks), selected index,
ANI file limit (32 MiB), datastart/dataend, and each track/RLE read within the
selected block. External animindex is block-relative, including index zero;
embedded animindex remains relative to its descriptor, matching upstream
mstudioanimdesc_t::pAnimBlock. File paths are checked before filesystem access.

An absent ANI retains static geometry and any supported embedded clips. An
existing empty, oversized or malformed ANI rejects the model load before scene
or texture revisions change. Parsing occurs before committing visible geometry.

The app generates a separate external probe MDL/VVD/VTX/ANI. Simulator smoke
loads and plays it after the repeated startup checks, then checks the lightmap
and model texture uploads, GPU completion and pause/resume. Native integration
checks visible vertex motion, malformed file preservation, missing-file bind
geometry and recovery. Startup has 76 checks; smoke expects 153 PASS.

A separate deterministic mutation test covers 6000 MDL/VVD/VTX/ANI truncations
and bit flips. SourceStudio.cpp and the harness use AddressSanitizer on Linux;
upstream static libraries are not instrumented. Leak checking is disabled.
Accepted models must sample/skin to finite vertices. This is a bounds probe,
not exhaustive format coverage or proof of leak freedom.
Local seed 0x510519: 2549 accepted, 3451 rejected, no ASan bounds report.
Local integration runtime also passed all contracts with 76 startup checks.

Limits: unsectioned raw Quaternion48/64/Vector48 and RLE tracks, at most 128
clips, 2048 frames per clip and 262144 cached bone poses. Sectioned blocks,
frame-animation, delta, IK, local hierarchy, virtual/included models, sequence
blending, bodygroups and full material skins remain pending. This supports a
subset of external animations, not arbitrary CS:S character sequences.
