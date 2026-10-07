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
прогон продвинул simulation ticks на 303. Это не проверка HUD, движения,
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
Общие runtime contracts отдельно проверяют MOD VPK lookup, удаление MOD
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
