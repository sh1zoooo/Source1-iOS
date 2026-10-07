# Прогресс Source 1 → iOS

Ориентировочно **92% минимального демонстрационного этапа**: BSP-сцена, камера, столкновения, VTF, живая физика и
геометрия Source studio model, weighted skinning, встроенные animation tracks и
первый VMT/VTF материал модели.
Этап 0.13 добавляет чтение костей/весов и CPU skinning с процедурной позой fixture.
Это не декодирование игровых MDL animation sequences и не физический скелет.
Этап 0.14 читает встроенные локальные animation tracks MDL: RLE rotation/position,
raw Quaternion48/64 и Vector48, интерполяция кадров и loop. Внешние ANI, секции,
delta, IK и blend sequences пока пропускаются, модель остаётся доступной в bind pose.
Этап 0.15 разрешает первый материал модели через MDL/CD texture directories,
VMT `$basetexture`, оригинальный VTF decoder и отдельную texture slot Metal.
Несколько материалов, skins и shader features пока впереди.
Этап 0.16 читает первый материал поверхности из BSP TEXINFO/TEXDATA/string table,
вычисляет UV по Source texture vectors и загружает LightmappedGeneric VMT/VTF.
Пока это один base-texture slot на всю preview-сцену, без lightmap.
Этап 0.17 сохраняет материал каждой поверхности и собирает до 16 VMT/VTF в
ограниченный atlas. Это уже разные текстуры пола/стен/brushes, но ещё без lightmap.
Этап 0.18 добавляет LDR RGBExp32 lightmaps в отдельный Metal atlas с полями
по краям tiles, UV стен в плоскости грани и bounded LZMA для материалов/lighting.
Отображается первая статическая lightstyle; HDR, animated styles и bump-lighting
не воспроизводятся. Это preview adapter, оригинальный shader API остаётся впереди.
Этап 0.19 разрешает внешний ANI filename, проверяет MDL animation block table
и читает простые raw/RLE tracks только внутри выбранного ANI block. Пропавший ANI
оставляет геометрию в bind pose; повреждённый существующий файл отклоняется.
Sections, IK, delta и blend sequences пока впереди.
Этап 0.20 поддерживает до 512 отдельных BSP material slots в двумерном atlas,
вместо подстановки slot zero после 16 материалов. Fixture с 17 материалами
проверяет второй ряд и повторный GPU completion после смены карты.
Этап 0.21 добавляет подключение распакованных content roots, загрузку BSP/MDL/VMT/VTF
через оригинальную filesystem, отключение и автоматическое подключение `cm`/`hl2`
при restart. Совместимость полного ClientMod-кеша ещё не проверена.
Процент — инженерная оценка оставшейся работы, а не число тестов или исходников.
100% здесь означает минимальный порт с реальным engine, загрузкой тестовой BSP-карты,
адаптированными материалами/рендерингом и камерой на iPhone. Готовая CS:GO не входит
в этот критерий; полный запуск CS:S также требует отдельного игрового этапа.

Подключены 24 оригинальные библиотеки (540 единиц компиляции), filesystem/VPK,
appframework, headless materials, model cache, studiorender и vphysics/IVP.
Оригинальные Host_Init и Host_RunFrame работают с -nogamedll, без игровой DLL.
Последний присланный лог iPhone 16e подтверждает 94 стартовые проверки 0.26,
cube/sphere rest, два SOLID_BBOX objects, MDL48, LDR lightmaps, GPU A18 и
смену ориентации. Pause/resume и live camera command в этом логе не показаны.
Пользователь ранее подтвердил плавную деформацию модели.

Новый этап: настоящий CModelLoader и CM_LoadMap загружают встроенный BSP-мир,
CM_BoxTrace ограничивает движение камеры. VTF читается оригинальной библиотекой;
полигоны и два сферических тела vphysics с ragdoll-шарниром выводятся нашим Metal адаптером.
Сенсорный ввод: слева движение, справа обзор. На Linux выполнены 30 000 кадров
и три цикла запуска/остановки; это не доказательство отсутствия утечек.
Версия 0.9.1 (build 11) прошла Linux Debug и ARM64-симулятор iPhone 16e, iOS 18.5:
две серии по 50 самотестов, runtime contracts, завершение кадра на GPU и
сворачивание/разворачивание. Итоговый скриншот комнаты проверен. Пользовательский лог также подтвердил новые 50 стартовых проверок, BSP-сцену,
GPU A18, смену ориентации и два pause/resume на iPhone 16e, iOS 18.6.2. [Отчёт проверки](VALIDATION-0.9.1.md).

