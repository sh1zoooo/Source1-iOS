# Source1-iOS

Экспериментальный перенос Source 1 на iOS ARM64. Базовые библиотеки Source проверены
на физическом iPhone 16e с iOS 18.6.2. В версии 0.9 оригинальный appframework запускает зависимости, затем вызываются настоящий Host_Init и кадры dedicated-движка в штатном режиме -nogamedll.

**Текущий этап: настоящий dedicated host, загрузка встроенного BSP через engine, камера со столкновениями и сцена vphysics. Изображение выводит наш адаптер Metal. Графический shaderapi Source и игра ещё не запущены.**

## Что реально подключено

540 единиц компиляции (539 из manifest и tool-mode displacement collision) из `tier0`, `tier1`, `mathlib`, `vstdlib`, `filesystem`, `vpklib`
`tier2`, `tier3`, `appframework`, `bitmap`, `engine`, `materialsystem`, `shaderapiempty`, `shaderlib`, `vtf`, `datacache`, `studiorender`, `vphysics` и IVP/Havana исходной базы
[nillerusr/source-engine](https://github.com/nillerusr/source-engine),
зафиксированной на `ed8209cc35c61fbd8ddff8480962a01c981eef2f`.

При запуске приложение получает настоящий `VEngineCvar004` через реестр интерфейсов
Source, выполняет `Connect` / `Init`, запускает `MathLib_Init` и проверяет allocator,
CRC32, bitbuf, KeyValues, матрицы, таймер и ConVar. Результаты попадают в лог.
Встроенный BSP проходит оригинальные `CMapLoadHelper`, `CModelLoader` и `CM_LoadMap`.
Камера использует Source `AngleVectors` и `CM_BoxTrace`; позы физических тел
читаются из vphysics. Триангуляция и вывод через Metal написаны нами.

Поле консоли работает через реальные `CCommand`, `ConCommand` и `ConVar`:

- `source_status` — перечень подключённых библиотек и границы текущего порта.
- `source_selftest` — повторить все 55 проверок Source.
- `source_host_selftest` — настоящий Host_Init, queued loader и idle ticks.
- `source_app_selftest` — проверить фабрику, порядок остановки и откат ошибок.
- `source_assets_selftest` — VTF, data cache, физические коллизии, симуляция и ragdoll-сочленение.
- `source_engine_selftest` — семь проверок оригинального engine.
- `source_fs_selftest` — повторить семь проверок файловой системы.
- `source_bsp_selftest` — BSP, engine brush model, CM collision, VTF и камера.
- `source_bsp_reset` — вернуться к встроенной комнате.
- `source_bsp_terrain` — встроенный пример displacement-рельефа.
- `source_bsp_materials` — встроенный BSP с 17 материалами, включая вторую строку atlas.
- `source_bsp_props` — BSP с двумя статическими MDL-объектами, размещёнными записями `sprp`.
- `source_content_mount cm` — добавить распакованные ресурсы из `Source1IOS/content/cm`.
- `source_content_unmount cm` — отключить эту папку от GAME search paths.
- `source_camera_reset` — вернуть камеру в начальную позицию.
- `source_physics_reset` — заново создать сцену с двумя телами и шарниром.
- `source_physics_impulse` — толкнуть подвижное тело.
- `source_bsp_load maps/name.bsp` — предпросмотр полигонов своего BSP.
- `source_model_load models/name.mdl` — загрузить статическую модель вместе с
  `name.vvd` и `name.dx90.vtx`; `source_model_reset` возвращает встроенный fixture.

Оригинальный `VFileSystem022` подключён к `Documents/Source1IOS/game` с path ID
`GAME` и `DEFAULT_WRITE_PATH`. Каталог доступен через приложение «Файлы».
Самотесты в отдельном каталоге `selftest` проверяют запись, чтение/seek, поиск,
KeyValues на диске, асинхронное чтение и встроенные данные VPK v1/v2.
Пользовательский каталог `game` самотесты не изменяют.

169 единиц компиляции настоящего `engine` связаны в dedicated-конфигурации.
Проверяются реальная фабрика, буфер команд (порядок, кавычки, wait) и пространственный
индекс (область, маски, луч, перемещение и удаление). Пространственные запросы используют настоящий уже инициализированный MDLCache. Настоящий `Host_Init(true)` и `Host_RunFrame` работают в dedicated-режиме без игры. Графический backend материалов, игровые клиент/сервер и звук **ещё не запущены**. Это не готовый запуск HL2/CS:S.

Консоль и файловая система запускаются через настоящий `CAppSystemGroup`:
статические фабрики, `Connect`, `PreInit`, `Init`, обратный `Shutdown` и
`Disconnect`. UIKit управляет кадрами, поэтому используется `Startup`, без
desktop main loop. Исправлены откат частичного запуска и восстановление
родительской фабрики после остановки. Самотесты намеренно вызывают ошибки
двух тестовых систем; сообщения `intentional ... failure` в логе ожидаемы.

**Прогресс: ориентировочно 76% до минимального запуска Source с тестовой картой
и камерой на iPhone.** Это оценка по подсистемам, а не процент исходников.
[Критерий готовности и оставшиеся этапы](docs/PROGRESS.md).

Сеть отключена параметром `-noip`; предупреждение о невозможности dedicated-сервера
ожидаемо: эта сборка проверяет idle host, без прослушивания соединений. Очередь
игровых конфигураций после Host_Init очищается, команды игры не разрешены.

## Скачать IPA

Последняя полностью проверенная сборка: [0.25.0, build 28 — IPA и диагностика](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37425496670/artifacts/11395675357).
90 стартовых проверок в 0.25; симулятор повторяет их и проверяет GPU, terrain,
BSP material grid/lightmaps, static props, MDL48/49 и ANI-анимацию, background/resume (181 PASS).
Версия 0.17 подтверждена пользовательским логом и кадром на iPhone 16e:
69 PASS, загрузка двух BSP-материалов и studio texture, GPU A18, iOS 18.6.2,
смена ориентации. Пользователь ранее подтвердил плавную деформацию модели.
[Отчёт 0.17 и границы проверки](docs/VALIDATION-0.17.0.md).
Enter / Go в консоли теперь выполняет команду и закрывает клавиатуру, как Run.
Этап 0.18/build 21 добавляет LDR lightmaps BSP, правильные UV стен fixture
и ограниченное декодирование сжатых данных материалов/освещения.
[Границы и проверки 0.18](docs/VALIDATION-0.18.0.md).
Этап 0.19/build 22 читает внешние ANI blocks с ограничением файла и каждого
диапазона. Это поддержка простых отдельных animation tracks, а не полный
sequence evaluator. [Проверки и границы ANI](docs/VALIDATION-0.19.0.md).
Этап 0.20/build 23 расширяет atlas до 512 различных материалов и проверяет
завершение обновлённой сцены на GPU. [Проверки 0.20](docs/VALIDATION-0.20.0.md).
Этап 0.21/build 24 подключает распакованные каталоги игровых ресурсов к Source
filesystem. [Размещение ресурсов и команды импорта](docs/CONTENT-IMPORT.md).
Этап 0.22/build 25 выбирает положение камеры из точек появления BSP, включая команды CS.
[Проверки и границы spawn camera](docs/VALIDATION-0.22.0.md).
Этап 0.23/build 26 разрешает VMT Patch include/insert/replace для базовой текстуры.
[Проверки Patch материалов](docs/VALIDATION-0.23.0.md).
Этап 0.24/build 27 отображает статические BSP props через MDL/VVD/VTX и VMT/VTF.
Модели получают положение и поворот из карты; это пока визуальные объекты без PHY-коллизии.
[Проверки и границы static props](docs/VALIDATION-0.24.0.md).
Этап 0.25/build 28 добавляет preview MDL 48 наряду с 49, включая простые встроенные
и внешние ANI tracks. Полный набор игровых sequences пока не поддерживается.
[Проверки MDL48 и портретной сцены](docs/VALIDATION-0.25.0.md).
[Результаты проверок движка](docs/VALIDATION-0.9.1.md).

Откройте [Actions](https://github.com/sh1zoooo/Source1-iOS/actions), выберите успешный
запуск **iOS host build** и скачайте артефакт `Source1IOS-<commit>`.
В ZIP находятся `Source1IOS-unsigned.ipa`, SHA256, лог и скриншот симулятора.
IPA нужно подписать вашим способом установки. Минимальная версия iOS — 17.0.
Сертификаты и секреты для сборки не требуются.

## Сборка

Сначала получите закреплённый submodule, без рекурсивной загрузки его необязательных
зависимостей:

```sh
git submodule update --init --depth 1
git -C third_party/source submodule update --init --depth 1 thirdparty ivp
```

На Mac нужны Xcode с iOS SDK и CMake 3.24+:

```sh
cmake -S . -B build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0
cmake --build build-ios --config Release -- -quiet CODE_SIGNING_ALLOWED=NO
python3 scripts/package_ipa.py build-ios/Release-iphoneos/Source1IOS.app artifacts/Source1IOS-unsigned.ipa
```

Проверки на Linux:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Сборка создаёт копию исходников в build-каталоге, проверяет SHA базы и применяет
явные исправления из `scripts/prepare_source.py`. Сам submodule не изменяется.
Уведомления исходной базы и сторонних библиотек включены в приложение.

[Техническое состояние и следующие этапы](docs/PORTING.md).

## Этапы 0.6–0.7.1

`VMaterialSystem081` работает с оригинальным `shaderapiempty`: это штатный Source
backend без графики. Через `CAppSystemGroup` проходят Connect/Init/Shutdown/Disconnect
материалов, физики, data cache, студийного рендерера, MDLCache и dedicated engine API.
С версии 0.7 вызываются настоящий `Host_Init` и idle-кадры `Host_RunFrame`.
`ModInit` с игровой DLL остаётся впереди. Встроенный brush-мир загружает engine; игровой сервер на нём не запускается.

Проверки этапа 0.6 подтверждают зависимости, чтение/запись VTF с тремя mip-уровнями,
отказ на коротком заголовке, RGBA→BGRA, поиск/блокировку/удаление data cache,
физическую форму бокса, трассировку луча и падение сферы под действием гравитации.
Физика выполняется оригинальным vphysics/IVP. Комнату и физические сферы на экране выводит наш Metal backend.

Общие tier-библиотеки учитывают число подключённых систем. Статические Source команды
восстанавливаются при повторном запуске; desktop globals и studio hooks сведены
к единственным владельцам. Проверка остановки/повторного запуска входит в Actions.

Самотест короткого VTF намеренно вызывает сообщение `Error unserializing VTF file`;
следующий результат должен быть `VTF truncated header rejected: PASS`.

Версия 0.7.1 дополнительно проверяет настоящее ragdoll-сочленение после импульса
и симуляции под гравитацией. Это основа для физического скелета персонажа;
сам персонаж пока не загружается и не отображается. Все 39 проверок версии 0.7.1, настоящий Host_Init и два цикла pause/resume
подтверждены пользовательским логом iPhone 16e. Версия 0.9.1 также подтверждена пользовательским логом: 50 стартовых PASS,
BSP-мир, GPU A18, смена ориентации и два pause/resume.


## BSP, камера и физика (0.9)

При старте вместо куба открывается тестовая комната: геометрический BSP создаётся
в каталоге selftest и читается оригинальным CMapLoadHelper. Треугольники передаются
оригинальному IVP для swept-hull столкновений камеры. VTF checker-текстура проходит
запись/чтение через Source и декодируется для нашего адаптера Metal.

Левая половина экрана: тянуть палец для движения. Правая: тянуть для обзора.
`source_camera_reset` восстанавливает позицию. `source_bsp_selftest` проверяет
геометрию, диапазоны секций, столкновения и проекцию. `source_bsp_load maps/name.bsp`
читает собственный BSP из Documents/Source1IOS/game/maps; при ошибке предыдущая
геометрия сохраняется. Старый ios_rotation_speed остаётся диагностическим ConVar.

Встроенный мир содержит 56 вершин, 42 поверхности, две BSP-области и семь
solid brush-боксов. Он загружен оригинальным engine; CM-трассировки и worldspawn
проверяются отдельно. Две жёлтые сферы связаны настоящим ragdoll-шарниром и
симулируются каждый кадр. Это основа для будущего физического скелета персонажа.

Пользовательские карты пока имеют только preview полигонов с единой тестовой
текстурой и IVP-столкновениями; engine CM-мир остаётся встроенным. Графический
materialsystem Source ещё не адаптирован. BSP-пакеты материалов, PVS,
lightmaps и модели ещё не поддержаны. Внешние .lmp отклоняются.
Лимиты: файл 128 MiB, 100 тысяч треугольников, координаты ±32768.

### Совместимость BSP (0.10.0, build 13)

Предпросмотр поддерживает BSP версий 19–21 и геометрические секции версии 0
(faces — также оригинальной версии 1).
Vertices, edges, surfedges и faces могут быть несжатыми или в Source LZMA:
декодирование выполняет оригинальный CLZMAStream с ограниченными входом и выходом.
Каждая геометрическая секция (сжатая и распакованная) и словарь ограничены 16 MiB.
Проверяются сигнатура, размеры, свойства, полнота декодирования и размер записей.
Сжатые неиспользуемые секции не декодируются и не мешают preview; это не означает
поддержку их содержимого. При отказе лог содержит имя файла, причину и номер секции.

Добавлены проверки независимого LZMA-потока, повреждённых размеров/свойств/данных,
а также runtime-тест равенства геометрии сжатой и несжатой карты и сохранения сцены
после неудачной загрузки. Конкретная пользовательская dust-карта ещё не проверена:
сам BSP не предоставлен. Файл нужен именно `.bsp`, не `.bpz` и не ZIP/RAR.

**Полная CS:S — отдельная цель, не «50% готова».** Предыдущие ~50% относятся к
минимальному engine/viewer-этапу. Для игры остаются оригинальный клиентский host,
графический materialsystem/shader backend, VMT/VTF карты, displacement/PVS/lightmaps,
скелетная анимация MDL, ввод/звук/UI, совместимые client/server игровые модули,
загрузка карты как игрового мира и проверка реального матча. Наличие tier-библиотек
и headless Host_Init не равно готовой CS:S. Игровые ресурсы предоставляет пользователь.

### Displacement preview (0.11.0, build 14)

Source CCoreDispInfo строит полную сетку displacement power 2–4. Геометрия и
metadata проверяются до вызова legacy builder: диапазоны массивов, power, parent
face, start corner, выпуклость/плоскость quad, конечность и пределы чисел.
Дополнительно читаются DISPINFO/DISP_VERTS/DISP_TRIS, включая bounded Source LZMA.
Лимит 16 MiB применяется к каждой из семи используемых геометрических секций.

Камера использует оригинальный Source displacement collision tree вместе с IVP
для обычных полигонов. Отдельная tool-mode копия Source collision tree с переименованными
экспортами использует освобождаемую память, не engine hunk: импорт/ошибка/reset не
должны накапливать деревья до конца Host_Shutdown. Это не подмена алгоритма коллизий.
Луч и swept hull проверяются на вершине холма высотой 32 units.

Пока нет displacement LOD/seam stitching, blend-материалов, lightmaps и поведения
поверхностных physics-флагов. Коллизии динамических IVP-тел на рельефе не подтверждены:
диагностический IVP point/hull trace по отдельному slope soup не дал правильной
высоты; камера использует специализированный путь Source. Также остаются IVP
contact-rescue warnings в длительных прогонах и рост RSS после restart.

### Static studio model preview (0.12.0, build 15)

Загрузчик читает согласованную тройку Source studio model v49: `.mdl`, `.vvd` v4
и `.dx90.vtx` v7. Проверяются checksum, иерархия body/model/mesh/strip group,
все смещения и индексы, конечность вершин и лимиты. Поддержаны VVD fixup-таблицы,
VTX triangle lists/strips и v49 extended strip layout. LOD 0 превращается в
треугольники и выводится существующим Metal-адаптером. Настоящий MDLCache отдельно
открывает тот же заголовок fixture-модели.

Это позволяет показать статическую геометрию совместимой модели персонажа, оружия
или prop, если пользователь положит все три файла в `Documents/Source1IOS/game/models`.
Пока не применяются кости, skinning, анимации, bodygroup/skin selection, VMT/VTF
материалы, прозрачность и физическая collision-модель. Модель рисуется в фиксированной
точке тестовой комнаты единой checker-текстурой; это ещё не Source studiorender.

### Weighted studio pose (0.13.0, build 16)

MDL-кости, их parent/local bind pose/poseToBone и VVD-веса теперь читаются и
проверяются. CPU skinning через mathlib Source применяет до трёх влияний к вершине
и нормали. Встроенная двухкостная модель сгибается каждый активный кадр; пауза
останавливает позу. Это позволяет в дальнейшем сгибать конечности персонажа.
Пока движение fixture процедурное: игровые animation sequences, flexes и
физический ragdoll-скелет ещё не подключены. Импортированная модель использует bind pose.

### Embedded MDL animation (0.14.0, build 17)

Fixture содержит настоящий встроенный MDL RLE-трек. Загрузчик проверяет и читает
локальные RLE rotation/position и raw Quaternion48/64/Vector48, после чего кадры
интерполируются mathlib Source и передаются skinning. Первый поддержанный clip
модели запускается автоматически. `source_anim_play 0` выбирает clip,
`source_anim_pause`/`source_anim_resume` управляют воспроизведением.
Внешние ANI, section/frame-animation, delta, IK и blend sequences ещё не поддержаны.
Это базовое воспроизведение отдельных треков, без полной игровой animation state machine.

### Studio base texture (0.15.0, build 18)

Первый материал MDL ищется в CD texture directories. Оригинальный KeyValues читает
VMT `$basetexture` для VertexLitGeneric/UnlitGeneric; оригинальная VTF библиотека
декодирует базовую текстуру для отдельного слота Metal. Fixture имеет оранжевые и
голубые полосы; BSP продолжает использовать checker. После загрузки другой модели
текстура обновляется по revision, без загрузки в GPU каждый кадр.

Лимиты: VMT 64 KiB и вложенность 16, VTF 16 MiB и 2048×2048, один frame/face/depth.
При отсутствующем или неподдержанном материале используется checker. Эта версия
не реализует полноценный VertexLitGeneric: нет normal/specular/envmap, прозрачности,
многоматериальных draw batches, skin tables, Patch/Proxies и BSP-материалов.

### First BSP material (0.16.0, build 19)

Preview читает BSP `TEXINFO`/`TEXDATA`/string table, вычисляет UV оригинальными
texture vectors и разрешает первый `LightmappedGeneric` VMT `$basetexture` в VTF.
Встроенная комната получает отдельную кирпичную текстуру; успешная смена карты
обновляет Metal-текстуру по revision, а отклонённая карта не меняет активные
пиксели. Это первый material slot, ещё без multi-material batching и lightmaps.

### BSP multi-material atlas (0.17.0, build 20)

До 16 уникальных безопасных BSP-материалов декодируются в ограниченные 64×64
atlas tiles. TEXINFO каждой поверхности сохраняет свой slot, а Metal повторяет UV
внутри него без протекания соседней текстуры. Fixture чередует кирпичный и синий
материалы. Это base textures preview; lightmaps и полноценные Source shaders ещё впереди.
