# Source1-iOS

Экспериментальный перенос Source 1 на iOS ARM64. Базовые библиотеки Source проверены
на физическом iPhone 16e с iOS 18.6.2. В версии 0.9 оригинальный appframework запускает зависимости, затем вызываются настоящий Host_Init и кадры dedicated-движка в штатном режиме -nogamedll.

**Текущий этап 0.37: офлайн-практика на `awp_lego_2` с оригинальным CCSPlayer, управлением, HUD и моделями 1P/3P. Для игры нужны импортированные ресурсы ClientMod. Изображение выводит адаптер Metal; полный графический клиент CS:S ещё не подключён.**

`SOURCE_BUILD_CSTRIKE=ON` собирает оригинальный сервер CS:S; manifest содержит
561 исходный файл, для Linux/Apple выбираются 548 объектов сервера и 23 объекта
вспомогательных библиотек. `SOURCE_LINK_CSTRIKE=ON` связывает GameDLL с host;
обе опции включены в iOS CI. Без связывания остаётся preview в `-nogamedll`.
Проверка и границы этапа описаны в [CSTRIKE-COMPILE.md](docs/CSTRIKE-COMPILE.md).

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
- `source_props_selftest` — на этой тестовой карте проверяет vphysics-объекты, ray/hull и блокировку камеры; возвращает исходную камеру.
- `source_bsp_phy` — те же объекты с реальной tapered PHY-формой вместо BBOX.
- `source_phy_selftest` — проверяет PHY-объекты, луч мимо узкой верхней части и блокировку камеры, сохраняя исходную камеру.
- `source_bsp_hdr` — HDR-only BSP со статическими PHY-объектами и преобразованным освещением preview.
- `source_hdr_selftest` — проверяет активную HDR-only сцену.
- `source_model_materials_selftest` — проверяет обе текстуры и skinning текущей demo MDL.
- `source_model_skin 1` — выбрать skin family текущей модели; `0` возвращает первый вариант.
- `source_bsp_skins` — два PHY-объекта одной MDL с разными skin families.
- `source_skin_selftest` / `source_props_skin_selftest` — проверки demo skin 1 и статических вариантов.
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
индекс (область, маски, луч, перемещение и удаление). Пространственные запросы используют настоящий уже инициализированный MDLCache. Настоящие `Host_Init(true)` и `HostState_Frame` работают со связанной GameDLL CS:S; offline practice активирует оригинальный уровень и серверного игрока. Графический backend материалов, полный клиент и звук пока не адаптированы.

Консоль и файловая система запускаются через настоящий `CAppSystemGroup`:
статические фабрики, `Connect`, `PreInit`, `Init`, обратный `Shutdown` и
`Disconnect`. UIKit управляет кадрами, поэтому используется `Startup`, без
desktop main loop. Исправлены откат частичного запуска и восстановление
родительской фабрики после остановки. Самотесты намеренно вызывают ошибки
двух тестовых систем; сообщения `intentional ... failure` в логе ожидаемы.

**Минимальное демо завершено на 100% по реализации:** настоящий Source engine,
тестовый BSP, адаптированные материалы/Metal и камера. Полный CS:S требует
полного клиентского этапа; физический запуск 0.37 на iPhone ещё не проверен.
[Критерий готовности и оставшиеся этапы](docs/PROGRESS.md).

Сеть отключена параметром `-noip`; предупреждение о невозможности dedicated-сервера
ожидаемо: приложение не слушает сетевые соединения. Очередь игровых конфигураций
очищается; offline practice запускается отдельными безопасными командами.

## Новый игровой этап 0.37

Реализованы: офлайн-игрок на `awp_lego_2`, штатные usercmd CS:S, прыжок,
приседание, стрельба/перезарядка, HUD и модели 1P/3P. Ресурсы нужно импортировать
из распакованного архива; полный ClientMod-клиент ещё не готов.
[Управление, импорт и границы проверки](docs/GAMEPLAY-0.37.md).

## Скачать IPA

Последняя прошедшая CI-сборка: [0.37.0, build 41 — IPA и диагностика](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37621264912/artifacts/11483041984).
122 стартовые проверки; симулятор подтверждает GPU, terrain, BSP materials/lightmaps,
PHY, MDL44/48/49, ANI, skins/bodygroups, BSP entities и background/resume (264 PASS).
В IPA связана оригинальная GameDLL CS:S: DLLInit, 196 server classes и штатный
DLLShutdown. Добавлены Play/Stop, Jump/Duck/Fire/Reload, реальный HUD и модели
1P/3P с оригинальными bone poses. Без ресурсов открывается демо-комната;
для практики импортируйте `cm`, `cstrike`, `hl2`, `platform` в
`Source1IOS/content` и нажмите Play. Карта `awp_lego_2` и все игровые действия
проверены локально двумя циклами; симулятор CI проверяет UI/startup/Metal без
пользовательских ресурсов. [Проверки и границы этапа](docs/GAMEPLAY-0.37.md).
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

Этап 0.26/build 29 добавляет `SOLID_BBOX` static prop collisions: камера
останавливается перед предметами, оригинальные vphysics-тела сталкиваются
с их bounding box. Учитываются MDL hull/render bounds, поворот и масштаб.
Reset сохраняет коллизии. Лимит — 512 collider instances; нулевые bounds
дают визуальный объект с диагностикой. `SOLID_NONE` не мешает камере,
`SOLID_VPHYSICS` пока отображается с диагностикой неподдержанного PHY.
Linux CI, iPhone ARM64 build и simulator GPU прошли; IPA/SHA256 и кадр проверены отдельно.
[Проверки коллизий static props](docs/VALIDATION-0.26.0.md).

