# 0.30.0 / build 33 — MDL skin families

~86% минимального prototype, не полного Source/CS:S.
Native проверки, iPhone ARM64 build / simulator GPU прошли.

## Реализация и границы

- До 256 skin families / 256 references на family. Весь сериализованный
  table проверяется, включая невыбранные families; индексы texture slots
  должны быть в диапазоне MDL materials. Без таблицы разрешён только skin 0.
- StudioVertex сохраняет исходный mesh material reference отдельно от
  выбранного texture slot. Family можно менять повторно без чтения файлов,
  накопительного remap, перестроения geometry или загрузки atlas.
- Select validates all vertex references before commit; bad family/reference
  не меняет assignments/activeSkin. Console принимает только decimal 0..255.
- Model preview сохраняет текущие posed vertices, camera, animation time и
  texture revision. Последующие CPU skinning frames сохраняют new assignments.
- Static props больше не пропускаются при skin != 0. Один model cache и
  полный texture atlas используются несколькими instances, каждому применяется
  свой family. Bounds/scale/angles/PHY и physics reset не зависят от skin.
- Invalid skin существующей модели отклоняет карту без замены текущей сцены.
- Это material family, не выбор bodygroups, sequence blends или game player
  teams. Full Source shader backend и game DLL остаются впереди.

## Проверено нативно

- Genuine Source libraries harness returncode 0 / Runtime contracts passed,
  no FAIL; starts/shutdown/restarts и lifecycle contracts прошли.
- Три новых startup checks: family 1/0 remap без geometry changes;
  invalid family / vertex reference / unused family index atomic rejection;
  тот же sampled bone pose после смены family.
- Live posed model: позиции всех projected vertices и camera неизменны при
  смене family; material codes меняются, atlas pixels/revision сохраняются.
  Negative, nondecimal, отсутствующее family и overflow-like input отвергаются.
- BSP fixture: две instances одной MDL48, skin 0/1, разные material assignments,
  два оригинальных PHY objects; checks повторены после physics reset.
  Invalid prop skin сохраняет texture pixels и scene revision.
- ASan: 24 000 deterministic MDL48/49 + embedded/external ANI mutations,
  single/multiple materials. У каждого accepted model выбирается valid family,
  проверяется invalid-family rejection и finite/material bounds skinning.
  10707 accepted, 13293 rejected. Leak detection отключён из-за среды;
  отдельный parser test не инструментирует legacy libraries, не доказывает
  отсутствие утечек и не является exhaustive fuzzing.
- Lifecycle tests сравнивают frame/time относительно начала pause, чтобы
  дополнительный animation frame не зависел от hard-coded frame totals.
- Python syntax, mocked launch diagnostics и diff whitespace PASS.

## Проверенный CI и скачанный артефакт

- Functional commit: `02e4207dfa9fc4967de59b293fec1cf0bcd21f3c`.
- [Actions 37514952479](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37514952479):
  Linux и iOS jobs success; Linux 5/5 CTest, runtime 5.56 s,
  studio mutation suite 2.89 s; total 9.05 s.
- Artifact `11436719432` скачан и проверен отдельно: plist
  0.30.0/build 33, bundle identifier и Mach-O 64-bit arm64 подтверждены.
- IPA SHA256 `aeb444b8e480e0b331df42bb8693e74d51dbc4f606091732d5e05b2a588da2b2`
  совпадает с manifest.
- 2 * 108 startup + BBOX 3 + PHY 3 + HDR 1 + materials 3 +
  skin 3 + prop skins 3 + runtime 1 = 233 PASS, no FAIL; GPU revision 9,
  MDL48/external ANI, HDR/PHY, pause/resume, content mount/unmount.
- Downloaded verifier с `--require-skins` и прежними contracts PASS.
  Старый 0.29 artifact не принимается как evidence skin families.
- PNG 1170x2532 визуально проверен: HUD ~86% / 108 PASS; две статические
  instances имеют разные texture assignments (синяя и полосатая front face),
  центральная animated MDL skin 1, HDR room и PHY spheres сохраняются.

Реальные игровые skin families и физический iPhone этой версией пока не проверены.
