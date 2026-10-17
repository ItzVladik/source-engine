# Зависимость оригинального Underhell от предустановленных save-файлов

Дата анализа: 2026-08-18

## Краткий вывод

Да, оригинальный Underhell намеренно распространяет бинарные сохранения и использует их как часть сценарной системы, а не как случайно оставленные разработчиком файлы.

Это подтверждается одновременно четырьмя вещами:

1. В `SAVE/Do Not Delete House.txt` прямо сказано, что удаление файлов ломает мод.
2. В `SAVE/Backup/Told you so....txt` предлагается восстановить `House.hiddensave` из backup.
3. `cfg/chapter1.cfg` не запускает карту, а выполняет:

   ```cfg
   maxplayers 1
   load house0.restart.hiddensave
   ```

4. `Uh_Chapter1_01_d.vmf` возвращает игрока в House командой:

   ```text
   load house0.hiddensave
   ```

Расширение `.hiddensave` ничего принципиально не меняет: внутри это обычный Source save с сигнатурой `JSAV`.

## Загруженные файлы

| Файл | Размер | Назначение по найденным ссылкам |
|---|---:|---|
| `house.hiddensave` | 2 100 415 | Рабочий snapshot House; House-карта пытается перезаписывать его |
| `house0.hiddensave` | 2 100 415 | Snapshot, который загружается из `Uh_Chapter1_01_d.vmf` |
| `house0.restart.hiddensave` | 2 100 274 | Чистая точка входа `chapter1.cfg` |
| `Backup/*.hiddensave` | те же размеры | Побайтовые резервные копии основных hidden save-файлов |
| `inventory_is_full.sav` | 40 326 250 | Обычный тестовый save из `Uh_Chapter1_13`; ссылок из cfg/VMF на него не найдено |

SHA-256 основных файлов и их backup-копий совпадает. Значит backup действительно предназначен для ручного восстановления обязательных snapshots.

## Что сохраняется внутри

Строковый анализ `house*.hiddensave` показывает, что это полный снимок `Uh_House_0`, включая:

- игрока и `CPlayerLocalData`;
- инвентарь, endurance, night vision и другие кастомные поля;
- `env_global` и таблицу global entities;
- logic relays, counters и отложенные outputs;
- NPC, physics props, двери и их текущее состояние;
- think-функции и context thinks;
- targetname/EHANDLE-связи;
- текущие sound/scene/controller состояния;
- ссылку на `uh_house_0.hl1`;
- карту `maps/Uh_House_0.bsp`.

То есть это не небольшой файл кампании, а сериализованный экземпляр практически всего серверного мира.

## Зачем авторы сделали этот костыль

House является hub-картой, в которую игрок возвращается после удалённых и пространственно не связанных эпизодов. Обычный `changelevel` с transition landmark хорошо переносит игрока между физически состыкованными картами, но не решает автоматически задачу «заморозить весь дом, уйти на большую главу, затем вернуть абсолютно каждую дверь, physics prop, relay и отложенное событие».

Авторы использовали save как готовую сериализацию hub-миры:

1. Сохранить House целиком.
2. Запустить/пройти отдельную главу.
3. Загрузить House snapshot.
4. Продолжить с физическим и логическим состоянием дома, существовавшим перед уходом.

Дополнительный `house0.restart.hiddensave` служит готовой начальной точкой для запуска Chapter 1 без прохождения предыдущей цепочки инициализации.

Это было быстро и сохраняло даже не предусмотренные специально состояния сущностей. Цена — жёсткая привязка дистрибутива к бинарному save-формату.

## Почему это ломает портирование

Source save не является стабильным форматом данных кампании.

Он зависит от:

- версии engine/save format;
- конкретного BSP и порядка map entities;
- имён C++ классов;
- datamap каждого класса;
- имён, типов и порядка полей;
- зарегистрированных think-функций;
- layout сетевых и серверных классов;
- доступных entity factories;
- модели и string tables;
- текущей DLL, на которой snapshot был создан.