Загрузчик studio model v49 проверяет MDL/VVD/VTX, VVD fixups, triangle lists/strips
и передаёт LOD 0 в Metal-адаптер. В 0.13–0.15 добавлены кости, веса, ограниченный
декодер встроенных анимаций и первый материал; это ещё не полный studio renderer.

Остаются графический shaderapi/materialsystem Source, настоящие материалы карт,
PVS, HDR/полное освещение, графический displacement LOD, полный evaluator sequences/ANI/IK,
аудио и игровая DLL. Версия 0.12.0/build 15 прошла Linux Debug, iPhone ARM64 и
симулятор: 121 PASS, завершённый GPU-кадр, видимая статическая модель и pause/resume.
Пользовательские BSP пока открываются только как ограниченный polygon preview;
engine brush-мир загружается только из нашей проверенной встроенной карты.

Версия 0.15.0/build 18 прошла Linux Debug и ARM64 iPhone/simulator builds;
симулятор дал 135 PASS, texture upload, GPU completion и pause/resume.
Артефакт и скриншот проверены отдельно. [Отчёт 0.15](VALIDATION-0.15.0.md).
На физическом iPhone эта новая версия пока не подтверждена.

Версия 0.17.0/build 20 прошла Linux Debug, ARM64 iPhone build и simulator smoke:
139 PASS, два BSP material slots, отдельная studio texture, GPU completion и
pause/resume. [Отчёт 0.17](VALIDATION-0.17.0.md). Пользовательский лог и кадр
подтверждают 0.17/69 PASS на физическом A18, загрузку обеих textures, GPU completion
и смену ориентации. Сворачивание/возврат в этом конкретном логе не показаны.

Версия 0.20/build 23 прошла Linux Debug runtime/ASan mutation tests, ARM64 iPhone
build и simulator smoke: 155 PASS, LDR lightmaps, 17-slot material grid, внешний
ANI, GPU completion обновлённой сцены и pause/resume. Загруженный IPA и скриншот
проверены отдельно. [Отчёт 0.20](VALIDATION-0.20.0.md).

Версия 0.21/build 24 прошла Linux Debug runtime/mutation tests, ARM64 iPhone
build и simulator smoke: 155 PASS, mount/load/unmount карты из content/smoke,
GPU completion revision 5, внешний ANI и pause/resume. Скачанный IPA, его SHA256
и скриншот проверены отдельно. [Отчёт 0.21](VALIDATION-0.21.0.md).
Полный кеш ClientMod пока не проверен; добавлен путь импорта распакованных ресурсов.

Этап 0.22/build 25 читает BSP entity text и выбирает камеру из info_player_start /
info_player_counterterrorist / info_player_terrorist / info_player_deathmatch.
Сброс камеры возвращает выбранную точку; это preview camera, не создание игрока.
Вход ограничен, повреждённые координаты не заменяют сцену. Оценка остаётся 70%.

Пользовательский лог и две фотографии 0.21 на физическом iPhone 16e подтверждают
77 PASS, LDR lightmap/base/model texture upload, GPU revision 2, pause/resume,
смену ориентации и разные позы встроенной модели. External ANI и кеш в этом
конкретном запуске не проверялись.

Этап 0.23/build 26 добавляет ограниченную цепочку VMT Patch include с применением
insert/replace к $basetexture по правилам Source. Общий путь используется для BSP
и studio model; циклы, traversal и KeyValues filesystem macros отклоняются.
Это поддержка базовой текстуры, не полного графического shader/material system.

Версия 0.23/build 26 прошла все три Linux CTest набора, ARM64 iPhone/simulator
builds и simulator smoke: 167 PASS, map spawn camera, Patch texture resolution,
импортированная BSP, GPU revision 5, external ANI и pause/resume. Скачанный IPA,
SHA256 и скриншот проверены отдельно. [Отчёт 0.23](VALIDATION-0.23.0.md).

Этап 0.24/build 27 добавляет визуальные static props BSP: словарь `sprp`,
MDL/VVD/VTX, положение/поворот/масштаб и общий VMT/VTF atlas. Повторяющиеся
модели декодируются один раз. Нативные тесты и 10 000 ASan/UBSan mutations
прошли; iOS CI и артефакт проверены, 175 PASS и GPU revision 6. Это ещё не PHY-коллизии
объектов и не игровой static prop manager. [Отчёт 0.24](VALIDATION-0.24.0.md).