Этап 0.27/build 30 добавляет bounded `.phy` для `SOLID_VPHYSICS`: один static
solid, до 128 terminal convex pieces, 2048 points на piece. Из IVPS/VPHY
читаются проверенные points; оригинальный vphysics builder пересобирает
каждую часть отдельно, не заполняя зазоры общей convex hull. Учитываются
поворот/масштаб, формы кэшируются по модели/масштабу. Missing PHY оставляет
визуальный prop с предупреждением; повреждённый/неподдержанный PHY отклоняет
карту без замены сцены. Native runtime и 20 000 PHY mutations прошли;
Linux CI (5/5), ARM64 iPhone build и simulator GPU прошли: 203 PASS,
GPU revision 7, live PHY после reset, pause/resume. Скачанный IPA/SHA256
и кадр проверены отдельно. Обычный запуск показывает PHY demo (98 startup PASS).
[Проверки PHY](docs/VALIDATION-0.27.0.md).
Этап 0.28/build 31 добавляет HDR-only BSP preview: `FACES_HDR` / `LIGHTING_HDR`,
или общие face records при отсутствии HDR faces. При наличии LDR lighting
сохраняется прежний LDR путь. HDR RGBExp32 преобразуется с фиксированной
экспозицией и Reinhard mapping в 8-bit atlas; eye adaptation и полный HDR
shader backend Source не реализованы. Linux 5/5, ARM64 build и simulator
210 PASS / GPU revision 8 прошли. Скачанный IPA/SHA256 и кадр проверены отдельно.
Обычный запуск показывает HDR/PHY demo с 101 startup checks.
[Проверки HDR-only](docs/VALIDATION-0.28.0.md).
Этап 0.29/build 32 сохраняет material каждого mesh, проверяет skin table MDL
и использует remap семейства 0. До 256 VMT/VTF slots модели помещаются в atlas,
статические props добавляют свои slots в BSP atlas (общий лимит 512).
Анимация сохраняет назначение материалов. Native runtime и 24 000 ASan studio
mutations, Linux CI 5/5, ARM64 build и simulator 221 PASS / GPU revision 8
прошли. Скачанный IPA/SHA256 и кадр проверены отдельно. Обычный запуск показывает MDL48 с двумя
материалами (105 startup checks). Выбор других skin families и shader features
ещё не реализован. [Проверки материалов MDL](docs/VALIDATION-0.29.0.md).
Общие BSP самотесты после смены карты/модели используют отдельную fixture-сцену,
сохраняя текущую карту, камеру, animation state и GPU texture revisions.
Этап 0.30/build 33 выбирает skin families MDL (до 256) без перезагрузки geometry,
animation или texture atlas. BSP props с ненулевым skin теперь отображаются
через тот же model cache, со своими texture assignments и сохранённой PHY.
Неверное семейство отклоняется без замены сцены. Native runtime / 24 000 ASan
studio mutations, Linux CI 5/5 и iOS simulator 233 PASS / GPU revision 9
прошли. Скачанный ARM64 IPA/SHA256 и кадр проверены отдельно. Demo: две skin variants статической
модели и анимированная MDL48 с skin 1 (108 startup checks).
[Проверки skin families](docs/VALIDATION-0.30.0.md).
Этап 0.31/build 34 добавляет bounded import вложенных многотомных VPK v1/v2
через оригинальный Source `CPackedStore`. На полном Clientmod Rec1.4 локально
прочитаны 6.09 GB VPK и загружена настоящая textured MDL48 модель молотова
(605 вершин, 780 треугольников). Поддержаны bone-255 empty clips и Windows-пути
VMT/MDL. Это импорт ресурсов, не запуск client/server DLL или правил CS:S.
[Проверки ClientMod VPK](docs/VALIDATION-0.31.0.md).
Этап 0.32/build 35 добавляет bounded preview старых studio-моделей MDL44,
встречающихся в настоящем CS:S/ClientMod content. На `awp_lego_2.bsp` теперь
загружаются пять ladder static props: 2 472 треугольника и две VTF-текстуры.
Отсутствующие в самом кеше `.phy` не подменяются неточной коллизией.
[Проверки MDL44](docs/VALIDATION-0.32.0.md).
Этап 0.33/build 36 читает визуальные модели `prop_dynamic` и
`prop_dynamic_override` из BSP entities: origin/angles/scale/skin, общий
MDL/VVD/VTX и VMT/VTF atlas. `source_bsp_entities` открывает fixture из двух
моделей с разными skins; `source_entities_selftest` проверяет staging.
Это bind-pose preview без entity animation, outputs или collision; nonzero
bodygroups пока пропускаются. [Проверки entities](docs/VALIDATION-0.33.0.md).
Этап 0.34/build 38 выбирает варианты частей MDL по Source `body/base`,
включая пустые варианты. По умолчанию отображается один вариант каждой части,
а не их объединение. BSP entity preview учитывает `body`, сохраняет skins и
общий материал-кэш; анимации сущностей/игровая DLL остаются впереди.
[Проверки bodygroups](docs/VALIDATION-0.34.0.md).
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

История этапа 0.36: [оригинальный игровой уровень на ресурсах ClientMod](docs/CLIENTMOD-0.36.md). Локальные проверки и iOS CI прошли; физический запуск этого этапа ещё не проверен.
