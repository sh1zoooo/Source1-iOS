# 0.16.0 / build 19 — first BSP VMT/VTF material

This milestone resolves the first BSP surface material instead of assigning
preview UVs from the dominant face axis. The bounded map reader now validates
TEXINFO, TEXDATA, TEXDATA_STRING_TABLE and TEXDATA_STRING_DATA lump size,
version, indices, dimensions, finite texture vectors and terminated safe names.
It applies Source texinfo vectors to every generated vertex, resolves a
LightmappedGeneric VMT `$basetexture`, decodes its VTF and stages it for Metal.

The built-in BSP uses `debug/debugempty` and a generated brick/mortar VTF so
the render result is visually distinct from the old checker fallback. Map
texture uploads are revision based; a rejected map cannot replace active pixels.
Missing or unsupported optional material files retain geometry with the bounded
checker fallback.

Local Debug runtime contracts pass with 68 startup checks. Tests cover BSP
material pixels, importing the same BSP through the GAME path, revision advance
on successful load and preservation across a rejected load. Simulator smoke
expects 137 PASS, BSP and studio texture upload, GPU completion and lifecycle.

Boundaries: this is one BSP base-texture slot shared across the preview. It does
not yet batch multiple surface materials, evaluate proxies, render lightmaps,
normal/specular maps, alpha or use Source's graphical materialsystem.

Run 37352860212 passed Linux runtime contracts and built both iPhone/simulator
apps. The simulator log itself reached 68 PASS, GPU completion and runtime
contracts PASS. The workflow was nevertheless marked failed because the smoke
script still waited for the obsolete pre-0.16 log phrase
`Source VTF preview texture uploaded to Metal` until timeout. Version 0.17
updates that assertion to the new BSP upload phrase; this 0.16 run is not called
a successful smoke run.
