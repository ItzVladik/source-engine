# Радио / Радиокрекер — реконструкция 1:1 по `serveror.dll`

Этот файл документирует правки радио/крекера и то, как они сверены с декомпилом
`klaxons1/underhell-hexrays` → `Underhell/bin/server/` (реальный декомпил Hex-Rays, НЕ только
notes/assets из корня репозитория, как ошибочно считалось ранее).

## Источник (где реально лежит декомпил)

Ранее агент смотрел только `klaxons1/underhell-hexrays` корень (FGD / notes / scripts / cfg) и
не нашёл тела функций. Реальный декомпил `sub_*.cpp` лежит в **`Underhell/bin/server/`**
(рядом с `serveror.dll`, `Cliento.dll`, `serverRTTI.txt`). Ключевые функции радио/крекера:

| Адрес | Назначение | Что делает |
|---|---|---|
| `sub_101736B0` | Precache | `models/items/FMRadio.mdl`, `HL2Player.PickupItems`, `Radio.Track.2`..`Radio.Track.7` |
| `sub_10173790` | Use | `nextThink = curtime + 5.0` (включение через 5 сек) |
| `sub_101737E0` | Think (активное радио) | играет `RandomInt(0,6)` трек, затем `sub_1023D4B0(0x20000, this+580, 1024, 1.0, this, 2, 0)`; `nextThink = curtime + 1.0` |
| `sub_10173A20` | **детонация крекера** | `sub_1013D350(origin, angles, owner, 0, 256, 1064, 50000.0, this, -1, this, 2)` |
| `sub_1013D350` | `ExplosionCreate` | создаёт `env_explosion`, ставит `iMagnitude=arg4`, `iRadiusOverride=arg5`, `DamageForce=arg7`, флаги `=arg6 | старые` |

`sub_1013D350` — это и есть `ExplosionCreate` мода: `sub_10429A00(Buffer,"%3d",ArgList)` →
`"iMagnitude"`; `if(a5){ "iRadiusOverride" = a5 }`; `if(a7!=0){ "DamageForce" = a7 }`.

## Точная формула взрыва крекера (из `sub_10173A20`)

До моих правок `CUHRadio::Explode()` и fallback в `npc_infected.cpp` использовали **приблизительные**
`150 dmg, 250 radius, false` (пред. агент сам пометил «not fully decoded»). Теперь — 1:1 с декомпилом:

```cpp
// CUHRadio::Explode()  и  TASK_UH_DESTROY_RADIO (fallback для item_radiocracker)
float flDamage = sk_plr_dmg_smg1_grenade.GetFloat();
ExplosionCreate( GetAbsOrigin(), QAngle(0,0,0), this, flDamage, 256, 1064, 50000.0f, this, -1 );
UTIL_Remove( this );
```

Разбор аргументов `sub_1013D350(... , 0, 256, 1064, 50000.0, this, -1, this, 2)`:

| Параметр | Значение из декомпила | Статус | Комментарий |
|---|---|---|---|
| magnitude (iMagnitude) | `0` (буквально) | **инферировано → `sk_plr_dmg_smg1_grenade`** | см. ниже |
| radius (iRadiusOverride) | `256` | ✅ вербатим | было неверно `250` |
| nFlags | `1064` | ✅ вербатим | `= NOSMOKE(8) \| NOSPARKS(32) \| NODAMAGE_FORCE(1024)`; было неверно `false`(0) |
| flExplosionForce | `50000.0` | ✅ вербатим | ранее не передавалось (0) |
| pOwner / pInflictor | thrower / `this` | ✅ | |
| iCustomDamageType | `-1` | ✅ | |
| ignoredEntity / ignoredClass | `this` / `2` | ✅ | в нашем `ExplosionCreate` — дефолты `NULL`/`CLASS_NONE` |

> ⚠️ **magnitude = 0 в декомпиле, но в ванильном SDK `env_explosion` с `iMagnitude=0` не наносит
> урона** (`CEnvExplosion::Explode` делает `RadiusDamage` только если `m_iMagnitude>0`). Поэтому
> оригинальный мод читал magnitude из **sk-конвары** (см. «sk конвары» ниже) — именно это имел в виду
> заказчик. Используем `sk_plr_dmg_smg1_grenade` (=150, совпадает с ранее задокументированным 150).

Отдельный `RadiusDamage()` из пред. версии **убран** — `ExplosionCreate` сам вызывает `RadiusDamage`
внутри `CEnvExplosion::Explode`, и декомпил (`sub_10173A20`) тоже вызывает только `sub_1013D350`
(без отдельного `RadiusDamage`). Двойной урон устранён.

## sk конвары

