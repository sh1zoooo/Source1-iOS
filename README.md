# Source1-iOS

Экспериментальный проект переноса Source 1 на iOS ARM64, с проверкой на iPhone 16e.

**Текущий этап: основа приложения. Модули движка Source ещё не подключены.**
Приложение запускает наш C++-слой, рисует тестовый треугольник через Metal,
обрабатывает уход в фон и позволяет отправить диагностический лог.

## Получить сборку

1. Откройте [Actions](https://github.com/sh1zoooo/Source1-iOS/actions).
2. Выберите успешный запуск **iOS host build** для нужного коммита.
3. Скачайте артефакт `Source1IOS-<commit>` и распакуйте ZIP.
4. Внутри находится `Source1IOS-unsigned.ipa`, контрольная сумма, а после успешного
   теста симулятора — скриншот и лог запуска.

IPA **не подписана**: для установки её нужно подписать вашим способом установки.
Сборка не требует Apple Developer-сертификатов или секретов GitHub.
Минимальная версия iOS — 17.0. Запуск на устройстве проверяется отдельно.

## Сборка на Mac

Нужны Xcode с iOS SDK и CMake 3.24+.

```sh
cmake -S . -B build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0
cmake --build build-ios --config Release -- -quiet CODE_SIGNING_ALLOWED=NO
python3 scripts/package_ipa.py build-ios/Release-iphoneos/Source1IOS.app artifacts/Source1IOS-unsigned.ipa
```

## Проверка C++-слоя без iOS

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Actions выполняет проверки, собирает приложение для ARM64 и симулятора,
запускает его и ждёт записи о первом отправленном кадре Metal.

Исходная база движка ещё не выбрана. Официальный Source SDK 2013 не содержит
полного кода движка; оценка базы и следующие этапы описаны в
[docs/PORTING.md](docs/PORTING.md).
