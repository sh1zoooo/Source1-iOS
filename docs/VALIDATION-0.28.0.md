# 0.28.0 / build 31 — HDR-only BSP preview

~82% минимального prototype, не полного Source/CS:S.
Native проверки, iPhone ARM64 build / simulator GPU прошли.

## Реализация

- Prefer LDR lighting. Если его нет и HDR lighting есть, используются
  `LUMP_LIGHTING_HDR` и `LUMP_FACES_HDR`, либо общие `LUMP_FACES`, когда HDR
  face records отсутствуют. HDR faces не смешиваются с LDR samples.
- HDR faces/samples проходят те же ограничения, что LDR: 16 MiB/lump,
  record stride/version, file ranges, общий decoded budget 64 MiB и bounded
  Source streaming LZMA decoder. Непроверенный compressed HDR lump не
  передаётся legacy Uncompress.
- Каждый face lightofs/luxel extent/styles/bump range проверяется относительно
  выбранного lighting lump. Atlas 1024x1024, tile border padding.
- HDR RGBExp32 channel / 255 * 2^exponent преобразуется `x/(1+x)`, затем
  gamma 1/2.2 в 8-bit atlas. Фиксированная экспозиция, без автоадаптации.
  Это approximate preview, не оригинальная Source HDR shader pipeline.
- Повреждённая карта не заменяет текущую сцену, текстуры или revision.
- HDR demo содержит два SOLID_VPHYSICS props; оригинальные vphysics формы
  и swept camera collisions сохраняются. MDL48/49 и ANI не изменены.

## Проверено нативно

- Original Source library harness: returncode 0, Runtime contracts passed,
  no `: FAIL`; новые startup checks прошли при повторных стартах.
- HDR-only selection, shared faces fallback, LDR preference.
- Значения выше 1 различимы после mapping; zero, минимальный/максимальный
  RGBExp32 exponent, RGB/alpha и tile borders проверены.
- Invalid HDR face stride, sample version/decoded size и light offsets
  отвергаются; runtime odd lighting length / out-of-range face lightofs
  сохраняют текущие texture pixels и revision.
- Реальный HDR-only fixture load: 42 lightmapped faces, 84 triangles,
  два PHY props. Новый atlas отличается от LDR; PHY checks после reset PASS.
- Python syntax, mocked launch-timeout diagnostics и diff whitespace PASS.

## Независимая проверка CI и скачанного артефакта

- Functional commit: `c6f86653b9b5c03f62a4c1af3214e32762d78128`.
- [Actions 37469173881](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37469173881):
  оба jobs success с первой попытки. Linux 5/5 suites, runtime 6.26 s.
- Artifact `11416920897`; plist 0.28.0/build 31, bundle ID,
  Mach-O 64-bit arm64 проверены отдельно после скачивания.
- IPA SHA256 `b779153f795d86f0d9919997db92d490edac34c11ac87251718a72339d70a707`
  совпадает с manifest.
- Simulator: 2 * 101 startup + BBOX 3 + PHY 3 + HDR 1 + runtime 1 =
  210 PASS, no FAIL, GPU map revision 8, MDL48/external ANI,
  content mount/unmount, pause/resume.
- Verifier с `--require-hdr` и прежними contracts прошёл. Старый 0.27
  artifact отвергается, когда запрошено подтверждение HDR.
- PNG 1170x2532 визуально проверен: HUD ~82% / 101 PASS,
  текстурированная комната, две PHY-модели, анимированная MDL48 и spheres.

Физический iPhone и реальные игровые HDR BSP этой версией ещё не проверены.
Animated styles, bump-lighting, HDR cubemaps/sky, full game DLL остаются впереди.