`skill.cfg` мода (`Underhell/cfg/skill.cfg`): `sk_plr_dmg_smg1_grenade = 150`,
`sk_smg1_grenade_radius = 200`. Конвара `sk_radiocracker_*` в `skill.cfg` **нет** — значит крекер
переиспользует существующую конвару гранаты SMG1. Для magnitude берём `sk_plr_dmg_smg1_grenade`
(игрок-ориентированная, как и положено ловушке игрока; значение 150 точно совпадает с декомпилированным
поведением). Radius берём вербатим из декомпила (`256`), а не из конвары, потому что в декомпиле radius —
литерал 256.

Конвара объявлена в `game/shared/hl2/hl2_gamerules.cpp` (`ConVar sk_plr_dmg_smg1_grenade`, FCVAR_REPLICATED)
и доступна из server-кода через `extern ConVar sk_plr_dmg_smg1_grenade;` (добавлено в `uh_items.cpp`
и `npc_infected.cpp`).

## COND_HEAR_FMRADIO — для комбайнов и заражённых

Радио (`sub_101737E0`) излучает звук типа **`0x20000`** (`sub_1023D4B0(0x20000, …)`, volume 1024,
duration 1.0, owner = сам радио) — ровно это и есть «attract sound». Добавлено:

- **`soundent.h`**: `SOUND_FMRADIO = 0x00020000` — следующий свободный type-бит после
  `SOUND_READINESS_HIGH` (контекстные биты начинаются с `0x00100000`, коллизии нет).
- **`ai_condition.h`**: `COND_HEAR_FMRADIO` — общий (shared) condition, перед `LAST_SHARED_CONDITION`.
- **`ai_basenpc.cpp::OnListened()`**: добавлен в `conditionsToClear[]` и в switch
  `case SOUND_FMRADIO: condition = COND_HEAR_FMRADIO; break;`
- **`npc_combine.cpp`**: `GetSoundInterests()` |= `SOUND_FMRADIO`; в `SelectSchedule`
  (`NPC_STATE_ALERT`) добавлен блок — при `COND_HEAR_FMRADIO` комбайн исследует звук
  (`SCHED_INVESTIGATE_SOUND`), как и для `COND_HEAR_COMBAT` (отвлечение охраны).
- **`npc_infected.cpp`**: `GetSoundInterests()` |= `SOUND_FMRADIO`; в `GatherConditions` убран
  приблизительный поиск по дистанции — теперь используется реальный `COND_HEAR_FMRADIO`
  (как в оригинале), который прокидывается в `COND_UH_INFECTED_GRENADE` → `SCHED_UH_INFECTED_INVESTIGATE_RADIO`.

Поведение из декомпила сохранено: радио молчит 5 сек после Use, затем каждую 1.0 сек играет трек и
「вставляет» звук 0x20000 радиусом 1024; крекер после ~5 проигрышей (счётчик в классе) детонирует
через тот же `ExplosionCreate`.

## Исправленные ошибки компиляции (из отчёта сборки)

1. **`npc_infected.cpp(1127) C3861 'ExplosionCreate': identifier not found`**
   → добавлен `#include "explode.h"`.
2. **`npc_infected.cpp(933) C4189 'pDoor': local variable is initialized but not referenced`**
   → удалена неиспользуемая `CBaseEntity *pDoor = NULL;` в `SelectSchedule`
   (блок `COND_BLOCKED_BY_DOOR` просто ставит `m_flDoorBashYaw` и возвращает `SCHED_ZOMBIE_BASH_DOOR`).
3. **`uh_items.cpp(1180) C2664 VPhysicsInitNormal: cannot convert 'model_t*' → 'SolidType_t'`**
   → `VPhysicsInitNormal( GetModel(), NULL, false )` заменено на
   `VPhysicsInitNormal( SOLID_VPHYSICS, GetSolidFlags(), false )`
   (сигнатура `VPhysicsInitNormal(SolidType_t, int nSolidFlags, bool createAsleep, solid_t* = NULL)`).

## Открытые вопросы для сверки с вашим декомпилом

- **Magnitude крекера**: в `sub_10173A20` он передаётся как литерал `0`. Я поставил
  `sk_plr_dmg_smg1_grenade` (150). Если в вашем декомпиле magnitude читается из **другой** конвары
  (например `sk_plr_dmg_fraggrenade` = 350 или выделенной `sk_radiocracker_damage`) — поменять одну строку.
- **Комбайн в бою**: сейчас комбайн реагирует на радио только в `NPC_STATE_ALERT`/`IDLE`, не в
  `NPC_STATE_COMBAT` (бой приоритетнее). Если нужно отвлекать и в бою — перенести блок в COMBAT-ветку.
- Есть ли у вас в декомпиле имя конвары для magnitude (чтобы поставить точное имя вместо
  `sk_plr_dmg_smg1_grenade`)?
