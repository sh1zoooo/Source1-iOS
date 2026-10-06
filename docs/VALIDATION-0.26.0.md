# 0.26.0 / build 29 — SOLID_BBOX static props

Оценка: ~78% минимального демонстрационного этапа, не полного Source или CS:S.
Статус: нативные проверки прошли; iOS ARM64 / simulator / Metal ожидаются в Actions.

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

Ожидается 94 стартовых PASS; simulator повторяет их, проверяет три live prop
collision contracts после reset и runtime contract: всего 192 PASS.
Независимый artifact verifier требует IPA version/build/SHA256,
map revision 6 на GPU, collision markers, MDL48/ANI и pause/resume.

Полный graphical materialsystem, игровой host/client/server, сеть, звук и
реальные CS:S assets ещё не проверены. Preview не является запущенной CS:S.