В реконструкции уже добавлялись и менялись поля `CHL2_Player`, `CNPC_Ace`, `CNPC_UH_Butcher`, `CNPC_CombineS`, flare, inventory, camera и других сущностей. Старый save может:

- пропускать новые поля;
- записывать старое значение не в тот смысловой объект;
- не найти think-функцию;
- восстановить несуществующий factory/classname;
- получить invalid function pointer;
- восстановить EHANDLE на другую сущность;
- упасть во время restore или вскоре после него.

Поэтому копирование оригинальных `.hiddensave` в порт — принципиально ненадёжное решение. Даже если конкретная версия сейчас загрузится, следующий datamap change снова её сломает.

## Исправленная проблема порта

До введения compatibility layer восстановленный `CLogicAutosave::InputHardSave` не соответствовал House VMF: был `FIELD_VOID`, игнорировал имя, зависел от Hard difficulty и запускал только `autosave`.

Теперь input принимает строку, поддерживает `name` и `save name` и создаёт именованный save. Это нужно для совместимости, но не делает shipped binary snapshots хорошей окончательной архитектурой.

## Нормальное решение

### Рекомендуемый вариант: versioned campaign state + чистая загрузка карты

Нужно разделить состояние на два класса.

#### 1. Долговременное состояние кампании

Хранить явно и версионировать:

- завершённые главы;
- ключевые решения;
- diary/mail/trophy flags;
- Hermit counters;
- найденные ключи и важные предметы;
- состояние крупных квестов;
- время суток/фазу House;
- состояние уникальных дверей, тайников и puzzle;
- player inventory/gear, которые должны переживать главу.

Формат может быть:

- KeyValues-файл `cfg/uh_campaign_state.vdf`/`save/uh_campaign_state.dat`;
- отдельный game-system save block;
- SQLite/JSON не нужен — для OB-era SDK достаточно versioned KeyValues.

Обязательные свойства:

```text
format_version
campaign_slot/id
map-independent stable keys
safe defaults for missing fields
explicit migrations N -> N+1
atomic write through temporary file
```

#### 2. Временное состояние карты

Не переносить весь entity graph между главами. При входе House загружать BSP заново:

```text
changelevel Uh_House_0
```

или соответствующую актуальную House-карту, после чего `logic_auto`/один bootstrap entity применяет campaign state:

- открывает нужные двери;
- удаляет уже подобранные уникальные items;
- выставляет skin/bodygroup;
- активирует нужные relays;
- создаёт сохранённые quest props;
- выбирает day/night;
- телепортирует игрока к правильной точке входа;
- восстанавливает inventory/health только в предусмотренном объёме.

Так карта становится детерминированной и не зависит от C++ datamap layout.

## Что делать с физическими объектами в House

Не следует сериализовать все physics props только потому, что save умеет это делать.

Варианты:

1. **Сбрасывать мелкий мусор** при каждом возвращении — самый стабильный вариант.
2. Для важных props назначить стабильный `targetname`/persistent ID и хранить только:
   - существование;
   - origin/angles;
   - open/closed/broken state;
   - skin/bodygroup.
3. Ввести `uh_persistent_prop` только для специально отмеченных объектов.

Не хранить сырые entindex, EHANDLE или указатели. Связи восстанавливать по стабильным строковым ID после spawn всех сущностей.

## Предлагаемая схема перехода

### Этап 0 — временная совместимость

- Исправить `HardSave` как строковый input.
- Разрешить именованный save независимо от difficulty.
- Нормализовать параметры вида:
  - `house.hiddensave`;
  - `save Level0_SouthWing_VentNode`.
- Вывести предупреждение, что legacy hidden saves несовместимы между build versions.

Это позволит временно запускать оригинальную логику, но не решит архитектурную проблему.

### Этап 1 — bootstrap без предустановленного save

- Заменить `chapter1.cfg: load house0.restart.hiddensave` на запуск карты/кампании.
- Добавить `point_uh_campaign` или auto game system.
- На чистом профиле создать default campaign state программно.
- Убрать требование наличия `house0.restart.hiddensave`.