Этап 0.25/build 28 расширяет bounded studio preview с MDL49 до MDL48/49,
включая простые embedded/ANI tracks. На портретном скриншоте 0.24 тестовые
static props были закрыты/вне ракурса; fixture переставлен, центры объектов
проверяются в портретной проекции. Нативные тесты и 12 000 ASan model mutations
прошли; четыре Linux CTest набора и обе iOS-сборки успешны. После тайм-аута
первого simctl launch повторный CI на новом runner прошёл: 181 PASS,
GPU revision 6, внешний MDL48/ANI, pause/resume. IPA SHA256 и скриншот
проверены отдельно; на изображении видны обе static props и анимированная
модель. Физический iPhone в этом этапе не тестировался.
[Отчёт 0.25](VALIDATION-0.25.0.md).

Этап 0.26/build 29 добавляет столкновения `SOLID_BBOX` static props:
оригинальные vphysics-объекты из hull bounds MDL и swept camera hull по
world AABB render bounds. Поворот и масштаб учитываются; reset пересоздаёт
объекты. Нативный прогон, падение куба/сферы и mutation-тесты прошли.
`SOLID_NONE` остаётся визуальным; `SOLID_VPHYSICS` пока требует отдельного
bounded PHY loader. Linux CI, ARM64 iPhone build и simulator прошли:
192 PASS, live collisions после reset, GPU revision 6, pause/resume.
Скачанный IPA/SHA256 и скриншот проверены отдельно.
[Отчёт 0.26](VALIDATION-0.26.0.md).

Этап 0.27/build 30 читает terminal convex point clouds из bounded PHY
и пересобирает формы оригинальным vphysics builder. Для SOLID_VPHYSICS
сохраняются несколько выпуклых частей, пустые зазоры, поворот/масштаб;
камера и статические vphysics objects используют эту форму. Imported
IVP pointers/topology не передаются legacy unserializer. Нативный runtime
и 20 000 ASan/UBSan PHY mutations прошли. Linux CI: 5/5 suites;
ARM64 iPhone build и simulator: 203 PASS, GPU revision 7, live PHY после
reset, pause/resume. Скачанный IPA/SHA256 и кадр проверены отдельно.
На физическом iPhone подтверждён предыдущий этап 0.26; 0.27 проверен
автоматически в simulator, не на телефоне пользователя.
[Отчёт 0.27](VALIDATION-0.27.0.md).

Этап 0.28/build 31 добавляет HDR-only faces/lighting в ограниченный BSP preview.
Согласованный выбор HDR records и RGBExp32 samples, fallback на общие faces,
LDR preference, fixed exposure/Reinhard mapping, edge padding и отказ
повреждённых метаданных проверены нативно. PHY shapes остаются активны после
HDR загрузки/reset. Это 8-bit preview, не HDR framebuffer или eye adaptation.
Linux CI 5/5, ARM64 iPhone build, simulator 210 PASS / GPU revision 8
прошли. Скачанный IPA/SHA256 и кадр проверены отдельно. Физический iPhone
для 0.28 пока не проверен. [Отчёт 0.28](VALIDATION-0.28.0.md).

Этап 0.29/build 32 добавляет per-mesh материалы MDL и remap default skin family.
Studio и BSP static props сохраняют разные texture slots; weighted skinning
сохраняет их при анимации. Bounded skin references проверяются до geometry
emission. Native runtime и 24 000 ASan mutations, Linux CI 5/5, ARM64 build
и simulator 221 PASS / GPU revision 8 прошли. Скачанный IPA/SHA256 и кадр
проверены отдельно. Самотест после смены карты/модели проверяет отдельную
fixture-сцену, сохраняя текущую. На физическом iPhone 0.29 не проверен.
Другие skin families / bodygroup selection / полный shader backend остаются
впереди. [Отчёт 0.29](VALIDATION-0.29.0.md).

