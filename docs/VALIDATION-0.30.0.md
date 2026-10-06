# 0.30.0 / build 33 — MDL skin families

~86% минимального prototype, не полного Source/CS:S.
Native проверки прошли; iPhone ARM64 build / simulator GPU ожидаются в CI.

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

Ожидается 2 * 108 startup + BBOX 3 + PHY 3 + HDR 1 + materials 3 +
skin 3 + prop skins 3 + runtime 1 = 233 PASS, GPU revision 9,
MDL48/external ANI, HDR/PHY, pause/resume, content mount/unmount.
Downloaded verifier требует `--require-skins` и прежние contracts.

Реальные игровые skin families и физический iPhone этой версией пока не проверены.
