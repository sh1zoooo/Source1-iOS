# 0.27.0 / build 30 — bounded static PHY

Оценка ~80% относится к минимальному prototype, не к полному CS:S.
Нативные проверки, ARM64 build и simulator GPU прошли; артефакт проверен отдельно.

## Реализация и границы

- Один static solid в PHY, checksum MDL; file 16 MiB, solid 4 MiB,
  keydata 1 MiB. VPHY version 0x100 / polygon type / no axis map и
  little-endian IVPS compact surfaces. MOPP, packed/Xbox/old-zero-signature
  и multiple skeletal solids не поддерживаются.
- Бounded дерево: до 4096 nodes, depth 64, forward child offsets,
  повторные nodes/terminal ledges отвергаются. Terminal ledge header,
  triangle indices, reciprocal edge offsets, point ranges и finite bounds
  проверяются memcpy-чтением, без pointer casts.
- До 128 convex pieces / 2048 referenced points на piece. IVP metres x,-z,y
  преобразуются в Source inches x,y,z. Используется default scale .0254.
- Imported topology/pointers никогда не попадают в VCollideLoad или
  UnserializeCollide. Original ConvexFromVerts/ConvertConvexToCollide
  пересобирают каждую terminal convex piece отдельно; объёмность points
  проверяется до builder. Это сохраняет convex decomposition и зазоры,
  но не binary identity, internal collision maps и surface material IDs.
- Physics object и camera swept hull используют PHY shape с origin/angles;
  uniform scale применяется к points. Cache по модели/scale; до 512 objects,
  262144 cached builder points, общий companion-read budget 64 MiB.
- Missing PHY = visual prop + warning, без ложной BBOX подмены. Present
  malformed/unsupported PHY = отказ загрузки карты, previous scene retained.
  Reset пересоздаёт objects, сохраняя shapes; RAII освобождает их после objects.

## Проверено до CI

- Genuine fixture записан original CollideWrite из studio tapered geometry,
  прочитан новым parser, пересобран original builder.
- Native runtime returncode 0 / Runtime contracts passed / no FAIL.
- Четыре новых startup checks: VPHY points, exact ray/scaling, malformed
  headers/offsets/edges/nonfinite points, separate pieces with empty gap.
- Live rotated PHY objects: луч проходит мимо верхней части при x=10,z=60,
  но проходит через модель при x=0; bounding box перекрывала бы оба луча.
- Camera blocked without tunneling; tests повторены после physics reset.
- Corrupt checksum и truncated imported PHY не заменяют scene/revision.
  Missing PHY и SOLID_NONE сохраняют visual geometry без fake collisions.
- ASan/UBSan pure parser: 20 000 deterministic mutations, 2571 accepted,
  17429 rejected; failure сохраняет previous output. Leak detection off
  из-за среды. Legacy libraries/builder не инструментированы этим тестом.
- Python syntax / mocked launch-timeout diagnostics / diff whitespace passed.

## Независимая проверка CI и артефакта

- Functional commit: `ab13cc4ba78f5664af60a076e2fb52c45ccaddf6`.
- [Actions run 37456745435](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37456745435):
  оба jobs success, первая попытка. Linux 5/5 CTest suites, runtime 5.91 s.
- Artifact `11410590895`, скачан после успешного прогона.
- IPA 0.27.0 / build 30, Mach-O 64-bit arm64, bundle identifier проверен.
- IPA SHA256: `45bb044ba3313c8c5a724cde89bd6b88f5444ef2f930052de5c92bd16fbe3275`;
  совпадает с manifest.
- 98 startup PASS; simulator repeat + BBOX 3 + PHY 3 + runtime 1:
  203 PASS, no FAIL. GPU map revision 7, MDL48/external ANI,
  texture/lightmap upload, content mount/unmount и pause/resume подтверждены.
- Все три live PHY checks прошли после physics reset: objects, tapered
  shape отличается от BBOX, swept camera blocks without tunneling.
- Screenshot PNG 1170x2532 визуально проверен: HUD ~80% / 98 PASS,
  два полосатых props, модель, световая карта и текстуры комнаты.
  Сам кадр не доказывает коллизии: доказательство — отдельные runtime checks.
- Verifier с `--require-phy` отвергает старый 0.26 artifact без этих checks.
- Физический iPhone пользователя пока подтверждает 0.26; новая версия
  0.27 проверена в simulator, без запроса дополнительной диагностики.

Ни реальные CS:S PHY assets, ни полный game/client host пока не проверены.
Renderer остаётся Metal preview adapter, не оригинальный Source shader backend.
