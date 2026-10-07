# ClientMod: ресурсы и игровой сервер (0.36 / build 40)

Пакет из [предоставленной ссылки](https://disk.yandex.ru/d/bBscvB6EWB1xog)
скачан и проверен повторно. `Clientmod Rec1.4.rar`: 3 071 006 460 байт,
SHA256 `75b56b7eaf6b495b1234b3de2aaf7a3cf5eee3b590223fcfac88627a31aac973`.
893 файла, 7 093 429 433 распакованных байта; 31 BSP и 48 VPK-файлов.
Исходников C/C++, APK, SO и DLL в архиве нет. Это пакет ресурсов; он не
содержит реализацию особых клиентских механик ClientMod.

## Изменения

- Оригинальный weapon parser использует `MOD` для TXT/CTX. Проверенные
  каталоги `cm`, `cstrike_clientmod`, `cstrike` и их VPK теперь доступны
  через этот path ID. Отмонтирование удаляет также MOD paths.
- `MOD_WRITE` и `GAME_WRITE` направлены в собственную папку `game` приложения.
  Запись через эти path IDs не меняет импортированные ресурсы.
- Embedded frame loop вызывает штатный `HostState_Frame`. Перед закрытием
  активного сервера state machine выполняет LevelShutdown/GameShutdown,
  затем освобождаются engine edicts и GameDLL. Ранее игровые сущности
  оставались со ссылками на освобождённые edicts и падали при выходе процесса.
- Перед остановкой material system штатный particle manager освобождает
  material references. Это исправляет падение при повторном DLLInit после
  запуска карты с ресурсами ClientMod.
- В SWDS отключена обработка desktop material flags при уведомлении
  `MDLCACHE_STUDIOHWDATA`. Серверные studio metadata и collision сохранены;
  headless shaderapi не предоставляет shader variables для этой обработки.
- Добавлен ручной `clientmod_probe`; пользовательские ресурсы не включены
  в репозиторий, IPA или CI fixtures.

## Локальная проверка

На `cm/maps/awp_lego_2.bsp` (BSP20, PHYSCOLLIDE lump 25 606 байт) штатный
`Host_NewGame` проходит GameInit, LevelInit и ServerActivate. До добавления
игрока обнаружены 49 живых игровых сущностей и 36 team spawns.
После 300 кадров сервер остаётся активным. Пустой сервер намеренно не
продвигает simulation ticks — это исходное поведение `SV_Frame`.

Опциональный штатный `IVEngineServer::CreateFakeClient` вызывает оригинальный
ClientPutInServer и создаёт серверного CCSPlayer без внешней сети. С ним
два полных цикла в одном процессе продвинули simulation ticks на 303 и 302
соответственно. Проверен именно ServerClass `CCSPlayer`. Это не проверка HUD, движения,
выбора команды, стрельбы или AI бота. Завершение активного уровня проходит
без прежнего падения в entity list.

```sh
cmake -S . -B build -DSOURCE_BUILD_CSTRIKE=ON -DSOURCE_LINK_CSTRIKE=ON
cmake --build build --target clientmod_probe
# DOCUMENTS/Source1IOS/content/{cm,cstrike,hl2,platform}
build/clientmod_probe DOCUMENTS awp_lego_2
SOURCE_CLIENTMOD_PLAYER=1 build/clientmod_probe DOCUMENTS awp_lego_2
```

Probe проверяет два полных цикла запуска/закрытия в одном процессе.
Linux: 6/6 CTest PASS (22,67 с). Общие runtime contracts проверяют MOD VPK lookup, удаление MOD
search paths и фактическое размещение MOD writes в собственной папке game.

## Ограничения

В iOS приложении остаётся BSP/MDL preview с камерой. Новый игровой уровень
пока запускается ручным native probe; iOS level/player smoke ещё не добавлен.
Физический iPhone в этом этапе не проверен.

На `awp_lego_2` оригинальная GameDLL не знает `info_ladder`, а у ladder static
props отсутствуют PHY; совместимость лестниц ещё не завершена. VPhysics
также выводит предупреждения для существующего authored self-test scene.
На `aimbotz` отдельная попытка загрузки завершилась `pure virtual method
called`, с предупреждениями о non-brush inline models и backwards bounds;
успешная проверка одной карты не означает поддержку всех 31 карт.

Следующие задачи: устранить конфликты inline brush models, подключить
client DLL, local connection/usercmd, управление CCSPlayer, HUD и оружие.
Для точного переноса специфичных механик ClientMod нужен их код или
отдельная спецификация поведения; архив ресурсов их не определяет.

CI [37611935182](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37611935182)
прошёл на code head `5d454e3d89579422b9bf0674052e6b619c24116e`,
merge ref `91569caec29397abbc6c80b706ff4bd8a7f92890`: Linux 6/6,
ARM64 IPA и simulator smoke. Скачанная IPA независимо проверена:
0.36.0/build 40, Mach-O ARM64 executable, 264 PASS без FAIL, GPU revision 10,
pause/resume; screenshot просмотрен. Физический iPhone не проверен.

[IPA и диагностика](https://github.com/sh1zoooo/Source1-iOS/actions/runs/37611935182/artifacts/11479071277).
SHA256 IPA: `dfddc9019800bfcbb5f4a242b8985c1ad1083d9a69d53f9bff5ca8ef7a842e9e`.
ARM64 archives также скачаны и проверены независимо: 571 Mach-O MH_OBJECT
(548 game, 10 particles, 5 dmx, 5 choreo, 2 soundemitter, 1 scene).
SHA256 archive artifact: `e5dfbe3c118aac625300a110f17fcaafa7b15e1982e6dcd90e53b37fa8e109e4`.

Native content probe не
запускается в CI, поскольку пользовательские ресурсы туда не загружаются.
