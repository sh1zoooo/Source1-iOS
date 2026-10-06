# 0.23.0 / build 26 — VMT Patch base textures

BSP and studio preview materials share a bounded VMT base-texture resolver.
Patch includes are normalized from materials/*.vmt (including backslash
separators), reject unsafe paths/cycles, and allow at most ten files of 64 KiB
with bounded KeyValues nesting. KeyValues #include/#base macros are rejected
before parsing, including quoted/case variants. The base shader must be one of
LightmappedGeneric, VertexLitGeneric or UnlitGeneric.

The resolver follows original CMaterial ApplyPatchKeyValues/recursive patch
accumulation for $basetexture: insert overwrites/adds the key, replace changes
only an existing key, and deeper patches override collected outer patch values.
Other shader parameters, proxies, fallbacks and rendering effects remain
unsupported. Individual missing/unsupported materials use the preview fallback.

The blue BSP fixture now uses a real Patch include/replace to resolve its VTF.
Startup verifies nested patch precedence, insert/replace key existence and
cycle/unsafe/include-macro rejection. Native contracts verify a studio texture
through a Patch and cyclic fallback without losing model geometry.

Local original-engine Debug contracts passed; startup has 83 checks. Simulator
smoke expects 167 PASS across two sets and runtime checks, updated GPU scene,
spawn camera, imported content, external ANI and pause/resume. Actions pending.

CoreSimulator boot commands now have a seven-minute limit, ordinary simctl
commands two minutes and shutdown thirty seconds. App startup still has its
separate sixty-second log deadline. Command names are printed before execution
so a runner failure can be distinguished from an app failure in job logs.