Этап 0.30/build 33 сохраняет все bounded skin families и material reference
каждой вершины. Смена family меняет assignments, сохраняя pose/time/atlas;
static props используют собственный skin вместо пропуска nonzero skin.
Проверены invalid selection/unused family references и сохранение сцены при
повреждённом prop skin. Native runtime и 24 000 ASan mutations прошли;
Linux CI 5/5 и iOS simulator 233 PASS / GPU revision 9 прошли.
Скачанный ARM64 IPA/SHA256 и кадр проверены отдельно. Физический iPhone
этой версией пока не проверен. [Отчёт 0.30](VALIDATION-0.30.0.md).

Этап 0.31/build 34 подключает вложенные многотомные VPK v1/v2 через оригинальный
`CPackedStore` после bounded-проверки directory tree, embedded ranges и частей.
Одновременно подключаются `cm`, `cstrike`, `hl2`, `platform`. Реальный Clientmod
Rec1.4 (6.09 GB VPK) прочитан локально; настоящая модель молотова MDL48,
VVD/VTX/PHY и обе VMT/VTF текстуры загрузились. Добавлены bone-255 bind-pose
анимации и безопасная нормализация Windows-путей материалов. Linux CI 5/5,
ARM64 IPA и iOS simulator 240 PASS / GPU revision 9 прошли; скачанный IPA,
SHA256 manifest и кадр проверены отдельно.
[Отчёт 0.31](VALIDATION-0.31.0.md).

Этап 0.32/build 35 расширяет bounded studio preview до MDL44/48/49. Локальный
прогон настоящей `awp_lego_2.bsp` через ClientMod VPK загружает 5 ladder props,
2 model types и 2 472 prop triangles без пропусков; обе VTF текстуры прочитаны.
Пять collision props честно отмечаются unsupported, потому что требуемых `.phy`
нет в предоставленном кеше. Native runtime и 36 000 ASan studio mutations,
Linux CI 5/5, iOS ARM64/simulator 242 PASS / GPU revision 9 прошли; скачанный
IPA, SHA256 manifest и кадр проверены отдельно.
[Отчёт 0.32](VALIDATION-0.32.0.md).

Этап 0.33/build 36: bounded visual-only `prop_dynamic` / `prop_dynamic_override`
из BSP entities. До 512 моделей, проверенные transform/scale/skin, общий
model cache и material atlas. Nonzero bodygroups пропускаются; анимации,
entity outputs, physical entities и game DLL этим не реализуются.
30 000 ASan/UBSan spawn/model entity mutations прошли локально. CI ожидается.
[Отчёт 0.33](VALIDATION-0.33.0.md).

0.33.1/build 37 исправил ожидания skin remap в entity test и диагностику launch
timeout. CI 37575059340 прошёл Linux 5/5, iPhone ARM64 и simulator
startup/GPU/lifecycle. Оценка ~92% относится только к минимальному прототипу.

Этап 0.34/build 38 выбирает MDL bodygroups по base/modulo, включая пустые
варианты, вместо объединения всей геометрии. BSP model entities учитывают body;
skins и общий atlas сохраняются. Linux Debug: 5/5 CTest прошли, включая 72 000 studio и 30 000 entity
mutations. CI 37576240688: Linux 5/5, ARM64 iPhone и simulator прошли.
Скачанный IPA 0.34.0/build 38, SHA256 manifest, 256 PASS, GPU revision 10,
pause/resume и screenshot проверены отдельно. На физическом iPhone и настоящих
ClientMod bodygroups новый этап пока не проверен.
[Отчёт 0.34](VALIDATION-0.34.0.md).

## Отдельный этап: компиляция оригинального сервера CS:S

Добавлен опциональный `SOURCE_BUILD_CSTRIKE=ON`: 561 исходный файл сервера
и 24 файла пяти вспомогательных библиотек. На Linux Debug компиляция этого
этапа завершена на 100%, проверены все 585 объектов архивов. Это процент
компиляции выделенного этапа, а не готовности полной игры. iPhone ARM64 CI
также подтвердил 585/585; в скачанном пакете проверена ARM64 архитектура
каждого объекта. Итоговый CI 37579634052: Linux 5/5 тестов, ARM64 и
simulator smoke прошли; 256 PASS, GPU revision 10 и pause/resume.
Первая попытка CI остановилась на двух simctl launch timeout без стартового
лога; повторная проверка на новом runner прошла.
Приложение остаётся 0.34.0, host пока запускается с `-nogamedll`; **92%**
минимального прототипа сохраняются. Детали и следующие зависимости:
[CSTRIKE-COMPILE.md](CSTRIKE-COMPILE.md).
