# Прогресс Source 1 → iOS

Ориентировочно **58%**: BSP-сцена, камера, столкновения, VTF, живая физика и
геометрия Source studio model, weighted skinning, встроенные animation tracks и
первый VMT/VTF материал модели.
Этап 0.13 добавляет чтение костей/весов и CPU skinning с процедурной позой fixture.
Это не декодирование игровых MDL animation sequences и не физический скелет.
Этап 0.14 читает встроенные локальные animation tracks MDL: RLE rotation/position,
raw Quaternion48/64 и Vector48, интерполяция кадров и loop. Внешние ANI, секции,
delta, IK и blend sequences пока пропускаются, модель остаётся доступной в bind pose.
Этап 0.15 разрешает первый материал модели через MDL/CD texture directories,
VMT `$basetexture`, оригинальный VTF decoder и отдельную texture slot Metal.
Несколько материалов, skins, shader features и материалы BSP пока впереди.
Процент — инженерная оценка оставшейся работы, а не число тестов или исходников.
100% здесь означает минимальный порт с реальным engine, загрузкой тестовой BSP-карты,
адаптированными материалами/рендерингом и камерой на iPhone. Готовая CS:GO не входит
в этот критерий; полный запуск CS:S также требует отдельного игрового этапа.

Подключены 24 оригинальные библиотеки (540 единиц компиляции), filesystem/VPK,
appframework, headless materials, model cache, studiorender и vphysics/IVP.
Оригинальные Host_Init и Host_RunFrame работают с -nogamedll, без игровой DLL.
Последний пользовательский лог iPhone 16e подтверждает 63 проверки версии 0.13,
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
PVS, lightmaps, графический displacement LOD, полный evaluator sequences/ANI/IK,
аудио и игровая DLL. Версия 0.12.0/build 15 прошла Linux Debug, iPhone ARM64 и
симулятор: 121 PASS, завершённый GPU-кадр, видимая статическая модель и pause/resume.
Пользовательские BSP пока открываются только как ограниченный polygon preview;
engine brush-мир загружается только из нашей проверенной встроенной карты.

Версия 0.15.0/build 18 прошла Linux Debug и ARM64 iPhone/simulator builds;
симулятор дал 135 PASS, texture upload, GPU completion и pause/resume.
Артефакт и скриншот проверены отдельно. [Отчёт 0.15](VALIDATION-0.15.0.md).
На физическом iPhone эта новая версия пока не подтверждена.
