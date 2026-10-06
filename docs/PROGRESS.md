# Прогресс Source 1 → iOS

Ориентировочно **70%**: BSP-сцена, камера, столкновения, VTF, живая физика и
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
Последний пользовательский лог iPhone 16e подтверждает 69 проверок версии 0.17,
GPU A18 и смену ориентации; пользователь подтвердил плавную деформацию модели.

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
