# 0.29.0 / build 32 — per-mesh studio materials

~84% минимального prototype, не полного Source/CS:S.
Native проверки прошли; ARM64 iPhone / simulator GPU ожидаются в CI.

## Реализация и границы

- Все texture records MDL (до 256) и до 32 CD search directories читаются
  с проверкой относительных offsets и bounded NUL strings.
- Skin table: до 256 references / 256 families, проверяется полный диапазон
  и каждый texture index. Mesh.material проверяется как skin ref либо прямой
  texture index при отсутствии таблицы. Используется только family 0.
- Material index сохраняется в StudioVertex после VVD/VTX remap, triangle
  strips и CPU weighted skinning; flat GPU index одинаков у трёх вершин face.
- Несколько VMT/VTF модели превращаются в двумерный 64px/tile atlas, до
  16 columns. Metal использует отдельный model atlas, repeat внутри tile и
  half-texel inset. Single-slot rendering/fallback сохраняет прежний путь.
- Static props добавляют каждый material в общий world atlas (512 slots),
  применяют remap каждой вершины, сохраняют rotation/scale и PHY shapes.
- Пропавший optional material получает checker tile; malformed model indices
  отвергаются до замены модели. Full shader parameters, bump/specular/alpha,
  LOD material replacements, выбор skins/bodygroups не реализованы.
- Demo: MDL48, 2 meshes / 2 materials, skin table меняет порядок 1,0;
  embedded bend animation, HDR/PHY room остаётся активной.

## Проверено нативно

- Genuine Source libraries harness: returncode 0, Runtime contracts passed,
  no FAIL, повторные starts/shutdown.
- Четыре startup checks: per-mesh/default skin remap; invalid skin offset,
  negative texture index и mesh material вне таблицы; two VMT/VTF atlas tiles;
  skinning сохраняет material IDs.
- Live model: обе GPU codes -1/-2, atlas 128x64 с различными tiles;
  skinning после animation sampling сохраняет slots.
- Live BSP props с заменённой fixture-моделью: оба slots 2/3 добавлены к
  world atlas (256x64), исходные model files затем восстановлены.
- Single-material fallback, VMT Patch, malformed VTF и прежние runtime
  contracts прошли без изменения поведения.
- Общие fixture checks после смены карты/модели выполняются в отдельном
  SourceMap preview; native HDR/multi-material scene сохраняет camera,
  texture pixels и обе texture revisions. Это устраняет ложный FAIL от
  fixture assertions, ожидающих исходную карту и single-material MDL.
- ASan studio parser: 24 000 deterministic mutations MDL48/49,
  embedded/external ANI, single/multiple materials; 10758 accepted,
  13242 rejected, golden fixtures сначала проверены. Проверяются finite
  skinned outputs и material bounds. LeakSanitizer отключён из-за среды;
  это bounds probe, не exhaustive fuzzing или проверка утечек. Legacy
  libraries не инструментированы этим отдельным тестом.
- Python syntax, mocked launch diagnostics и diff whitespace PASS.

Ожидается 2 * 105 startup + BBOX 3 + PHY 3 + HDR 1 + materials 3 + runtime 1
= 221 PASS, GPU map revision 8, texture upload, pause/resume, MDL48/ANI.
Downloaded verifier получает `--require-model-materials` и прежние flags.

Реальные модели CS:S и физический iPhone для этой версии ещё не проверены.
