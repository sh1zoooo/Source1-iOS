# Source1-iOS

Экспериментальный перенос Source 1 на iOS ARM64. Первый этап приложения проверен
на физическом iPhone 16e с iOS 18.6.2; эта версия добавляет настоящий код Source.

**Текущий этап: частичный порт базовых библиотек Source. Полный движок пока не запускается.**

## Что реально подключено

101 файл из `tier0`, `tier1`, `mathlib`, `vstdlib` исходной базы
[nillerusr/source-engine](https://github.com/nillerusr/source-engine),
зафиксированной на `ed8209cc35c61fbd8ddff8480962a01c981eef2f`.

При запуске приложение получает настоящий `VEngineCvar004` через реестр интерфейсов
Source, выполняет `Connect` / `Init`, запускает `MathLib_Init` и проверяет allocator,
CRC32, bitbuf, KeyValues, матрицы, таймер и ConVar. Результаты попадают в лог.
Вращающийся куб преобразуется функциями `AngleMatrix` и `VectorTransform` из Source.
Вывод через Metal и сама тестовая геометрия написаны нами.

Поле консоли работает через реальные `CCommand`, `ConCommand` и `ConVar`:

- `source_status` — перечень подключённых библиотек и границы текущего порта.
- `source_selftest` — повторить проверки Source.
- `ios_rotation_speed 0` — остановить куб.
- `ios_rotation_speed 30` — возобновить вращение.

Модуль `engine`, оригинальная файловая система, материалы, загрузка BSP,
игровые клиент/сервер и звук **ещё не подключены**. Это не готовый запуск HL2/CS:S.

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
