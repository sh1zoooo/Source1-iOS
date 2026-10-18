# Arm ASTC decompression

Source: https://github.com/ARM-software/astc-encoder
Version: 5.0.0, commit `2ff200e08c98a61775351687ea2b5d38e455ad8b`.
License: Apache-2.0, included as LICENSE.txt and in source file notices.

Unmodified library sources and headers from Source/astcenc*, excluding command
line tooling. Source1 builds with ASTCENC_DECOMPRESS_ONLY; no encoder or CLI is
included. MobileASTC.cpp provides byte-length checks, thread-local codec contexts
and the BGRA output swizzle. Native GLES ASTC uploads bypass this fallback.
