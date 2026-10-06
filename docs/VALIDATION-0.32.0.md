# 0.32.0 / build 35 — CS:S MDL44 static props

~91% минимального prototype, не полного Source/CS:S.

## Реализация

- Bounded studio parser принимает MDL44 вместе с MDL48/49. Используемые поля
  header, bones, materials, models и animation descriptors имеют проверяемый
  совместимый layout; все offsets/counts по-прежнему проверяются до чтения.
- MDL44 проходит тот же VVD4/VTX7 geometry, material, skin и animation путь,
  что и новые модели. Неизвестная версия по-прежнему отклоняется.
- Simulator smoke требует отдельный MDL44 contract; verifier не принимает
  артефакт этапа без этого свидетельства.

## Проверено локально

- Native genuine-Source harness: return code 0, `Runtime contracts passed`,
  no FAIL; MDL44 geometry/embedded animation contract проходит пять циклов.
- ASan/UBSan studio mutation test: 36 000 deterministic MDL44/48/49 cases,
  15 998 accepted и 20 002 безопасно rejected, без sanitizer errors.
- Реальный Clientmod Rec1.4 подключён через оригинальный `CPackedStore`.
  `maps/awp_lego_2.bsp`: 1 184 world triangles, 9 material slots,
  449 lightmapped faces и 36 spawn candidates.
- После MDL44 загружены все 5 ladder static props: 2 model types,
  2 472 prop triangles, 0 visual skips. VTF 64×256 и 64×512 декодированы.
- Для пяти SOLID_VPHYSICS ladder props в кеше нет требуемых `.phy`; loader
  сообщает 5 unsupported collision objects и не создаёт ложный BBOX.

## Границы

- Это совместимость одного старого studio layout, а не доказательство загрузки
  каждой CS:S/ClientMod модели.
- Game client/server, оружие, HUD, правила, networking и полный Source shader
  backend не реализованы. Android APK/SO на iOS не исполняются.

Ожидается 2 × 112 startup + VPK content 1 + BBOX 3 + PHY 3 + HDR 1 +
materials 3 + skin 3 + prop skins 3 + runtime 1 = 242 PASS, GPU revision 9.
