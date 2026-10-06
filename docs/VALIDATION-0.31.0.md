# 0.31.0 / build 34 — ClientMod VPK и реальная MDL48

~89% минимального prototype, не полного Source/CS:S.
Native и реальный ClientMod cache probes прошли; iPhone ARM64 build / simulator
GPU ожидаются в CI.

## Реализация

- Рекурсивно находятся до 32 `_dir.vpk` в подключаемой content-папке.
- До оригинального `CPackedStore` проверяются VPK v1/v2 signature/header,
  directory tree, entry terminators, preload/embedded ranges и все referenced
  chunks. Лимиты: directory 64 MiB, 512 chunks, chunk 512 MiB, total 8 GiB.
- По unmount удаляются loose и VPK search paths. Malformed directory, missing
  chunk, symlink, traversal и legacy ZIP не попадают в Source parser.
- Все существующие `cm`, `cstrike_clientmod`, `cstrike`, `hl2`, `platform`
  подключаются в определённом порядке; game folder больше не обрывает поиск.
- MDL animation record с bone 255 распознаётся как штатный empty/bind-pose
  sentinel. Windows `\\` в MDL material directories и VMT `$basetexture`
  нормализуются до безопасного относительного пути.

## Проверено локально

- Genuine Source native harness: returncode 0, `Runtime contracts passed`, no
  FAIL. Вложенный VPK v2 читается через GAME и полностью удаляется при unmount;
  повреждённая signature отклоняется до mount.
- Clientmod Rec1.4 RAR: 3,071,006,460 bytes; SHA256
  `75b56b7eaf6b495b1234b3de2aaf7a3cf5eee3b590223fcfac88627a31aac973`.
  969 entries, 31 BSP (18 version 20, 13 version 19), 48 VPK files. В верхнем
  архиве не найдено APK, SO или DLL.
- Подключены реальные `cm`, `cstrike`, `hl2`, `platform`: 9 directory VPK,
  38 chunk files, 6,091,595,943 bytes. Через оригинальную Source filesystem
  прочитаны RIFF WAV, VMT, VTF, MDL, VVD, DX90.VTX и PHY.
- `models/weapons/w_eq_molotov_thrown.mdl`: MDL48, 605 source vertices,
  780 triangles, 2 meshes, one-bone `@idle` bind-pose clip. Обе VMT/VTF
  textures декодированы из cache как 512×512 и собраны в 2-slot atlas.
- `maps/awp_lego_2.bsp` загружена прямо из cache: 1,184 preview triangles,
  9 material slots, 449 lightmapped faces, 36 spawn candidates. Команда
  `source_clientmod_demo` загружает эту карту и textured molotov model.
- Пять static props карты пока пропущены: две используемые CSS-модели имеют
  старый неподдержанный MDL layout. Это следующий известный разрыв форматов.

## Границы

- RAR требуется распаковать снаружи приложения в `Documents/Source1IOS/content`.
- Проверка одной реальной модели не означает совместимость всех моделей/карт.
- Game DLL/client DLL, оружие, HUD, правила раунда, networking и полный Source
  shader backend не реализованы. Исполняемый Android-код на iOS не используется.

Ожидается 2 * 111 startup + VPK content 1 + BBOX 3 + PHY 3 + HDR 1 +
materials 3 + skin 3 + prop skins 3 + runtime 1 = 240 PASS, GPU revision 9.
