# 0.26.0 / build 29 — SOLID_BBOX static props

Оценка: ~78% минимального демонстрационного этапа, не полного Source или CS:S.
Статус: нативные проверки, Linux CI, iPhone ARM64 build и simulator Metal прошли.
Присланный пользователем лог подтверждает 94 стартовых PASS на физическом
iPhone 16e / A18 / iOS 18.6.2: cube/sphere rest, два SOLID_BBOX objects,
MDL48, texture/lightmap upload, GPU completion revision 3, смена ориентации.
Live camera command и pause/resume в этом логе не выполнялись.

## Изменение

Читается solid mode из `sprp` v4–11. Для `SOLID_BBOX` создаются owned convex
boxes через оригинальные `BBoxToConvex` / `ConvertConvexToCollide`, затем
`CreatePolyObjectStatic` с origin, angles и scaled MDL hull bounds.
Камера использует world AABB transformed render bounds и оригинальный
vphysics swept hull; это соответствует различию physics/render bounds
в Source static prop path. Формы освобождаются после physics objects и
environment, не накапливаются в глобальном `BBoxToCollide` cache.

Загрузка staged: повреждённая карта не заменяет текущую. Reset пересоздаёт
коллайдеры. Ограничение 512 collider instances; bounds проверяются на
конечность, порядок и предел. Degenerate bounds сохраняют видимую модель,
но дают предупреждение и не создают collider.

`SOLID_NONE` визуален. `SOLID_VPHYSICS` визуален с явной диагностикой
неподдержанного PHY; точный collision mesh ещё не загружается. Native PHY
unserializer содержит вложенные смещения без достаточных checks, поэтому
его нельзя напрямую вызвать для импортированных данных.

Для IVP event solver исправлена консервативная верхняя граница скорости:
projected rotation использует `sqrt(1.001 - dot²)`, поэтому сумма исходных
rotation speeds не всегда служит верхней границей. Используется запас
`1.001` и `P_DOUBLE_EPS`; assert не отключается.

## Проверки до публикации

- Нативный runtime integration с оригинальными engine/vphysics/IVP libraries:
  `Runtime contracts passed`, returncode 0, без `: FAIL`.
- Поворот/масштаб AABB, отдельные ray и swept hull, остановка камеры без
  tunneling, повтор после physics reset.
- Куб и сфера падают с z=96 на box высотой 64; после 240 шагов 1/120 s
  центр остаётся между z=70 и z=75. Платформа находится в свободном месте
  BSP, без пересечения с существующими brushes.
- Импортированный invalid solid mode отвергается с сохранением сцены;
  NONE и неподдержанный VPHYSICS не получают ложных BBOX collisions.
- 10 000 static prop mutations под ASan/UBSan: 5 863 accepted / 4 137 rejected.
- 12 000 MDL48/49 mutations под ASan: 5 108 accepted / 6 892 rejected.
  Legacy libraries не инструментированы; leak detection выключен из-за
  ограничений среды. Это не ASan-проверка всего движка.
- Подготовка pinned upstream и Python syntax checks прошли.
- Mocked launch-timeout test сохраняет diagnostic log; не является GPU test.

Подтверждены 94 стартовых PASS; simulator повторяет их, проверяет три live prop
collision contracts после reset и runtime contract: всего 192 PASS, без FAIL.
Независимый artifact verifier требует IPA version/build/SHA256,
map revision 6 на GPU, collision markers, MDL48/ANI и pause/resume.

## Независимая проверка Actions и артефакта

- [Commit](https://github.com/sh1zoooo/Source1-iOS/commit/29e1c58546d3338b0dcb60f7c722919b5a1d5d0f).
- [Actions run 37433223388](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37433223388): обе jobs success, первая попытка.
- Linux: 4/4 CTest passed, 0 failed; runtime 5.68 s, общий CTest 6.85 s.
- [Скачанный artifact 11397874122](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37433223388/artifacts/11397874122).
- IPA plist: `0.26.0`, build `29`; executable — Mach-O 64-bit arm64.
- IPA SHA256: `5da4df95f5beb38b5dd7a8da46c16c74370ef628a5074db5040c83b00f6778bf`.
  Независимый verifier подтвердил совпадение с SHA256SUMS.
- Runtime log: 192 PASS, GPU completion map revision 6, reset и все три
  live prop checks, внешний MDL48/ANI, content mount/unmount, pause/resume.
- PNG 1170×2532 осмотрен: оба striped static props видны впереди, animated
  studio model позади, textured BSP/lightmaps и spheres; HUD ~78%, 94 PASS.
- Артефакт 0.25 с требованием prop collision markers корректно отвергнут.

Этот отчёт добавлен отдельным documentation commit после скачивания и
проверки функционального build; код движка после CI не изменялся.

Полный graphical materialsystem, игровой host/client/server, сеть, звук и
реальные CS:S assets ещё не проверены. Preview не является запущенной CS:S.