### Этап 2 — возврат в House без `load`

- Заменить `load house0.hiddensave` в конце главы на `changelevel`/campaign transition.
- Перенести House progression из snapshot в explicit state keys.
- Сделать House startup idempotent: повторное применение одного state даёт одинаковый результат.

### Этап 3 — удалить legacy snapshots

После проверки всех путей:

- не поставлять `house*.hiddensave`;
- удалить backup-инструкции;
- оставить optional importer только если нужна совместимость со старыми пользовательскими профилями;
- не поддерживать импорт оригинального binary save в новые C++ datamaps.

## Можно ли автоматически конвертировать старые hidden saves

Полная надёжная конверсия практически нецелесообразна. Для чтения старого snapshot нужны старые datamaps и точное понимание каждой сериализованной сущности.

Допустим ограниченный одноразовый импорт:

- запустить оригинальную совместимую DLL;
- загрузить snapshot;
- экспортировать только whitelist логических значений в новый KeyValues format.

Но новый порт не должен напрямую парсить и восстанавливать весь `JSAV` старой версии.

## Рекомендация для этого репозитория

Окончательная цель:

```text
предустановленные .hiddensave: не нужны
обычные пользовательские quick/auto saves: поддерживаются текущей DLL
межглавное состояние: versioned campaign state
переходы: fresh map + deterministic state application
```

Это сохраняет стандартные пользовательские saves внутри одной версии игры, но убирает shipped binary snapshots как обязательный игровой asset.

## Файлы, которые потребуется изменить при реализации

Контент/VMF:

- `Underhell/cfg/chapter1.cfg`;
- `Uh_Chapter1_01_d.vmf` (`load house0.hiddensave`);
- `Uh_House_1_d.vmf` (`HardSave house*.hiddensave`, `OnLoadGame` ветки);
- House bootstrap relays и global counters.

Код:

- новый `CUHCampaignState`/`point_uh_campaign`;
- player inventory export/import;
- versioned persistent prop support;
- временное исправление `CLogicAutosave::InputHardSave`;
- тесты map spawn/return/new game/migration.

## Реализованный временный proxy

В коде добавлены:

```text
uh_hidden_save_proxy 0
uh_load_hidden_save <name.hiddensave>
```

По умолчанию proxy выключен и `uh_load_hidden_save` выполняет обычный `load`.
При `uh_hidden_save_proxy 1` известные snapshots перенаправляются:

```text
house0.restart.hiddensave -> changelevel Uh_House_0
house0.hiddensave         -> changelevel Uh_House_0
house.hiddensave          -> changelevel Uh_House_0
house1.hiddensave         -> changelevel Uh_House_1
```

`point_servercommand` также перехватывает соответствующие `load` из VMF, когда cvar включён. Прямую engine-команду `load` из старого `chapter1.cfg` перехватить безопасно нельзя, поэтому для proxy-теста cfg должен вызывать:

```cfg
uh_load_hidden_save house0.restart.hiddensave
```

Это намеренно экспериментальная прослойка: fresh map пока не применяет полноценный versioned campaign state.

Также исправлен `logic_autosave.HardSave`: input принимает строку, нормализует как `name` так и `save name`, и выполняет именованный save независимо от difficulty.

## Минимальный набор тестов

1. Чистая установка без каталога `SAVE` запускает Chapter 1.
2. Возврат из Chapter 1 в House не вызывает `load`.
3. Ключевые решения и inventory сохраняются.
4. Удалённые уникальные items не появляются повторно.
5. Day/night и House ending корректно восстанавливаются.
6. Обычный quicksave загружается на том же build.
7. Обновление campaign format выполняет migration.
8. Изменение datamap NPC/player не требует пересоздания shipped save.
9. Повторный вход House не дублирует outputs/items.
10. Отсутствие старых `.hiddensave` не вызывает crash или soft lock.
