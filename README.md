# Source1-iOS

Экспериментальный перенос Source 1 на iOS ARM64. Базовые библиотеки Source проверены
на физическом iPhone 16e с iOS 18.6.2. В версии 0.6 оригинальный appframework запускает dedicated engine API и его зависимости: материалы без графики, model cache и физику.

**Текущий этап: частичный порт ядра и файловой системы Source. Полный движок пока не запускается.**

## Что реально подключено

539 единиц компиляции из `tier0`, `tier1`, `mathlib`, `vstdlib`, `filesystem`, `vpklib`
`tier2`, `tier3`, `appframework`, `bitmap`, `engine`, `materialsystem`, `shaderapiempty`, `shaderlib`, `vtf`, `datacache`, `studiorender`, `vphysics` и IVP/Havana исходной базы
[nillerusr/source-engine](https://github.com/nillerusr/source-engine),
зафиксированной на `ed8209cc35c61fbd8ddff8480962a01c981eef2f`.

При запуске приложение получает настоящий `VEngineCvar004` через реестр интерфейсов
Source, выполняет `Connect` / `Init`, запускает `MathLib_Init` и проверяет allocator,
CRC32, bitbuf, KeyValues, матрицы, таймер и ConVar. Результаты попадают в лог.
Вращающийся куб преобразуется функциями `AngleMatrix` и `VectorTransform` из Source.
Вывод через Metal и сама тестовая геометрия написаны нами.

Поле консоли работает через реальные `CCommand`, `ConCommand` и `ConVar`:

- `source_status` — перечень подключённых библиотек и границы текущего порта.
- `source_selftest` — повторить все 35 проверок Source.
- `source_app_selftest` — проверить фабрику, порядок остановки и откат ошибок.
- `source_assets_selftest` — VTF, data cache, физические коллизии и симуляция.
- `source_engine_selftest` — семь проверок оригинального engine.
- `source_fs_selftest` — повторить семь проверок файловой системы.
- `ios_rotation_speed 0` — остановить куб.
- `ios_rotation_speed 30` — возобновить вращение.

Оригинальный `VFileSystem022` подключён к `Documents/Source1IOS/game` с path ID
`GAME` и `DEFAULT_WRITE_PATH`. Каталог доступен через приложение «Файлы».
Самотесты в отдельном каталоге `selftest` проверяют запись, чтение/seek, поиск,
KeyValues на диске, асинхронное чтение и встроенные данные VPK v1/v2.
Пользовательский каталог `game` самотесты не изменяют.

169 единиц компиляции настоящего `engine` связаны в dedicated-конфигурации.
Проверяются реальная фабрика, буфер команд (порядок, кавычки, wait) и пространственный
индекс (область, маски, луч, перемещение и удаление). Пространственные запросы используют настоящий уже инициализированный MDLCache. Полный `Host_Init`, графический backend материалов, загрузка BSP, игровые клиент/сервер
и звук **ещё не запущены**. Это не готовый запуск HL2/CS:S.

Консоль и файловая система запускаются через настоящий `CAppSystemGroup`:
статические фабрики, `Connect`, `PreInit`, `Init`, обратный `Shutdown` и
`Disconnect`. UIKit управляет кадрами, поэтому используется `Startup`, без
desktop main loop. Исправлены откат частичного запуска и восстановление
родительской фабрики после остановки. Самотесты намеренно вызывают ошибки
двух тестовых систем; сообщения `intentional ... failure` в логе ожидаемы.

**Прогресс: ориентировочно 20% до минимального запуска Source с тестовой картой
и камерой на iPhone.** Это оценка по подсистемам, а не процент исходников.
[Критерий готовности и оставшиеся этапы](docs/PROGRESS.md).

## Скачать IPA

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

## Новый этап 0.6

`VMaterialSystem081` работает с оригинальным `shaderapiempty`: это штатный Source
backend без графики. Через `CAppSystemGroup` проходят Connect/Init/Shutdown/Disconnect
материалов, физики, data cache, студийного рендерера, MDLCache и dedicated engine API.
`ModInit` и `Host_Init` пока не вызываются; загрузка карты и игровой цикл впереди.

Девять новых проверок подтверждают зависимости, чтение/запись VTF с тремя mip-уровнями,
отказ на коротком заголовке, RGBA→BGRA, поиск/блокировку/удаление data cache,
физическую форму бокса, трассировку луча и падение сферы под действием гравитации.
Физика выполняется оригинальным vphysics/IVP. Куб на экране всё ещё выводит наш Metal backend.

Общие tier-библиотеки учитывают число подключённых систем. Статические Source команды
восстанавливаются при повторном запуске; desktop globals и studio hooks сведены
к единственным владельцам. Проверка остановки/повторного запуска входит в Actions.

Самотест короткого VTF намеренно вызывает сообщение `Error unserializing VTF file`;
следующий результат должен быть `VTF truncated header rejected: PASS`.
