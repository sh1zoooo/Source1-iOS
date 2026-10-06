# 0.20.0 / build 23 — material grid and updated-scene GPU verification

The BSP atlas now supports up to 512 distinct safe material names, with 16
columns of 64x64 tiles and up to 32 rows (8 MiB RGBA). Duplicate names share
slots. More than 512 names rejects the map before replacing the current scene;
the old misleading substitution of the first material after slot 16 is removed.
Missing individual VMT/VTF files still use the checker fallback in their slot.
Metal wraps within a tile, then addresses its column and row independently.

The new `source_bsp_materials` fixture uses 17 distinct VMT names referring to
the two generated base textures. Its atlas is 1024x128, with slot 16 on the
second row. Native contracts check pixels, dimensions, emitted material IDs,
import/reset and rejection preservation. It remains a fixture, not a CS:S map.

Smoke first repeats 77 startup checks, loads terrain, then loads the material
grid and plays the external ANI model. It requires 155 PASS and completion of
map revision 4 on the GPU, not merely the first pre-update frame. Completion
logging is dispatched onto the main queue alongside runtime frame/file writes;
new map revisions and foreground resume request a completion check.

Local Debug runtime passed all contracts. The external animation bounds probe
also passed 6000 AddressSanitizer mutations (2549 accepted, 3451 rejected);
see the scope and exclusions in the 0.19 report.

Limits remain: base textures are downsampled to 64x64; lightmaps use a separate
bounded LDR atlas and the first static style. This is a Metal preview adapter,
not the original graphical shaderapi/materialsystem. Full maps, PVS, material
shader features, sequence blending, audio and game modules still need work.
