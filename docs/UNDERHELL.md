# UNDERHELL — Inventory Reimplementation

Reimplement Underhell inventory system 1:1. Original: closed-source mod, Source Engine 2007 (Orange Box, episodic codebase). Reference: decompiled Hex-Rays output in [`klaxons1/underhell-hexrays`](https://github.com/klaxons1/underhell-hexrays).

This file: devlog. Written per caveman guidelines — terse, no fluff, substance only.

## Goal

- Build drop-in `client.dll` + `server.dll` for existing Underhell install.
- Preserve engine-visible surface: classnames, datamap field names, sendtable prop order, console commands, exported interfaces.
- Code must stay clean. Behavior unverifiable this stage = stub + `// TODO`.

## Compatibility rules (why each decision matters)

| Surface | Constraint |
|---|---|
| Exported symbols | `CreateInterface`, `ServerGameDLL003` / `ClientGameDLL003` factories — same as SDK. No changes needed. |
| Entity classnames | `item_apple` … from FGD Item List. `LINK_ENTITY_TO_CLASS` per class. |
| Sendtable prop order | Engine matches server↔client by index inside table, not name. Order must match original exactly: `m_HL2Local, m_fIsSprinting, m_bShoulderFlashlight, m_bFlashlightOn, m_bInventoryEnabled, m_iUHBatteryCount, m_iUHHermitCardsCount, m_iUHHermitCurrentQuestCount, m_iUHHermitTotalQuestCount, m_bDisplayHermitCard, m_iInventory[28]`. |
| Datamap field names | Save/restore is name-based. Original `CHL2_Player` datamap: `m_sndWaterSplashes`, `m_fSavedSensitivity`, `m_flArmorReductionTime`, `m_flTimeNextLadderHint`, `m_flTimeUseSuspended`, `m_hHermitCardCurrentQuestCounter`, `m_hHermitCardTotalCounter`, `m_hHermitCardTotalQuestCounter`, `m_hLocatorTargetEntity`, `m_iArmorReductionFrom`, `m_iInventory`, `m_pCarryingRagdoll`, `m_bShoulderFlashlight`. |
| Console commands | `switch <a> <b>`, `dropitem <slot>`, `useitem <slot>`, `emit`, `UpdateInventory` (server, client-executable), `cl_inventoryToggle` (client). |
| Class names | Underhell modified `CHL2_Player` / `C_BaseHLPlayer` directly. No new player class. Items derive from `CItem`. |

## Reverse-engineered facts (hexrays reference)

### Player inventory state — server (`CHL2_Player`)

Original byte offsets (sendtable sub_102DD5E0):

| Member | Offset | Type |
|---|---|---|
| `m_iInventory[28]` | 4928 | int array, 4-byte stride (slot value = item id, 0 = empty) |
| `m_bShoulderFlashlight` | 5040 | bool |
| `m_iUHBatteryCount` | 5044 | int |
| `m_iUHHermitCardsCount` | 5048 | int |
| `m_iUHHermitCurrentQuestCount` | 5052 | int |
| `m_iUHHermitTotalQuestCount` | 5056 | int |
| `m_bDisplayHermitCard` | 5060 | bool |
| `m_bFlashlightOn` | 5061 | bool |
| `m_bInventoryEnabled` | 5062 | bool |
| `m_HL2Local` | 5080 | embedded datatable, `SendProxy_SendLocalDataTable` |
| `m_fIsSprinting` | 5216 | bool (1 bit) |
| item classname table | 5236 | `const char*[34]`, indexed by item id |

Also: held-item handle at byte 2164 (right after `m_hActiveWeapon`), per-slot float array at 800 (HUD weights), per-slot sync array at 1020. TODO: port held-item handle + CBasePlayer additions (`m_UHObjectives` @2676, `m_bIronSighted` @2137, `m_StuckLast` @2176).

Client (`C_BaseHLPlayer`, sub_1018E280): same prop order, own offsets (`m_iInventory` @5052, `m_HL2Local` @5164, bools @5284-5288, ints @5292+).

### Item id table (34 entries, sub_102E27A0)

| id | Print name | Classname |
|---|---|---|
| 0 | none | — |
| 1 | Apple (Red) | item_apple |
| 2 | Apple (Green) | item_apple |
| 3 | Banana | item_banana |
| 4 | Burrito | item_burrito |
| 5 | Sandwich | item_sandwich |
| 6 | Banana Bunch | item_bananabunch |
| 7–12 | Soda (6 kinds, one "Mega Soda") | item_uhsoda |
| 13 | Flare | item_flarepack |
| 14–18 | GlowStick Red/Yellow/Green/Blue/Purple | item_glowstick |
| 19–23 | Lit GlowStick (same colors) | `"nothing"` — no world entity |
| 24 | PainKillers | item_painkillers |
| 25 | Syringe | item_syringe |
| 26 | Bandages | item_bandages |
| 27 | Healthkit | item_healthkit |
| 28 | Health Vial | item_healthvial |
| 29 | Chocolate Bar | item_chocobar |
| 30 | Orange | item_orange |
| 31 | FM Radio | item_fmradio |
| 32 | Radio Cracker | item_radiocracker |

TODO: verify exact soda/health print names from hexrays string dump.

### Mechanics (decoded)

- **Use** (`UH_ItemAction`, vtable idx 411 — first Underhell virtual): read id from slot. Create world entity from classname table. Fail + id 19–23 → consume slot, remove lit glowstick child (warn if none). Success → place at eye position, spawn, call `Use( player, player, USE_ON, id )`. Glowstick 14–18 → slot becomes `id+5` (lit). Then clear slot, `engine->ClientCommand( edict, "UpdateInventory" )`.
- **Drop** (`dropitem`, same virtual with bUse=false): style + drop world item at eye position (body groups per id; flare → flare prop; glowstick → coloured glowstick prop). TODO: second drop path (sub_102DE310, eye + forward*56 + up*64, classname indexed by slot) still to reconcile.
- **Specials 19–23**: equip logic (sub_10416380) — decrement counter at player+848, remove held entity, consume slot, resync. TODO: port.
- **`UpdateInventory`** (server handler sub_102DDDE0): per non-empty slot `StateChanged( 4928+4*i )` (or dirty flag), remove held item, clear handle, `engine->ClientCommand( "UpdateInventory" )`.
- **Console commands use the client-command route**: `emit`/`switch`/`dropitem`/`useitem` are NOT registered ConCommands — the engine forwards them as client commands into `CHL2_Player::ClientCommand` (the Underhell override = hexrays sub_102DDBF0, falls through to the vanilla handler which owns `emit`). Client panel sends them via `engine->ClientCmd`. TODO: verify 2007 engine forwards `UpdateInventory` when the client dll also registers it (resync is belt-and-braces in our port since CNetworkArray marks changes itself).
- **Client**: `CInventoryPanel` = `vgui::Frame` named `InventoryPanel` (factory class `CInventoryPanel`), 28 slot children, vgui messages `NewSelection` / `NewMouseReleased`. Debug msg on construct: `"InventoryPanel has been constructed"`. `cl_inventoryToggle` → `engine->ClientCmd("UpdateInventory")` + toggle panel. Client `UpdateInventory` cmd sets refresh flag. OnThink gates on player alive + `m_bInventoryEnabled` (+ an unidentified bool at client player offset 3681 — TODO).
- **Slot visuals** (client, sub_1012E6C0): per-id sprite + localization token, replicated 1:1 — ids 24/25 (painkillers/syringe) have text only; id 12 = SodaPowerPunch / `#UnderHell_Inventory_MegaSoda`; **glowstick icon colours do not match the print names** (id 14 "Red" uses GlowstickGreen icon) — quirk preserved.

### Item entities

Full classname list: `FGD/Item List.txt` (food, ammo, equipment, health) + `Weapon List.txt` (weapons, later stage). Plus `item_random` (keyvalues `disableshadows`, `respawn`, `nothing`; input `Respawn`). Precache list (sub_101753E0) also has `item_painkillers`, `item_syringe`, `item_battery_pack`, `item_ammo_ar2_altfire`.

## Stage log

| # | Commit | Stage |
|---|---|---|
| 1 | docs | devlog + findings |
| 2 | feat(shared) | item id registry + name table |
| 3 | feat(server) | item entity classes (stubs) |
| 4 | feat(server) | CHL2_Player inventory state + use/drop + commands |
| 5 | feat(client) | CInventoryPanel + network props + client commands |
| 6 | build | vcproj registration |
| 7 | feat(server) | endurance/hunger + stamina-recharge gating |
| 8 | feat(server) | functional food/health item use effects |
| 9 | feat(client) | CHudEndurance green bar + endurance props |
| 10 | feat(server) | healthkit/healthvial go into inventory + heal on use |
| 11 | fix(client) | endurance/stamina HUD (vertical/horizontal FgColor bars) |
| 12 | fix(server) | +use-only pickup (no auto-pickup / no physcannon grab) |
| 13 | feat(server) | flashlight on batteries (item_battery / item_batterypack) |
| 14 | fix(client) | inventory toggle closes on second "I" |
| 15 | fix(client) | bars hidden without suit; inventory not suit-gated; toggle debounce per-frame |
| 16 | feat(client) | CHudUHBattery (contour + charge + count) + CHudBleeding |
| 17 | feat(server) | bleeding system + passive hunger decay |
| 18 | feat(client) | CHudUHHermitCards (deck icon + count + quest progress) |
| 19 | feat(server) | working glowsticks (lit prop strapped to player) |
| 20 | fix(server) | healthkit/healthvial always stash into inventory (+use) |
| 21 | fix(client) | battery/cards HUD fade rate (icons now persist) |
| 22 | fix(server) | battery item (pg_battery model, +1/+2, max 20, removed on pickup) |
| 23 | fix(client) | HUD panels pinned to original geometry (no proportional blow-up) |
| 24 | feat(client) | flashlight battery charge (m_flUHBatteryCharge) drains smoothly |
| 25 | feat(server) | npc_infected functional port (variants + fast claw AI) |

## Progress

Stage 1–6 done (first batch):

- Shared: item id enum + id→classname/print-name table (exact original strings).
- Server: 40 item entity classes registered under original classnames (Spawn/Precache stubbed, models from FGD), `item_random` stub, `CHL2_Player` inventory state (sendtable in original order, datamap per original save format), `UH_ItemAction` use/drop logic ported 1:1, `switch`/`dropitem`/`useitem` via `ClientCommand`, `UpdateInventory` resync command, `uh_give_item` dev cheat.
- Client: `C_BaseHLPlayer` inventory props (recv table matches server order), `CInventoryPanel` loaded from the original `resource/UI/InventoryPanel.res` + 28 slots (4x6 grid + row of 4, per original creation loops) + original sprite/token table, `cl_inventoryToggle` + `UpdateInventory` commands.
- Build: sources registered in both episodic vcproj files.

Next stage: item pickup flow (`CUHItem::MyTouch` → `UH_AddInventoryItem`), per-item use effects, held-item handle, equip counter at player+848.

## Endurance / hunger system (stage 7–9)

Two HUD bars in the original: **red = suit power** (the vanilla sprint
"stamina", `m_HL2Local.m_flSuitPower`) and **green = endurance** (`m_iEndurance`,
the "hunger" meter). The green bar is restored by eating/drinking and consumed
as stamina recharges — lower endurance = slower stamina recharge.

Decoded from the original (sub_102E0E60 recharge branch, sub_102DF1A0 eat,
sub_102DF2E0 drink):

- `m_iEndurance` @2184 (int, CBasePlayer), `m_iBleedCounter` @2188 (int) —
  both networked (client recv table sub_10043D70 reads them @3432/@3436).
  Server-only accumulators: `m_fEStaminaCount` @2132, `m_flPseudoEndurance`
  @2148, `m_flPseudoHealth` @2144, `m_iEHealthCount` @2160 (runtime only).
- ConVars (original registrations): `uh_player_endurance` 100,
  `uh_player_endurance_rate` 1600, `uh_player_endurance_rate2` 8000,
  `uh_player_endurance_stamina_effect` 100, `uh_player_bleed_rate` 8000,
  `uh_bleeding_chance` 5.
- **Recharge** (`SuitPower_Update` → `UH_UpdateEndurance`): when the suit
  would recharge (devices off, <100 power, 0.5s delay),
  `rate = max(endurance, 25) * 0.01 * 12.5 * frametime`; charge by `rate`;
  `m_fEStaminaCount += rate`; when it reaches 50 → `--endurance` (clamped 0).
- **Eat** (`UH_Eat`, sub_102DF1A0): endurance += gain (clamp 100), health +=
  gain (clamp 200 — food can overheal), play sound. **Drink** (`UH_Drink`,
  sub_102DF2E0): same, but Mega Soda flavour (id 12, flavour 5) multiplies
  endurance gain by 2.5, plays "Player.Drink".
- Food per-item eat gains (decoded + localization): apple 5/1 ("Player.Eat.
  Apple"), banana 5/1, burrito 5/1, chocobar 5/1, orange 5/1, sandwich 10/1,
  banana bunch 15/3, soda drink 10/1 (flavour = id − 7).
- Food `Use()` gate (sub_10171E10 etc.): alive + not gas-masked + CHL2_Player.
  Gas-mask check is a TODO until the gear system is ported.
- Bandages `Use()` (sub_101725C0): usable while hurt/bleeding; stops bleed +
  heals 5 while bleeding, else heals 1.
- Client `CHudEndurance` (sub_100C8680/sub_100C8710): blue bar (HullColor
  "0 0 255 255") driven by `m_iEndurance`; <20 → "EnduranceLow", <50 →
  "EnduranceMedium", ≥50 → "EnduranceHigh" animation sequences. The original
  uses a modded-vgui panel (settings "HullColor"/"HullDisabledAlpha"/
  "BarInsetX"/"icontall") not present in the OB SDK — our `CHudEndurance` is
  a faithful behavioural clone of the vanilla suit-power bar.

### TODOs (endurance scope)

- Melee stamina drain: Underhell melee weapons carry a `StaminaToDrain` stat
  (default 15.0, parsed in sub_10274870) that drains suit power on swing —
  blocked on the Underhell melee-weapon classes (not yet ported).
- Kick: explicit TODO (no kick implementation in this repo yet).
- Full bleeding system: damage→bleed roll (`uh_bleeding_chance`), `BleedThink`
  health drain (`uh_player_bleed_rate`), and the health-vial slow-bleed effect.
  `m_iBleedCounter`/`m_flLastBleedTime` are in place and wired to bandages.
- Painkillers/syringe heal amounts are a reconstruction (no RTTI for the
  original classes; quest items with text-only slots).

## HUD bars (stage 11) — decoded from scripts/HudLayout.res + HudAnimations.txt

The mod ships its own `scripts/HudLayout.res` / `scripts/HudAnimations.txt` /
`resource/ClientScheme.res`, so the panel names, geometry and colours below
are authoritative (not reconstructed):

- **HudStamina** (horizontal bar, xpos 32 ypos 448 wide 240 tall 18): the
  Underhell stamina bar replacing the vanilla CHudSuitPower. Bar geometry:
  `BarInsetX 26, BarInsetY 7, BarWidth 210, BarHeight 4, BarChunkWidth 1,
  BarChunkGap 0`. Reads suit power; `< 35` → "StaminaLow" (red), else
  "StaminaNormal" (scheme FgColor).
- **HudEndurance** (vertical gauge, xpos 10 ypos 332 wide 18 tall 134): the
  hunger bar. `BarInsetX 7, BarInsetY 104, BarWidth 4, BarHeight 84,
  BarChunkHeight 1, BarChunkGap 0` — fills bottom-up. Reads `m_iEndurance`;
  `< 20` → "EnduranceLow", `< 50` → "EnduranceMedium", else "EnduranceHigh".
- **Colour model**: both bars draw with the panel **FgColor**, animated by
  HudAnimations.txt: scheme `FgColor` = `0 128 255` (blue), endurance medium
  = `230 230 50` (yellow), `DamagedFg` = `180 0 0` (red). So blue → yellow →
  red as the meter drains (the user's observation: blue, yellowing/reddening
  toward zero). The stamina bar only goes blue → red (two states).
- The original bars also draw an icon/contour sprite (`sprites/hud/hud_stamina`,
  `sprites/hud/hud_endurance`) and use `PaintBackgroundType 2` (rounded box).
  The port draws the chunked bar only; the icon sprite is a TODO.

## Pickup model (stage 12) — verified: no auto-pickup

Underhell items are **not** auto-picked by touch and are not grabbed as
physics props:

- `CItem::ItemTouch` (sub_10177A20) still exists and calls MyTouch, but
  Underhell items never register it: `CUHItem::Spawn` clears the touch
  handler (`SetTouch(NULL)`) so walking over an item does nothing.
- `+use` picks the item up: `CUHItem::Use` → `MyTouch`. Consumables
  (food/health) only apply their effect when called with `USE_ON` (the
  inventory `useitem` path from UH_ItemAction); any other use type forwards to
  MyTouch, so world `+use` always takes the item instead of eating/healing it.
- The vanilla CHealthKit / CHealthVial follow the same model (Spawn clears
  touch; Use dispatches on USE_ON).
- Weapons are still vanilla `BumpWeapon` (touch) — TODO: verify Underhell also
  requires +use for weapons (likely, same "no auto-pickup" rule).

## Flashlight batteries (stage 13)

Underhell's flashlight runs on discrete batteries (`m_iUHBatteryCount`), not
suit power:

- `item_battery` (vanilla CItemBattery) grants **1** battery on pickup instead
  of charging suit armour.
- `item_batterypack` (CItemBatteryPack) grants **5** batteries (count TODO —
  verify against original).
- `FlashlightTurnOn` requires a battery; `UH_UpdateFlashlightBattery` (hooked
  into PreThink) drains one battery every `uh_flashlight_battery_time` seconds
  (default 60) while the light is on, and switches it off at zero.
- The original is a full viewmodel system (shoulder flashlight, holster
  animation, `FlashlightViewModelThink`, `item_flashlight`/`item_shoulder-
  flashlight` equipment, `m_bShoulderFlashlight`) — still TODO; this ports the
  core battery mechanic on the vanilla EF_DIMLIGHT flashlight.

## Battery + bleeding HUD (stages 16–17)

From scripts/HudLayout.res / decompile:

- **HudUHBattery** (xpos 8 ypos 200 wide 40 tall 64): contour sprite
  `sprites/hud/hud_battery_contour` (contourx 1 contoury 0 contourwide 24
  contourtall 42), a vertical chunked bar (BarInsetX 6 BarInsetY 31 BarWidth
  14 BarHeight 23 BarChunkHeight 2 BarChunkGap 1, HullColor "2 127 252 192"),
  and the discrete count printed as "x<N>" (sub_100BDC80). The original fades
  the whole gauge when stable and brings it back on battery-count / flashlight
  / nightvision changes (sub_100BDF90). The bar fill reads a 0..100 float
  (client offset 5212 — an Underhell-specific field whose identity is not
  recoverable); our port fills one chunk per battery (count-capped).
- **HudBleeding** (xpos 248 ypos 408 wide 24 tall 36): `sprites/hud/
  hud_blooddrop` tinted red, alpha = m_iBleedCounter * 2.55 (sub_100BE800),
  shown only while bleeding.
- Both use HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT (the
  original sub_100B3790(this, 56) = 0x38).

### Bleeding mechanic (server, decoded from sub_101EF960 PostThink)

- `m_iBleedCounter` (@2188) is the wound severity; `m_iEndurance` (@2184) the
  hunger meter. Runtime floats: `m_flPseudoHealth` (@2144), `m_flPseudoEndurance`
  (@2148), `m_flLastBleedTime` (@2152), previous-tick timestamp (@2156),
  `m_iEHealthCount` (@2160).
- Start: taking damage rolls `uh_bleeding_chance` (5%) and adds to
  `m_iBleedCounter` (exact damage→bleed scaling not recoverable — this port
  adds 1 bleed point per damage point, clamped 100).
- Per think while bleeding: rate = bleedCounter * 0.006, halved if it rounds to
  zero, doubled while sprinting, reduced by endurance * 0.00075. Damage
  accumulates in m_flPseudoHealth and is applied in whole HP (spawning a blood
  drip); each point decrements bleedCounter, and at <=10 the wound closes.
  Bleeding cannot take the last HP (original sends "kill" at health 0).
- Passive hunger decay: endurance drains (0.2 - health * 0.000875) per second,
  applied in whole points — so hunger falls faster at lower health.

## Hermit cards HUD (stage 18)

From scripts/HudLayout.res + decompile:

- **HudUHHermitCards** (xpos r124 ypos 16 wide 128 tall 100): contour sprite
  `sprites/hud/hud_hermitcards` (contourx 34 contoury 10 contourwide 78
  contourtall 54), the collected count printed as "<N>/52" (text1x 78 text1y
  30, HudNumbers font), and — while a quest is active — quest progress
  "   <cur>/<total>" (text2x 10 text2y 30). Paint sub_100BD080.
- Visibility (think sub_100BCFA0): appears when `m_iUHHermitCardsCount` or
  `m_bDisplayHermitCard` changes, fades out ~3 s after the last change. The
  deck icon therefore only appears once the first card is collected.
- Server (sub_102E1B60): the counters are copied from game stats
  `GC_HermitCards` / `GC_HermitQuest_Total` / `GC_HermitQuest_Current` into
  the networked fields; `m_bDisplayHermitCard` flips true once cards exist.
  The Underhell game-stats system is not ported, so the port exposes
  `CHL2_Player::UH_SetHermitCards()` + a `uh_give_hermit_card` cheat instead.

## Glowsticks (stage 19)

Decoded from the original `item_glowstick` Use() (sub_101742D0):

- **Use** (unlit 14–18, from the inventory): create a `prop_physics` with
  `models/pg_props/pg_obj/pg_glow_stick.mdl`, set the colour via bodygroup
  (red/yellow/green/blue/purple = 0/2/4/6/8), place at the player's origin
  + 36 z, add EF_BONEMERGE | EF_NOSHADOW, make it non-solid, then parent it to
  the player and remember it in `m_hActiveGlowStick` (@2164). The model is
  self-illuminated so no separate light entity is needed (matches the
  original, which only creates an `env_flare` for the flare item).
- **Use again** (lit 19–23): the slot has no world entity ("nothing"), so the
  handler removes `m_hActiveGlowStick` and clears the slot.
- **Drop** (unlit 14–18): `UH_SpawnItemInWorld` already drops a lit coloured
  `prop_physics` glowstick.

## Health pickup (stage 20)

`item_healthkit` / `item_healthvial` now **always** stash into the inventory
on +use while a free slot exists (no health gate). Only when the inventory is
full does it fall back to healing on touch. Using the kit from the inventory
heals `sk_healthkit` / `sk_healthvial` and stops / slows bleeding
(sub_102F07A0 / sub_102F08D0).

## Pickup / item_random decode (from RTTI + datamap blob + vtables)

- **Class hierarchy**: `CItemRandom : CWorldItem : CItem : CBaseAnimating`.
  `CWorldItem` adds `m_iType` + a `"type"` keyvalue; its Spawn only handles
  types 44 (item_battery) and 45 (item_suit), warning `"unable to create
  world_item %d"` otherwise — a legacy wrapper.
- **CItemRandom::Spawn** (sub_101757D0): roll 0..99; spawn when
  `roll + 1 > m_inothing * sk_itemrandom{1,2,3}[skill]`. Pool = 77 entries
  (0-based ids from the original switch): 0..25 food/gear, 26..43 ammo,
  44..74 weapons, 75/76 radios. Spawned item inherits spawnflags; the
  `disableshadows` key carries `EF_NOSHADOW` (0x10, matching const.h).
  Empty pool → `Msg("item_random item possibilites count is 0\n")`.
  `m_hOldItem` tracks the spawned item; `InputRespawn` re-rolls.
- **CItemRandom datamap extracted from the original .data blob** (82 fields,
  52-byte typedescription_t): every keyvalue name + offset recovered
  (m_bitem_* / m_bweapon_* at 804..880, m_inothing=884, m_bRespawn=888,
  m_hOldItem=892). Keyvalue names = the pool classnames.
- **Pickup flow** (CItemBanana::MyTouch, sub_101720D0): RTTI-cast to
  CHL2_Player → free-slot gate (sub_10171D30 != 28) → sound
  `HL2Player.PickupItems` (per-item extras like PickupBandages/PickupArmor)
  → SetOwnerEntity → `CHL2_Player` vtable [410] = **UH_GiveItem** →
  UTIL_Remove.
- **[410] UH_GiveItem** (sub_102726310): first free slot, else the item is
  spawned back into the world at eye + forward*56 + up*64 (drop-on-full).
- Item Spawn details: apple skin random 0/1 (red/green) picks the inventory
  id on pickup; soda skin picks flavour; armour gates on armour < 100 and
  grants 10 (TODO-verify); bandages gate on hurt/bleeding.
- Vanilla items stay vanilla (CItemBattery/CHealthKit/CItemSoda/item_sodacan/... untouched). Inventory sodas are item_uhsoda.

## RTTI + vtable validation (from original binaries)

RTTI dumps (clientRTTI.txt / serverRTTI.txt) + PE parsing of Cliento.dll /
serveror.dll let the vtables be read directly. Key results:

- **CHL2_Player vtable** (serveror.dll @ 0x1055069C):
  - `[328]` = sub_102DDBF0 = `ClientCommand` override (switch/dropitem/useitem dispatch) — our client-command routing design matches the original exactly.
  - `[409]` = sub_102E27A0 = item name-table builder (a CHL2_Player method).
  - `[410]` = sub_102DE310 = the world-drop routine (eye + forward*56 + up*64, per-id prop styling) — same switch as [411], used by a different call path (TODO: find caller).
  - `[411]` = sub_102E05F0 = `UH_ItemAction` (use; dropitem calls it with bUse=0) — attribution verified.
  - `[412]..[423]` = further Underhell player virtuals (TODO: decode).
- **Client classes**: `IInventoryPanel` (4 entries, [0]=dtor), `CInventoryPanelInterface` (4 entries: [1]=create panel via `new`, [2]=cleanup+delete, [3]=re-run slot creation), `CInventoryPanel` (259 entries; [96]=OnThink confirmed, [30]=dtor with slot loop, [27..29]=settings virtuals), `vgui::DragnDropSlot` = the 28 slot panels (a modded-vgui class, NOT in the OB SDK — our CInventorySlotPanel stands in).
- **Item classes** (serveror.dll): per-class Spawn/Precache override CItem slots [24]/[25] (CItemRandom::Precache = the registry list sub_101753E0); shared bool-returning `CItem::Use` at [64] (vanilla physcannon-impulse logic — original modified the base to return bool; our SDK's Use is void, so the failure path stays TODO).
- Original keeps vanilla item classes (CItemBattery, CHealthKit, CHealthVial, CItem_Box*Rounds, CItem_RPG_Round, ...) — our port mirrors that split.

## Save-file validation (inventory_is_full.sav)

Original save parsed. Confirmations:
- Datamap field names in the file match the implementation: `m_iInventory`, `m_bInventoryEnabled`, `m_bShoulderFlashlight`, `m_iUHBatteryCount`, `m_HL2Local` all present.
- Player inventory array decoded: 28 x 4-byte ints, empty slot = 0. The "full" save holds 27 items — values all within 1..32 and consistent with the id table (2=apple, 5=sandwich, 10/11=soda, 14/18=glowsticks, 26=bandages, 27=healthkit, 28=healthvial, 29=chocobar, 30=orange). No 19..23 saved — lit glowsticks are runtime state, as expected.
- Field records: `u16 fieldIndex, u16 dataLength, data` — the inventory record's length field = 112 (28 x 4). Matches the network array layout.

## Mod asset findings

- `resource/ui/inventorypanel.res`: panel = `CInventoryPanel`, x=368, y=84, wide=1024, tall=512, titlebar visible, `Texture1 = Sprites/Hud/Inventory/Inventory`, PaintBackgroundType 1. OB-era vgui has no `Texture1` key (newer-vgui feature) — background applied manually in our port.
- Slot structure: `Inventory.vtf` is 1024x512. `.res` `PaintBackgroundType 1` + `Texture1` = engine `DrawTexturedBox` — the VTF is **stretched to the live Frame rect**, not drawn 1:1 and not centered. `.res` places the Frame at **(368,84)** size 1024×512 (raw pixels; Frame is not proportional). The metal plate sits in the middle of that canvas ~(174,87)–(849,424); pockets are an **8×4 grid, pitch 84**. 28 game slots fill **row-major** (3×8 + 4) by UV-mapping those texels through `GetWide()`/`GetTall()` so a scaled Frame still keeps icons in the pockets. Hexrays 28/37/44/119 are `GetProportionalScaledValue` leftovers and do not land on this VTF. LMB/RMB Use/Drop menu as before.
- Sprites: `materials/Sprites/Hud/Items/*.vmt` — every name from the id switch exists (128x128). `Inventory.vtf` = 1024x512 background (DXT5). `Blank.vtf` = 1x1 with alpha 0 (empty slots invisible — a "blank" panel is the original behaviour, minus the background texture).
- Localization (`resource/Underhell_english.txt`, UTF-16): all `UnderHell_Inventory_*` tokens exist; no Painkillers/Syringe tokens (quest items, text-only slots). Item descriptions document real effects: food = endurance + health, bandages stop bleeding, healthvial slows bleeding, radio/radiocracker attract enemies after 5s, flare ignites enemies, glowsticks strap to waist.
- Engine: build 4104 (OB Ep2, Feb 2010). `sv_sendtables` / `sv_dump_class_info` are 2013-era commands — unavailable here. Verification instead comes from the hexrays sendtable decode + the save file.

## Command-flow note (important)

`UpdateInventory` must NOT be a server ConCommand. Original message flow: client registers it plain (no FCVAR_CLIENTCMD_CAN_EXECUTE), so `engine->ClientCmd("UpdateInventory")` (from cl_inventoryToggle) is NOT run locally but forwarded to the server → `CHL2_Player::ClientCommand` dispatches it → resync → server sends `engine->ClientCommand(edict, "UpdateInventory")` → client executes its local handler (refresh flag). A server-side ConCommand registration would double-handle the message and risk a client/server ping-pong. Our port mirrors the original flow exactly.

## TODOs (tracked)

- [ ] Full per-item effects (health, armor, food, ammo, batteries, radio) — stubbed.
- [ ] Pickup (touch) flow: item entity → player add.
- [ ] Item stacking/count semantics.
- [ ] CBasePlayer additions port: `m_UHObjectives`, `m_bIronSighted`, `m_StuckLast`, held-item handle at 2164, per-slot arrays 800/1020.
- [ ] `CHL2PlayerLocalData` exact Underhell contents (original trimmed to 136 B).
- [ ] Original prediction data sets (server + client) — verify against hexrays.
- [ ] Verify all datamap field offsets vs original (sub_102E21B0).
- [ ] Weapons as inventory items.
- [ ] item_random pool behavior.
- [ ] Post-build copy path → mod dir (currently copies to `game/hl2/bin`).
- [ ] Client slot panel visuals + icon mapping (vgui materials).

## npc_infected (stage 25)

`CNPC_UH_Infected` (classname `npc_infected`) is a fast feral zombie. This is a
functional port on CAI_BaseNPC + ai_default schedules (the original carries a
16-entry vtable with custom climb/sprint/infection logic that is out of scope
for one pass). Faithfully reproduced from the FGD + string table:

- 8 body variants (inmate/guard/worker/rural/doctor/uniform/office/urban),
  each with a "disable" keyvalue restricting the random pool. Models
  `models/infected/infected_*.mdl`.
- `SpeedModifier` (0..1, blank = random) — parsed; actual run-speed scaling
  waits on the infected's animation ground-speed (TODO).
- `additionalequipment` melee weapon — parsed; the Underhell melee weapons
  (`weapon_melee_pipe` etc.) are not ported yet, so the innate claw attack
  covers melee.
- Random limb loss flag (`m_bInfectedFlag`) — visual limp TODO.
- `OnSpotInfectedBody` output fired on death.
- Intelligence: standard NPC senses + hunt/chase/melee; a custom fast claw
  attack (AE_NPC_ATTACK_BROADCAST -> direct DMG_SLASH + lunge) and a
  climb-touch stub. `SCHED_MELEE_ATTACK1` when in range.

TODO: climb animation, sprint speed, infection spread, gibs, the weapon give.

## Battery items (stage 22)

From the original: `item_battery` uses `models/PG_props/pg_obj/pg_battery.mdl`
and grants **+1** battery; `item_batterypack` grants **+2**; both cap at
**20** (`m_iUHBatteryCount`). Both are +use only (no touch auto-pickup) and
remove themselves on pickup. The flashlight charge is a networked float
`m_flUHBatteryCharge` (0..100) that drains continuously over
`uh_flashlight_battery_time`, giving the HUD a smooth meter (the original
reads the same value from a float at client offset 5212, inside
C_HL2PlayerLocalData — ported as a plain CHL2_Player network var).

## HUD geometry (stage 23)

The Underhell `scripts/HudLayout.res` is a mod asset not present in the SDK,
so the ported HUD panels had no size and their bar/contour offsets were
proportional-scaled (blown up ~2.25x at 1080p). Each panel now pins itself to
the original .res geometry (SetProportional(false) + SetSize + SetPos) and
uses raw-pixel offsets, so the battery/cards/bleeding icons and the
stamina/endurance bars render at their intended size.

## Weapon system (stage 26 — foundation)

Weapon scripts (`scripts/weapon_*.txt`) are complete mod assets carrying all
the Underhell tuning (StaminaToDrain, MeleeRange, PunchPitch/Yaw, ExpOffset
ironsight, ...). The SDK's `weapon_parse` reads them automatically, so the
port only needed to:

- **Extend `FileWeaponInfo_t`** (shared) with the Underhell keys, parsed in
  `FileWeaponInfo_t::Parse` (decode sub_10274870): `OneHanded`,
  `MeleeDelayedFire`/`MeleeRoF`/`MeleeRange`, `StaminaToDrain`,
  `PunchPitch`/`PunchYaw`/`SnapPitch`/`SnapYaw` ("min, max" ranges),
  `CrouchRecoilMult`/`CrouchAccuracyMult`/`RunAccuracyMult`,
  `UH_Weapon_Special`→`Penetration`, and `ExpOffset` (ironsight x/y/z +
  xori/yori/zori + accuracy).
- **21 weapon classes** (classnames from serveror.dll RTTI + Weapon List.txt):
  melee axe/baton/pipe/wrench/cleaver → `CBaseHLBludgeonWeapon`; pistols
  (glock/beretta/socom/python/dualberetta), SMGs (mp5/mp5_eod/mp7), shotguns
  (m3/m5/spas12/xm1014), rifles (g36k/sniper), BFG (mgl/minigun) →
  `CBaseHLCombatWeapon`. Matching thin client stubs in `c_uh_weapons.cpp`.
  Vanilla weapons stay vanilla.
- **impulse 101** gives the full Underhell set (CHL2_Player::CheatImpulseCommands
  case 101 → `UH_GiveAllWeapons`).
- **One weapon per slot**: `Weapon_Equip` ejects the occupied slot's weapon.
- **Melee stamina**: `CUHMeleeWeapon::PrimaryAttack` drains `StaminaToDrain`.

### TODO (weapon stage)

- Per-weapon damage / fire rate / recoil application (currently melee damage
  is a hardcoded per-class value; guns use the base fire path).
- Ironsight (ExpOffset) viewmodel offset + accuracy — client viewmodel work.
- Silencer + laser sight toggles (script data exists; needs networking + viewmodel).
- Free-aim camera (viewmodel lags the crosshair) + dynamic hand switching
  (OneHanded / BuiltRightHanded / cl_righthand flip).
- NPC acttables for the melee weapons (player works, NPCs TODO).

## Weapon system origins (notes/ in the hexrays repo)

The mod author (Mxthe) documented his sources in `notes/`:

- **`ironsight.txt`** — the VDC **"Adding Ironsights"** tutorial (jorg40/Cin).
  The `ExpOffset { x y z xori yori zori accuracy }` block in every weapon
  script is this system verbatim: `CBaseViewModel::CalcViewModelView` slides
  the viewmodel up to the eye by `m_expOffset`/`m_expOriOffset`, interpolated
  by `m_expFactor` over `gMoveTime`. The port parses these into
  `FileWeaponInfo_t::m_expOffset` / `m_expOriOffset` / `m_flAccuracy`.
- **`Over the Shoulder View - Valve Developer Community.html`** — the VDC
  **"Over the Shoulder View"** tutorial. Its MAIN body is a THIRD-PERSON
  over-the-shoulder camera (the `cam_ots_*` convars: offset, offset_lag,
  origin_lag, shake_*, translucencyThreshold). Its "OPTIONAL: Adding free aim"
  section is a SEPARATE FIRST-PERSON feature: the mouse moves the crosshair on
  screen (deadzone + auto-turn past the edge), decoupling the aim point from
  the view, and the weapon VIEWMODEL tilts/rolls toward the crosshair — the
  "weapon sways toward the mouse when not aiming" behaviour. Do NOT conflate
  the two: OTS = third person; free-aim = first person.

### One-handed weapons & the flashlight (user-confirmed + decoded)

`OneHanded` exists because Underhell has a **flashlight that is itself a
viewmodel** (`models/weapons/v_flashlight_pg.mdl`, `FlashlightViewModelThink`,
`m_bFlashlightHolstered`, convar `uh_flashlight_anim`). While a one-handed
weapon (pistols, melee) is equipped, the left hand holds the flashlight (or a
**fake flare** — `models/weapons/v_flare_pg.mdl`, `m_bHoldingFlare`). The
player precache (sub_101E25F0) also pulls `v_kick_jake_*.mdl` — the kick
attack viewmodels (distinct from weapons).

### First-person hand/glove skin (map-driven, not a cvar)

The hands on the viewmodel (`models/weapons/v_hands.mdl`) change texture via
**entity inputs fired at `!player`**, not a convar:

- `ViewModelSkin <int>` (input func sub_101EEE40) — sets `m_nSkin` (offset 848)
  on the player's two viewmodels; this is what changes the hand/glove texture.
- `SetPlayerSkin <int>` — sets the player model skin.
- `SetPlayerModel <model>` — swaps the player model (default
  `models/player/jake_casual.mdl`; guard/PMC variants exist).
- `SetPlayerKickModel <model>` — swaps the kick-attack viewmodel
  (`models/weapons/v_kick_jake_*.mdl`).

Example from `uh_prologue_2_d.vmf` (PMC): `OnNewGame` fires
`!player,SetPlayerModel,models/player/jake_pmc.mdl` +
`setplayerkickmodel,models/weapons/v_kick_jake_pmc.mdl` +
`Viewmodelskin,3`. In `Uh_House_1_d.vmf` a trigger fires
`!player,setplayerskin,1` and `!player,ViewmodelSkin,0`. There is **no**
`uh_hand*` cheat convar in the original binaries — the player inputs above are
the whole mechanism (the closest cheat is `ent_fire !player viewmodelskin N`).

### Silencer / laser sight (from the string table)

- **Silencer**: player flags `m_bHavePistolSilencer` @3371 /
  `m_bHaveRifleSilencer` @3372 (set via `SetPistolSilencer` /
  `SetRifleSilencer` inputs), client `silencer_toggle` command, viewmodel
  animations `ACT_VM_ATTACH_SILENCER` / `ACT_VM_DETACH_SILENCER`, and a
  `single_shot_silenced` sound key in the weapon scripts.
- **Laser sight**: `sprites/laserpointer.vmt` / `sprites/laserdot.vmt` /
  `sprites/laserbeam.vmt`, `m_bLaserToggleState`, `Valve_Hint_LaserSight`.

### FireMode

`UH_Weapon_Special` also carries `"FireMode"` (only the G36K sets it, value 1
= full auto; matches FIREMODE_FULLAUTO in basehlcombatweapon.h). Parsed as
`FileWeaponInfo_t::m_iFireMode`.

## Ironsight (stage 27)

Port of the VDC "Adding Ironsights" system (jorg40/Cin) that Underhell uses.
Decoded 1:1 from the original toggle (`server/sub_101ECF40`), the viewmodel
send/recv tables (`sub_100F8EA0` / `sub_10015160`) and the player sendtable
(`sub_101E6C70`).

State (all networked, mirrors the original binary):

- `CBaseViewModel::m_bExpSighted` — networked bool (server @1120, client
  @1960) that the server toggles; the client `CalcViewModelView` reads it to
  slide the viewmodel up to the eye by the weapon's `ExpOffset`
  (`m_expFactor` interpolates 0 hip → 1 sighted over ~0.1 s).
- `CHL2_Player::m_bIronSighted` — authoritative flag (accuracy + HUD).
- `CHL2_Player::m_fIronsightedTime` — last toggle time (0.1 s debounce).

Server toggle (`UH_ToggleIronsight`, decode `sub_101ECF40`):

1. Debounce 0.1 s.
2. Preconditions: active weapon exists and is not a melee weapon (weapon-info
   `MeleeWeapon` flag → `m_bMeleeWeapon`, hexrays `sub_100D0E00` reads info
   offset 1832). TODO: the original also gates on a weapon flag @1144 and
   `m_bHardLowered` (each with an `|| m_bIronSighted` escape) — not ported.
3. Drop sprint (`StopSprinting`).
4. `m_Local.m_iHideHUD ^= HIDEHUD_CROSSHAIR` (original toggles bit 256).
5. Toggle the viewmodel `m_bExpSighted` (networked).
6. Enter: `ClearUseEntity()`, `HL2Player.Ironsighton`, FOV =
   `GetDefaultFOV() * uh_ironsight_zoom` (default 0.9 → mild zoom; the real
   sighting effect is the viewmodel slide), `m_bIronSighted = true`, and
   `m_flMaxspeed = uh_ironsight_zoom_focus` (default 40) if lower.
7. Leave: `HL2Player.Ironsightoff`, `m_bIronSighted = false`, FOV = 0
   (default), restore `m_flMaxspeed = hl2_walkspeed` (150) if higher.
8. `m_fIronsightedTime = curtime`.

Key corrections vs. the earlier (wrong) reading of the hexrays:

- **`uh_ironsight_zoom` (0.9) is a FOV *multiplier*, not a transition rate**:
  `FOV = GetDefaultFOV() * uh_ironsight_zoom`, not a subtraction.
- **`uh_ironsight_zoom_focus` (40) is the aim-walk speed, not a FOV value**:
  despite its help text ("subtracted from the default FOV" — stale), the code
  writes it to `m_flMaxspeed` (`sub_100EA7B0` → player offset 4132) and
  restores `hl2_walkspeed` on leave.
- **`m_bExpSighted` is networked on the viewmodel**, not a client-local flag;
  the client reads it directly (single source of truth, no client
  `ironsight_toggle` CON_COMMAND — the string only appears in the resync path
  `sub_100D8E90`). This also keeps the `CBaseViewModel` layout identical in
  both DLLs (the earlier `#if CLIENT_DLL`-guarded members shifted every
  `CNetworkVar` below them and corrupted memory → the crash).

The FOV is set directly (`m_iFOV`/`m_flFOVTime`/`m_iFOVStart`/`m_flFOVRate`),
not via `CBasePlayer::SetFOV`. The original's custom setter `sub_100F8040`
also claims `m_hZoomOwner` (passing the player itself); going through it makes
`IsZooming()` true while only ironsighted and collides with the vanilla suit
zoom. A direct set gives the same `GetFOV()` result without touching
`m_hZoomOwner` — intentional, documented divergence.

`m_bIronSighted` / `m_fIronsightedTime` are networked (accuracy + FOV follow
the authoritative server state).

### Weapon fire tuning (stage 28)

`CUHGunWeapon` now reads the parsed weapon-script stats in the fire path:

- **Fire rate** (`GetFireRate`): the Underhell scripts carry no cycle-time key,
  so each class gets a value recovered from serveror.dll (see stage 30).
- **Recoil** (`AddViewKick`): `PunchPitch`/`PunchYaw` ranges, scaled by
  `CrouchRecoilMult` while ducked.
- **Spread** (`GetBulletSpread`): a base cone lerped by an accumulated spam
  penalty, scaled by `CrouchAccuracyMult` (duck), `RunAccuracyMult` (moving)
  and the `ExpOffset` accuracy while ironsighted (`m_bIronSighted`).
- **Ironsight desync**: switching weapons while sighted now un-sights
  (`Weapon_Switch` calls `UH_DisableIronsight`).

### Weapon damage + fire rate (stage 30)

The weapon scripts carry no damage / fire-rate keys — those are baked into the
C++ classes. Recovered from the original:

- **Damage** lives in `cfg/skill.cfg` as `sk_plr_dmg_<weapon>` (the original
  registers each as a `ConVar` default "0" and the engine's skill.cfg sets the
  real value; it also reads them through a per-weapon `GetDamage()` vtable
  override — hexrays slot 213). Values (baked into the constructors here):

  | Weapon | Damage | | Weapon | Damage |
  |---|---|---|---|---|
  | axe / baton / pipe / wrench / cleaver | 35 / 13 / 15 / 25 / 50 | | glock / beretta / socom / python / dualies | 10 / 15 / 20 / 120 / 20 |
  | mp5 / mp5_eod / mp7 | 12 / 10 / 8 | | m3 / m5 / spas12 / xm1014 | 12 / 16 / 14 / 12 |
  | g36k / sniper | 20 / 80 | | bfg_mgl / bfg_minigun | 200 / 50 |

- **Fire rate** (extracted from serveror.dll vtable + disassembly):

  | Weapon | Rate | Source |
  |---|---|---|
  | pistols (glock/beretta/socom/python/dualies) | 0.2 s | shared fire routine `sub_1027AEC0` |
  | SMG mp5 / mp5_eod / mp7 | 0.075 s | `GetFireRate` vtable slot 277 → `sub_102801F0` (`flds 0x105300E4`) |
  | BFG minigun | 0.075 s | same `GetFireRate` (`sub_102801F0`) |
  | G36K | 0.1 s | select-fire `GetFireRate` `sub_103F5150` (`flds 0x1048775C`) |
  | shotgun m3/m5/spas12/xm1014, sniper, BFG mgl | ~0.75 / ~1.5 / ~1.0 s | custom pump/delay fire paths — TODO exact |

- **Note**: the vanilla base `CBaseCombatWeapon::PrimaryAttack` never sets
  `info.m_iDamage`, so the previous port fired with the ammo-definition damage
  instead of the Underhell value. `CUHGunWeapon::PrimaryAttack` now builds the
  shot itself with `m_iDamage = GetDamage()`.

### Silencer + laser sight (stage 31)

Decoded from `sub_101E2F50` (toggle) + `sub_101F11D0` (ClientCommand dispatch)
+ the player sendtable (`sub_101F2D30`).

- **Silencer**: `m_bSilenced` on the gun, `m_bHavePistolSilencer` /
  `m_bHaveRifleSilencer` (networked) on the player. `silencer_toggle` gates
  pistols (`GetWeaponType() == 1`) and rifles (`== 4`) on owning the matching
  silencer; other weapons toggle freely. `SetPistolSilencer` /
  `SetRifleSilencer` inputs grant the silencer. The original's SoundData adds
  `single_shot_silenced` at index 2 (`SINGLE_SILENCED`), used when silenced.
- **Laser sight**: `m_bLaserToggleState` (networked) + `UH_ToggleLaser` +
  `laser_toggle` command. Client `uh_laser.cpp` draws a per-frame beam + impact
  dot from the eye along aim (`sprites/laserbeam.vmt` + `sprites/laserdot.vmt`).
- **DropWeapon**: `UH_DropWeapon` (command) — un-sight, refuse in vehicle /
  sprinting / melee, toss the active weapon forward at 300; `m_bDisableWeaponDrop`
  + `DisableDropWeapon` / `EnableDropWeapon` inputs.
- **Throw_Nade**: `UH_ThrowNade` (command) — un-sight then start the
  `weapon_frag` throw (arm-deploy flare branch TODO).

### TODO (ironsight / weapons)

- Free-aim (first-person: crosshair decouples from the view, weapon viewmodel
  tilts toward the mouse) still TODO — see the dedicated section below. It is
  NOT the same as the third-person over-the-shoulder (OTS) camera.
- Shotgun multi-pellet fire (skill.cfg `sk_plr_num_shotgun_pellets = 12`):
  the port fires a single shot per trigger pull; the original fires 12 pellets.
- Exact fire rate for SMG / shotgun / sniper / BFG (recover from each weapon's
  fire function in serveror.dll).
- Silenced viewmodel activity (`ACT_VM_*_SILENCED`) + ATTACH/DETACH_SILENCER
  animations — the port only switches the sound.
- Client laser beam/dot rendering.

## Remaining original parameters (stage 32 — inventory of what's left)

Full list of original `CHL2_Player` state + commands not yet ported (from the
sendtable `sub_101F2D30` / ClientCommand `sub_101F11D0`):

| Feature | State | Command / input | Status |
|---|---|---|---|
| Night vision | `m_bHaveNightVision` @2138, `m_bNightVisionOn` @3369, `m_bNightVisionEnabled` @2140 | `NightVision_Toggle`, `SetNightVision` | done (battery drain TODO) |
| Gas mask | `m_bHaveGasMask` @2139, `m_bGasMaskOn` @3370, `m_bGasMaskEnabled` @2141 | `GasMask_Toggle`, `SetGasMask` | done |
| Shoulder flashlight | `m_bShoulderFlashlight` @5040 | — | networked, viewmodel TODO |
| Flashlight viewmodel | `m_bFlashlightHolstered` | `v_flashlight_pg.mdl` | TODO |
| Fake flare | `m_bHoldingFlare` @2122, `m_bFlareHitting` @2124, `m_bFlareMarker` @2125 | `v_flare_pg.mdl` | TODO |
| Kick | `m_bKickMarker` @4185 | `uh_jake_kick`, `v_kick_jake_*.mdl` | TODO |
| Weapon drop | `m_bDisableWeaponDrop` @2136 | `DropWeapon`, `InputDisableDropWeapon` / `InputEnableDropWeapon` | TODO |
| Grenade throw | — | `Throw_Nade` | TODO |
| Fire-mode select | `m_iFireMode` (script `FireMode`) | `firemode_toggle` | TODO (G36K select-fire) |
| Left arm | `m_bLeftArmDeployed` @2121 | — | TODO |
| Hermit cards | `m_iUHHermitCardsCount` @5048, quest counters | — | partial (`uh_give_hermit_card` cheat) |

Also still TODO from earlier stages: gas-mask check in food `Use()`, melee
swing stamina gate, full bleeding→damage scaling, exact held-item handle @2164.

## Message entities + mirror reflection (env_message/env_hudhint/env_global + player-in-mirror)

Decoded from the "New Message Entities Inputs" and "Player Model and Mirror
Reflections" tutorials + the original datamaps (CMessage sub_101387F0,
CEnvHudHint sub_10137AD0) and input handlers (sub_101EEE40 = ViewModelSkin).

- **env_message** (`CMessage`): `InputMessage` shows the parameter directly
  (does NOT store). `SetMessage` is the highest priority (17); `SetMessagePriorityN`
  (1..16) store into a slot, higher N wins; `RemoveMessagePriority` takes the
  priority number (1..16) and falls back to the next-highest; `ShowMessage`
  displays the active one. `GlobalEnvMessageIndex` (0-7) persists the active
  priority between maps via the global-state counter `uh_envmessage_<idx>`
  (TODO: original stores the message "as player state" — verify string survives).
- **env_hudhint** (`CEnvHudHint`): `InputHint` / `InputHintThroughParameter`
  (FIELD_STRING) show the hint from the parameter.
- **env_global** (`CEnvGlobal`): `SetGlobalOn` / `SetGlobalOff` / `SetGlobalDead`
  affect the global NAMED in the parameter (not the entity's own keyvalue), so
  one env_global can drive many shared globals.

### Player in mirrors (mirror/monitor-only rendering)

- `CBaseEntity::m_bIsMirrorOnly` (networked bool, keyvalue `uh_rendermirrorsonly`).
  Server sendtable + client recvtable (after `m_bAlternateSorting`).
- Client convar `cl_player_render_mirror` (FCVAR_CHEAT, default 1).
- Client render hook: `g_bRenderingReflectiveGlass` is set around
  `CReflectiveGlassView::Draw()` / `CRefractiveGlassView::Draw()`;
  `C_BaseEntity::ShouldDraw()` rejects `m_bIsMirrorOnly` entities outside that
  pass; `C_BasePlayer::ShouldDraw()` lets a mirror-only local player draw there
  even in first person.
- Player inputs `SetPlayerModel` (sets model + flags mirror-only),
  `SetPlayerSkin`, `ViewModelSkin` (viewmodel hand/glove skin), and
  `SetPlayerKickModel` (kick viewmodel, TODO full kick) — see
  `game/server/underhell/uh_player_model.cpp`.

## Weapon pickup: one weapon per class/slot (stage follow-up)

Decoded from the original pickup flow (`GiveNamedItem` sub_101F0AA0 + `Use` ->
`BumpWeapon`) and confirmed against the weapon-script buckets:

- **`CBasePlayer::GiveNamedItem`** now calls `Use(this, this, USE_TOGGLE, 0)`
  instead of `Touch(this)`. Underhell weapons clear their touch function
  (`SetPickupTouch` -> `SetTouch(NULL)`), so the vanilla `Touch` no-op'd and
  impulse 101 spawned weapons on the floor without equipping them. `Use` ->
  `BumpWeapon` is the same path as pressing +use.
- **`CHL2_Player::BumpWeapon`** enforces one weapon per bucket/slot: picking up
  a weapon of a class the player already carries drops (or, during the
  impulse-101 cheat, silently removes) the current weapon(s) in that slot before
  equipping the new one. This makes impulse 101 leave exactly one weapon per
  slot (the last given) and makes +use replace the held weapon, matching the
  original "weapon of a class can only be replaced" behaviour.

### Mirror fix (follow-up)

The first mirror attempt gated `C_BaseEntity::ShouldDraw()` on the reflective
pass, which removed mirror-only entities from the leaf system (ShouldDraw is
only re-evaluated on visibility updates, not per draw). Corrected:

- `ShouldDraw()` returns true for mirror-only entities (always visible).
- `C_BasePlayer::ShouldDraw()` returns true for a mirror-only local player even
  in first person.
- The draw is gated in `C_BaseEntity::DrawModel` / `C_BaseAnimating::DrawModel`
  (skip unless `g_bRenderingReflectiveGlass`), so mirror-only entities stay in
  the leaf system but only actually render during the reflective/refractive pass.

## Bullet time (bt_* commands) — decoded (TODO: port)

Reverse-engineered from `serveror.dll` (ConVar registrations sub_10453C20-10453CE0,
change-callback sub_100EA800, bullet entity sub_10107970/sub_101078D0, EmitSound
pitch hook sub_1023B9A0/sub_1023BC60, impulse-110 sub_101EC700, death hook
sub_102DDB80).

### ConVars / command

| Name | Kind | Default | Flags | Purpose |
|---|---|---|---|---|
| `bt_enabled` | ConVar (change callback) | "0" | FCVAR_CHEAT\|FCVAR_REPLICATED | master toggle (callback applies timescale/overlay/speed) |
| `bt_timescale` | ConVar | "0.3" | 0 | slow-mo factor (host_timescale + sound pitch) |
| `bt_enemybulletspeed` | ConVar | "500" | 0 | tracer speed for enemy-fired bullets |
| `bt_playerbulletspeed` | ConVar | "2000" | 0 | tracer speed for player-fired bullets |
| `bt_plr_speed` | ConVar | "250" | 0 | player maxspeed while active |

`bt_enabled` is a ConVar **with a change callback** (registered via the 6-arg
ConVar ctor, unlike the plain 4-arg ConVars). The callback (sub_100EA800):

- finds `host_timescale` + `hl2_normspeed` convars,
- for every player:
  - OFF: `r_screenoverlay off`, `host_timescale = 1.0`, restore maxspeed
    (hl2_normspeed, 150).
  - ON: `r_screenoverlay dev/bullettime`, `host_timescale = bt_timescale`,
    `m_flMaxspeed (offset 4132) = bt_plr_speed`.

### Bullet entity (CBullet — not present in vanilla Orange Box SDK)

`sub_10107970` = Spawn: picks tracer model by ammo type — `bt_9mm.mdl` (9mm/
AR2), `bt_357.mdl` (357), `w_pellet.mdl` (buckshot, `m_nBulletType ^= 1`), else
`bt_762.mdl` (rifle default). Bullet speed = `bt_enemybulletspeed` or
`bt_playerbulletspeed` depending on a shooter flag (offset 1212 = enemy-fire).

`sub_101078D0` = Think: while bullet time active velocity = dir * (speed *
timescale); otherwise dir * 2500. (So bullets visibly crawl during bullet time
and zip at 2500 normally.)

### Sound pitch

`EmitSound` (sub_1023B9A0 / raw wave sub_1023BC60) scales the sound pitch by
`bt_timescale` while active (≈ 65% pitch at 0.3) — the classic slow-mo audio.

### Ammo / death / impulse

- Ammo drain (sub_100CF490 / sub_100CF500) is **skipped while bullet time is
  active** — infinite ammo during slow-mo.
- Player death (sub_102DDB80) sets `bt_enabled = 0`.
- `impulse 110` toggles it via the CheatImpulseCommands path (plays
  `Player.bullettimestart` / loops `Player.bullettimeloop` / `Player.bullettimeend`,
  and schedules a `BulletTimeEndContext` think that stops the loop).

### Port plan (TODO)

1. Register the 5 `bt_*` ConVars (bt_enabled with a change callback).
2. Port the `CBullet` entity (models are already in the mod assets; precache via
   sub_10107790: `w_bullet`, `bt_9mm`, `bt_357`, `bt_762`, `w_pellet`).
3. Hook `FireBullets` to spawn `CBullet` (decode sub_100EAFB0 — the tracer/
   penetration loop that spawns the bullet entity per ammo type).
4. Pitch-scale in `CBaseEntity::EmitSound` when `bt_enabled`.
5. Gate ammo drain on `bt_enabled`.
6. Death hook + impulse 110 (already have the CheatImpulseCommands override).

The heaviest unknown is #3 (the exact FireBullets -> CBullet wiring) since
sub_100EAFB0 is a large inlined tracer loop.

## Self-driving jeep + gunner seat (stage follow-up)

`prop_vehicle_jeep` in Uh_Chapter1_16_d is self-driven: the map fires
`Vehicle_Jeep,Throttle,1` / `HandBrakeOff` etc., Bryan enters as the NPC driver,
and the player sits at the mounted gun. Decoded from the original
`CPropVehicleDriveable` (datamap sub_102692E0, sendtable sub_10266BA0):

- `m_bPlayerAtGun` (networked bool) — true while the player mans the gun.
- `ToggleGunMode` input (sub_103EC3C0) toggles it; `EnableMountedGun` keyvalue
  (`m_bEnableMountedGun`).
- `DriveVehicle` (sub_103ED6A0) only calls the driving input
  (`CFourWheelVehiclePhysics::UpdateDriverControls`) when `!m_bPlayerAtGun`.
  Otherwise the player's (absent) driving input would stomp the scripted
  throttle every frame and the jeep would never move / get stuck in geometry.

Ported: `m_bPlayerAtGun` (server + client send/recv), `ToggleGunMode` input,
`EnableMountedGun` keyvalue, and the `DriveVehicle` gate. TODO: gunner eye
attachment (`vehicle_gunner_eyes` vs `vehicle_driver_eyes`, sub_103EA8F0) for
the correct gun camera, and the mounted-gun tracer (AR2Tracer vs tau cannon).

## Jeep gunner seat — eye + remaining pieces (stage follow-up 2)

The `prop_vehicle_jeep` in Uh_Chapter1_16_d is self-driven (Throttle/HandBrake
inputs + Bryan the NPC driver), while the player mans a mounted gun on top.

- `SharedVehicleViewSmoothing` now picks `vehicle_gunner_eyes` instead of
  `vehicle_driver_eyes` when `m_bPlayerAtGun` (decode sub_103EA8F0). This is
  the camera "on top" fix.
- TODO: `m_bEnableMountedGun` swaps the driver's tau cannon for an AR2-tracer
  mounted gun in `FireCannon` (decode sub_103EE570 / sub_103EAB30 gate
  `(!m_bEnableMountedGun || m_bPlayerAtGun)`).
- TODO: `EnterVehicleImmediatelyAsDriver` / `EnterVehicleAsDriver` are
  Underhell-added NPC inputs (Bryan, an npc_citizen, enters the jeep as the
  visual driver). Not yet ported — Bryan won't appear seated until they are.
- `r_JeepViewZHeight 10` (map-fired client command) already exists and raises
  the jeep view for the gunner.

## NPC weapon acttables (stage follow-up — fixes allies-can't-shoot + T-pose)

The NPC acttable maps the NPC's activities (ACT_RANGE_ATTACK1, ACT_IDLE,
ACT_RELOAD, ...) to the weapon-specific model activities. Our port had only
`{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_AR2, true }` for every gun, so:

- Citizens / cops (npc_citizen with nypdcop models) holding pistols had no
  fire animation (they need ACT_RANGE_ATTACK_PISTOL, not AR2) -> T-pose + no
  shooting.
- SMGs mapped to AR2 too (combine_soldier has AR2, but citizens don't).

Decoded the original acttables from the binary (pistol->ACT_RANGE_ATTACK_PISTOL,
smg->ACT_RANGE_ATTACK_SMG1, shotgun->ACT_RANGE_ATTACK_SHOTGUN, rifle->AR2) and
ported the FULL per-category acttables (idle/idle_angry/reload/walk_aim/run_aim/
crouch/gesture + low variants) matching vanilla weapon_pistol/smg1/shotgun/ar2.
BFG minigun (anim_prefix smg2) uses the SMG1 acttable.

This is the fix for "special forces allies can't shoot" and the NPC T-pose
("Bad sequence (-1 ...) in GetSequenceLinearMotion"): the NPC requested an
activity its model lacked, SelectWeightedSequence returned -1, and the model
froze in the reference pose.

## Kick attack (uh_jake_kick) — ported

Decoded from CHL2_Player::ClientCommand sub_101F11D0 + the kick think/impact
handlers sub_101F2990 / sub_101F0050 / sub_101E5A60. Fully ported:

- `MAX_VIEWMODELS` 2 -> 3: viewmodel index 2 is the kick viewmodel
  (`models/weapons/v_kick_jake_*.mdl`, set by `SetPlayerKickModel`).
- ConVars: `uh_kick_damage` (21), `uh_kick_forcemult` (2), `uh_kick_enabled`
  (1, FCVAR_CHEAT).
- Command `uh_jake_kick`: gates (alive / not sprinting / not in vehicle /
  not already kicking / `DisableKick` / `uh_kick_enabled` / suit power >= 20),
  drains 20 suit power, raises the kick viewmodel, viewpunch (-2,0,0), rumble
  (4,0,4), swing + exertion voice, schedules the strike +0.35s later.
- `UH_KickThink`: first pass does a 72-unit forward trace (mask 0x600400B),
  applies DMG_CLUB damage + force, plays kick_body/kick_wall, fires the
  `OnKicked` output (added to CBaseEntity) on the victim, then holsters +0.4s.
- Inputs `DisableKick`/`EnableKick` + output `OnDisabledKickAttempted` (player);
  `OnKicked` output on CBaseEntity (map: "OnKicked" "door_KillingRoom,EnableMotion").

TODO (NPC-kick direction): `m_flViewkick` / `m_hLastNPCToKickMe` — the reverse
case where an NPC kicks the player (sk_combine_s_kick etc.), not yet ported.

## Weapon pickup + shotgun fire fixes (decoded from DefaultTouch/BumpWeapon)

Weapons are **+use only** — no auto-equip on touch. Picking up goes through
`CBaseCombatWeapon::Use -> CHL2_Player::BumpWeapon` (+use, and GiveNamedItem's
`Use` call for impulse 101). Walking over a weapon instead **scavenges its
loaded clip ammo** into the player's reserve and leaves the weapon on the
ground (`SetPickupTouch` -> `AmmoScavengeTouch`; grenades `weapon_frag` are
consumed as a grenade pickup). Ammo boxes keep the vanilla walk-over pickup.

- `CHL2_Player::BumpWeapon` (the +use path):
  - `weapon_frag` is an ammo pickup (give 1 grenade, max 4, remove the frag) and
    never replaces another weapon (fixes the BFG being dropped when grabbing a
    frag in the same bucket 5).
  - one weapon per bucket: throws the current same-bucket weapon forward
    (Weapon_Drop with forward * 300, decode sub_100D02C0) before equipping the
    new one; impulse 101 silently strips it instead.
- Shotguns fire `sk_plr_num_shotgun_pellets` (7) pellets with a 10-degree cone
  (m_iShotsPerFire + wide spread), instead of a single bullet; NPC fire uses
  the same pellet count.

TODO: sk_plr_num_shotgun_pellets is hardcoded to 7 (its default) rather than
read from the convar; shotgun pump animation delay (m_bNeedPump / SequenceDuration)
is approximated by the 0.8 s fire rate.

NOTE: the original DLL's `DefaultTouch` (sub_100D02C0) does auto-pick on touch,
which conflicts with the observed in-game behaviour (weapons need +use). Kept
the working +use-only model and left this note rather than matching the
decompile byte-for-byte.

## Select fire (firemode_toggle) — ported

Decoded from CHL2_Player::ClientCommand sub_101F11D0 -> the weapon method at
vtable+840 (sub_102B0D10) -> CHLSelectFireMachineGun::PrimaryAttack sub_102B18E0
+ the trigger-latch re-arm in sub_10279A80.

- The Underhell gun base derives from `CHLSelectFireMachineGun`, so `m_iFireMode`
  lives on the weapon at offset 1404 (vanilla enum: FIREMODE_FULLAUTO=1,
  FIREMODE_SEMI=2, FIREMODE_3RNDBURST=3). Default is full auto (1).
- `firemode_toggle` (config.cfg binds it to "x") flips 1 <-> 2 and plays the
  hard-coded `Weapon_Pistol.Empty` click. The original is asymmetric: full->semi
  is silent, semi->full plays the click.
- Semi mode is trigger-gated: a latch (`m_bFireOnEdge`, the CHLMachineGun byte at
  offset 1388) is armed in `WeaponIdle()` when the attack button is released and
  consumed by `PrimaryAttack()`, so a held trigger fires exactly one shot in
  semi mode. Full auto keeps firing while held.

Ported into CUHGunWeapon: m_iFireMode + m_bFireOnEdge + UH_ToggleFireMode() +
WeaponIdle() override + a semi gate in PrimaryAttack.

Divergence: the original restricts select-fire to the CHLSelectFireMachineGun
subtree (G36K, the SMGs, BFG minigun — pistols/shotguns/sniper/BFG-MGL derive
from CHLMachineGun and have no fire mode). Here every CUHGunWeapon gets the
toggle for simplicity; behaviour is identical for the G36K (the only script
that declares FireMode).

TODO: the original maps fire mode to distinct viewmodel activities (semi vs
full-auto vs shotgun, plus silenced variants — sub_10279580 returns 207/208/209
etc.). Those enum entries don't exist in the vanilla activity list, so the
animation mapping is left at ACT_VM_PRIMARYATTACK here.


## Item pickups — implemented (ammo / gear / armour)

Decoded the remaining item MyTouch handlers and implemented them (server-side,
+use pickup, auto-apply + consume, matching the original amounts/sounds):

- **Ammo boxes** (sub_10171670 / 101716F0 / 10171780 / 10171820 / 101718B0),
  skill-scaled via GetAmmoQuantityScale:
  - item_box_pistol_ammo -> 30 Pistol (`HL2Player.PickupPistolAmmoBox`)
  - item_box_357_ammo   -> 20 357    (`HL2Player.Pickup357AmmoBox`)
  - item_box_smg1_ammo  -> 50 SMG1   (`HL2Player.PickupSMGAmmoBox`)
  - item_box_rifle_ammo -> 50 AR2    (`HL2Player.PickupRifleAmmoBox`)
  - item_ammo_buckshot  -> 6 Buckshot (`HL2Player.PickupBuckShotAmmo`)
- **item_heavyarmor** -> 45 armour, max 200 (sub_10174730).
- **Gear ownership** (item_flashlight / item_nightvision / item_gasmask /
  item_shoulderflashlight) sets the player flags (m_bFlashlightOn /
  m_bHaveNightVision / m_bHaveGasMask / m_bShoulderFlashlight). NOTE: night
  vision / gas mask USAGE (the toggle commands + client overlay) is still TODO —
  the pickups just grant ownership.
- **item_shield / item_cap_pmc / item_headset_pmc** -> grant 10 armour like the
  helmets (models riotshield / pmc_cap / pmc_headset). The shield's real block
  mechanic is still TODO.

Still TODO (unknown class names / no RTTI): item_bandagespack, item_syringepack,
item_flags, item_health.

## Night vision + gas mask — implemented

Decoded the two gear toggles and ported them (server + client):

- **NightVision_Toggle** (client command -> CHL2_Player vtable 404, sub_102E19B0):
  mutual exclusion with the gas mask, ownership gate (`m_bHaveNightVision` &&
  `m_bNightVisionEnabled`), battery gate (deny when `m_iUHBatteryCount <= 0` &&
  `m_flUHBatteryCharge <= 10`), toggles the networked `m_bNightVisionOn`, flips
  the "NightVision" playermodel bodygroup, plays `Player.nvon`/`Player.nvoff`.
- **GasMask_Toggle** (client command -> sub_101ED380): mutual exclusion with
  night vision, ownership gate (`m_bHaveGasMask` && `m_bGasMaskEnabled`),
  toggles the networked `m_bGasMaskOn`, starts/stops the looping
  `GasMask.Breath.Normal` (CSoundPatch), flips the "GasMask" bodygroup, plays
  `Player.GasMaskOn`/`Player.GasMaskOff`.
- **SetNightVision / SetGasMask** inputs (FIELD_BOOLEAN) grant/take the gear
  (sub_101E36C0 / sub_101E3750); taking them while active forces the overlay off.
- Gear resets on Spawn + Event_Killed (breath loop stopped, "on" flags cleared).
- Client: `m_bNightVisionOn`/`m_bGasMaskOn` recv'd on C_BaseHLPlayer; a new
  `IScreenSpaceEffect` (`uh_gear_overlay.cpp`) draws `shader/nightvision` and
  `shader/gasmask` full-screen (the materials + shaders are in the game install).

TODO (decode leftovers): the original also clears `r_flashlightscissor` on the
night-vision toggle, sets a custom effects flag 0x400 (outside SDK EF_MAX_BITS),
and drains the flashlight battery while night vision is on (auto-off in
sub_102E3DE0) — the turn-on battery gate is ported, the continuous drain is not.

## Second hand / left arm (decoded, TODO: implement)

The "second hand" is **viewmodel index 1** (index 0 = active weapon, index 2 =
kick). It holds the flashlight, a flare, or a grenade. State (server offsets /
networked where noted):

| Field | Offset | Net | Meaning |
|---|---|---|---|
| `m_bLeftArmDeployed` | 2121 | yes (client 3451) | left arm raised / holding something |
| `m_bHoldingFlare` | 2122 | yes (client 3452) | holding a flare (throws flare instead of grenade) |
| `m_bFlareMarker` | 2123 | no | flare throw anim in progress |
| `m_bFlareHitting` | 2124 | no | flare hitting (don't re-throw) |
| `m_bFlashlightHolstered` | 2172 | no (saved) | flashlight holstered in the left hand |
| `m_flFlareTime` | 2128 | no | flare fuse time |
| `m_bShoulderFlashlight` | 5040 | yes | shoulder-mounted flashlight (vs hand-held) |

Decode map:

- `sub_101E7EA0(p, 1)` == `GetViewModel(1)` (reads m_hViewModel @4024 + 4*i).
- **Throw_Nade** (`sub_101ED130`): if holding a flare -> `sub_101E97E0` (flare
  throw chain); else if grenades: un-sight, get viewmodel 1, set its model to
  `models/weapons/v_grenade.mdl`, skin 1, set `m_bLeftArmDeployed=1` +
  `m_bFlareMarker=1`, play a 173 throw anim on the active weapon, then
  `SetContextThink(curtime+0.4, "FlashLightContext")` (the actual throw is
  staged 0.4 s later).
- **FlashLightContext think** (`sub_101EE050`): the multi-stage throw chain.
  If holding a flare it spawns a `prop_physics` `models/PG_props/pg_obj/pg_flare.mdl`
  with a 90 s fuse and 200u velocity (`sub_101E9580`), sets viewmodel 1 skin 2,
  clears `m_bHoldingFlare`, then re-schedules; finally it drops the active
  weapon and re-equips (`sub_100CE740` + vtable 1248).
- **Left-arm flashlight deploy** (`sub_101F0C60`): un-sight, throw any held
  flare, toggle `m_bFlashlightHolstered`; when deploying sets viewmodel 1 model
  = `models/weapons/v_flashlight_pg.mdl`, skin 1, activity 32, `m_bLeftArmDeployed=1`.
  Registered in the datamap as "FlashlightViewModelThink".
- **Flare throw** (`sub_101E9580`): spawns the flare prop_physics, sets a
  curtime+90 fuse, applies 200,200,200 velocity in the throw direction, removes
  the held flare viewmodel.
- **Arm think** (`sub_101EB870`): the flare-hit trace (mask 1174421507) that
  strikes a surface with the flare and marks `m_bFlareMarker`.

The whole thing is driven by the weapon-script `OneHanded` flag (pistols/melee):
while a one-handed weapon is active the left hand holds the flashlight (or a
flare). `OneHanded` lives at weapon-info offset 80.

Implementation notes for later:
- MAX_VIEWMODELS is already 3 (index 1 = left arm, index 2 = kick), so the
  viewmodel slot exists; `SetPlayerKickModel` already manages index 2.
- Needs: a "FlashLightContext"-style staged think, `m_bLeftArmDeployed` /
  `m_bHoldingFlare` networked bools, the flare prop_physics spawn/throw, and
  hooking weapon deploy so one-handed weapons raise the left arm. The flare
  (`v_flare_pg.mdl`) + flashlight (`v_flashlight_pg.mdl`) viewmodels are
  precached in the original sub_101E25F0.

## Second hand / left arm — implemented (flashlight / flare / grenade)

The left arm is viewmodel index 1 (index 0 = weapon, 2 = kick). State added to
CHL2_Player (m_bLeftArmDeployed @2121 + m_bHoldingFlare @2122 networked; the
rest server-local). New file `game/server/underhell/uh_leftarm.cpp`:

- **Animated grenade throw** (`Throw_Nade`, sub_101ED130): the grenade viewmodel
  (`v_grenade.mdl`) goes into the left hand, the arm raises, `ACT_VM_THROW`
  plays on the active weapon, and the grenade actually leaves 0.4 s later from
  `UH_LeftArmContextThink` (the original "FlashLightContext" think). The old
  immediate `WeaponFrag_ThrowNow` call was replaced.
- **Flare** (`sub_101E9580`): using a flare pack (`UH_ITEM_FLARE_PACK`) now
  equips a flare in the left hand (`v_flare_pg.mdl`) instead of re-picking the
  item; `Throw_Nade` then throws a lit `prop_physics` flare
  (`pg_flare.mdl`, 200 u/s, 90 s fuse) instead of a grenade.
- **Flashlight in the left hand** (`sub_101F0C60`): while the flashlight is on
  and a one-handed (pistol) or melee weapon is active, the left arm raises
  `v_flashlight_pg.mdl`; otherwise it is hidden. Re-evaluated on flashlight
  on/off, weapon switch and weapon drop (`UH_UpdateLeftArm`).

NOTE: the flashlight's LIGHT is still the vanilla EF_DIMLIGHT; the left-arm
`v_flashlight_pg.mdl` is the visual hold. The full viewmodel flashlight entity
(shoulder mount, holster animation, FlashlightViewModelThink) is still TODO.

## Night vision "solid gradient" — shader DLLs, not game code

The night vision / gas mask overlays use custom shaders shipped in the mod:

- **`game_shader_generic_eshader_2007.dll`** — the game-shader DLL that
  implements the `shader/nightvision` / `shader/gasmask` / `shader/filmgrain`
  shaders. Loaded by the material system (engine), NOT by client.dll — do NOT
  reverse it; it just needs to be present in `<mod>/bin/` and load correctly.
- **`shadereditor_2007.dll`** — the runtime shader editor (dev tool). client.dll
  loads it at startup via `CreateInterface("ShaderEditor005")` (sub_1011F6E0)
  and only activates it with `-shaderedit`. Irrelevant to gameplay; do NOT
  reverse it.

The client already draws `shader/nightvision` / `shader/gasmask` full-screen
(matching sub_10141600's DrawScreenSpaceRectangle). A "solid gradient" means the
shader from the game-shader DLL isn't being applied — i.e. the DLL isn't
loading in the install (check `Underhell/bin/game_shader_generic_eshader_2007.dll`
exists and gameinfo.txt points at the mod), not a client.dll/server.dll bug.

## Radio / flashlight / grenade throw fixes (second-hand system)

Three bugs fixed in the second-hand (left arm, viewmodel index 1) + inventory
paths, each cross-checked against the decompile (`klaxons1/underhell-hexrays`,
`Underhell/bin/server/sub_*.cpp`).

### 1. FM radio / radio cracker "useitem" never threw (dead code)

`UH_ItemAction` (server `uh_player_inventory.cpp`) had the FM-radio / radio-
cracker branch **nested inside** the flare-pack branch, after its `return true;`:

```cpp
if ( bUse && iItem == UH_ITEM_FLARE_PACK )
{
    ...
    return true;

    // unreachable: this `if` was inside the flare block, after `return true`
    if ( bUse && ( iItem == UH_ITEM_FM_RADIO || ... ) )
    {
        ...
    }
}
```

The radio `if` was dead code — MSVC compiles it (C4702 unreachable-code
warning) but it never runs, so "useitem" on a radio did nothing. "dropitem"
worked because it goes through the separate `if (!bUse)` drop path (no radio
attract logic — expected).

Fix: close the flare-pack block after `return true;` so the radio branch is a
sibling. The radio branch spawns `uh_radio` (class `CUHRadio`), sets
`SetIsCracker()` per id, `Spawn()`, then `Use(USE_ON)` to activate (think +5 s,
then `CSoundEnt::InsertSound( SOUND_FMRADIO, … 1024, 1.0 )` every second).

Decode: `UH_ItemAction` = vtable [411] = `sub_102E05F0`; radio activation =
`sub_10173790` (Use -> think +5 s) / `sub_101737E0` (SOUND_FMRADIO insert).

### 2. Second hand didn't raise the flashlight (no-weapon case)

`UH_UpdateLeftArm` gated the hand-held flashlight viewmodel on
`bOneHanded = pWeapon && (OneHanded || MeleeWeapon)` — **false when the player
has no active weapon**. `FlashlightTurnOn` already allows the light with
`!pWeapon || OneHanded || MeleeWeapon`, so with no weapon the flashlight turned
ON (EF_DIMLIGHT) but the left hand never showed `v_flashlight_pg.mdl`.

Fix: `UH_UpdateLeftArm` now uses the same `bLeftArmFree = !pWeapon || OneHanded
|| MeleeWeapon` gate, keeping the two functions in sync.

Decode: the whole left-arm flashlight hold is driven by the weapon-script
`OneHanded` flag at weapon-info offset 80 (pistols: `weapon_pistol_glock.txt` /
`weapon_pistol_beretta.txt`; melee: `weapon_melee_axe/baton/pipe/wrench.txt` +
`weapon_cleaver.txt` all ship `"OneHanded" "1"`). Deploy = `sub_101F0C60`
(sets viewmodel 1 = `v_flashlight_pg.mdl`, skin 1, `m_bLeftArmDeployed=1`).

### 2b. Flashlight with a two-handed weapon auto-switches to one-handed

The original does **not** deny the flashlight when a two-handed weapon is
active — it **switches to a one-handed weapon and raises the flashlight**.

Decode: `sub_101F0C60` (flashlight deploy) calls `sub_101E60C0` before toggling
the holster state. `sub_101E60C0` (with `sub_100CF460` = `this[525]` =
`m_hActiveWeapon`, `sub_100D0CC0` = `GetWpnData`, `sub_100D0E00` =
`GetWpnData()+1832` = `m_bMeleeWeapon`):

- active weapon exists and is not melee and `GetWpnData()+80` (`OneHanded`) == 0
  → scan all 48 weapon slots (`this+524` down to `this+477`, the `m_hMyWeapons`
  array);
- switch to the first OneHanded **non-melee** weapon (pistol) via
  `Weapon_Switch(weapon, 0)` (vtable +964); if none, remember the OneHanded
  **melee** weapon and switch to it as fallback;
- if there is no one-handed weapon at all, drop the current weapon (vtable
  +1236) and holster.

Port (`FlashlightTurnOn` + `UH_FindOneHandedWeapon`): with a two-handed weapon
active, switch to `UH_FindOneHandedWeapon()` (non-melee preferred, melee
fallback); only deny when no one-handed weapon exists. The original's
"drop the weapon" fallback is left out (deny instead) — documented divergence.

### 3. Grenades never threw (`weapon_frag` never owned)

`UH_ThrowNade` / `UH_LeftArmContextThink` looked up
`Weapon_OwnsThisType("weapon_frag")` and called `WeaponFrag_ThrowNow()`. But in
Underhell a grenade is **ammo, not a weapon**: `CHL2_Player::BumpWeapon`
converts a picked-up `weapon_frag` into `"grenade"` ammo (max 4) and removes the
entity. The player therefore never owns `weapon_frag`, the lookup returned
NULL, and every `Throw_Nade` returned early.

Fix (decode `sub_101ED130`): the original gates the throw on the grenade ammo
count — `sub_100CF610(a1, "grenade") > 0`, where `sub_100CF610` is
`GetAmmoCount(name)` (`this[v5 + 445]` = the player's ammo array). The port now:

- `UH_ThrowNade`: `GetAmmoDef()->Index("grenade")` + `GetAmmoCount() <= 0` gate,
  then stages the throw (grenade viewmodel in left hand, `m_bFlareMarker=1`,
  `ACT_VM_THROW`, think +0.4 s — unchanged).
- `UH_LeftArmContextThink`: spawns the frag directly via
  `Fraggrenade_Create(… , vForward*1200, 3.0 s fuse, owner=player)` and
  `RemoveAmmo(1, grenade)` — the vanilla `CWeaponFrag::ThrowGrenade` +
  `DecrementAmmo` path, run against the player's grenade ammo instead of a
  weapon_frag instance.

`WeaponFrag_ThrowNow` / `CWeaponFrag::ThrowNow` are now unused (left in place,
harmless) — the throw no longer needs a weapon_frag entity.

## VMF entity audit (maps vs. SDK implementation)

Audited the 5 compiled maps in `klaxons1/underhell-hexrays` → `Underhell/maps/`
(`Uh_House_1_d.vmf`, `uh_prologue_1_d.vmf`, `uh_prologue_2_d.vmf`,
`uh_chapter1_11_d.vmf`, `Uh_Chapter1_16_d.vmf`) — the SDK repo itself ships no
`.vmf`. Cross-checked every `"classname"` against our server DLL registrations.

### A. No missing entity classes

160 unique classnames appear across the maps. All are registered (vanilla HL2
+ Underhell custom). No map would fail with "unknown entity" at load.

Underhell-custom entities actually used by the maps, with status:

| Entity | Count | Status |
|---|---|---|
| `item_random` | 175 | implemented (pool; see §C) |
| `prop_dynamic_override` | 191 | implemented (+use / OnPlayerUse) |
| `prop_physics_override` | 171 | vanilla (props.cpp) |
| `env_message` | 17 | extended, implemented |
| `env_hudhint` | 9 | implemented |
| `env_global` | 6 | implemented |
| `item_flashlight` / `item_battery_pack` / `item_uhsoda` | 4 | implemented |
| `npc_infected` | 1 (point_template + npc_template_maker) | implemented — partial (climb/sprint/infection/gibs TODO) |
| `weapon_pistol_python` / `weapon_rpg` | 2 | implemented |
| `item_healthkit` / `item_healthvial` | 9 | vanilla modified (stash to inventory) |

`npc_infected` spawns via `npc_template_maker` (`TemplateName
Infected_Mainframe`), and the template sets body-variant keyvalues `Worker/
Uniform/Rural/Inmate` (capitalised in the VMF vs. lowercase `worker/uniform/
rural/inmate` in the FGD + our datamap — case-insensitive, so non-issue, but
worth an in-game glance at the spawned variants).

### B. `additionalequipment` on NPCs (Uh_Chapter1_16_d)

`npc_combine_s` / `npc_citizen` carry `additionalequipment` lists
(`weapon_smg_mp5, weapon_smg_mp5_eod, weapon_smg_mp7, weapon_rifle_g36k,
weapon_shotgun_m3/m5/spas12/xm1014`) — all 8 classnames are registered in our
SDK, so NPC weapon give works.

### C. `item_random` melee pool — 5 entries spawn nothing (faithful to original)

Our `s_ItemRandomPool` (and the original `sub_101757D0`) reference these
classnames for pool entries 46–50, which do NOT match the registered melee
classnames (`weapon_melee_wrench/pipe/axe`), or don't exist at all:

| Pool id | Spawned classname | Reality |
|---|---|---|
| 46 | `weapon_wrench` | registered as `weapon_melee_wrench` → NULL |
| 47 | `weapon_pipe` | registered as `weapon_melee_pipe` → NULL |
| 48 | `weapon_axe` | registered as `weapon_melee_axe` → NULL |
| 49 | `weapon_hammer` | not registered (no CWeaponHammer) |
| 50 | `weapon_shiv` | not registered (no CWeaponShiv) |

Confirmed in the decompile: `weapon_wrench`/`weapon_hammer`/`weapon_shiv`
appear ONLY in `sub_101757D0` (item_random Spawn), while `weapon_melee_*`
appear in the Precache registry `sub_101753E0` + the LINK_ENTITY_TO_CLASS
functions. The FGD itself warns: "Melee weapons may not work with item_randoms".
So these 5 entries silently spawn nothing in both the original and our port —
a 1:1 reproduction of an original quirk, not a regression.

### D. NOT-implemented player inputs fired by the maps (real gaps)

The maps fire these inputs at `!player`; our `CHL2_Player` datamap does not
register them. Every name below is confirmed present in the original
`serveror.dll` (binary string table), so they are real Underhell inputs, not
VMF typos. Usage counts across all 5 maps:

| Input | Uses | What it does (string-level) |
|---|---|---|
| `SetStatusVisibility` | 20 | show/hide the HUD status panels |
| `EnableInventory` / `DisableInventory` | 6 / 5 | toggle the inventory system |
| `EmptyInventory` | 4 | clear all inventory slots |
| `BleedPlayer` (fired `Bleedplayer`/`bleedplayer`) | 10 | force the bleed state |
| `RemoveLitGlowstick` (also `Removelitglowstick`) | 8 | remove the lit glowstick strapped to the player |
| `removeheldflare` | 3 | drop/remove the held flare (left hand) |
| `SetEndurance` | 2 | set the hunger/endurance meter |
| `AddEndurance` | 1 | add endurance |
| `SetBatteries` | 2 | set the flashlight battery count |
| `GiveShoulderFlashlight` / `RemoveShoulderFlashLight` | 2 / 1 | grant/remove the shoulder flashlight |
| `SetHudVisibility` | 3 | HUD visibility (related to SetStatusVisibility) |
| `DisplayHermitCards` | 1 | show the hermit-card deck HUD |
| `DisableBt` / `EnableBt` | 1 / 1 | bullet-time off/on (bt_* TODO) |

`SetStatusVisibility` / `SetHudVisibility` live in the CBasePlayer datamap
builder `sub_101F2D30`. The rest are datamap inputs whose semantics need the
same datamap + handler decode before porting (all still TODO).

### Conclusion

- **No missing entity classes** — nothing breaks at map load.
- The only "not implemented" entity behaviour with visible in-game impact is
  **§C** (5 `item_random` melee pool entries never roll a weapon) — which
  matches the original exactly.
- The substantive gaps are the **§D player inputs** — inventory/environment
  control the maps rely on (`EmptyInventory`, `DisableInventory`,
  `SetBatteries`, `BleedPlayer`, `SetStatusVisibility`, …), still TODO and the
  next sensible porting target.

## HUD audit (client panels vs. decompiled CHud* + HudLayout.res)

Cross-checked our 5 Underhell HUD panels against the decompiled client
(`Underhell/bin/client/sub_100BDF90` battery think / `sub_100BDC80` battery
paint / `sub_100CAC10` stamina / `sub_100C8710` endurance / `sub_100BE800`
bleeding / `sub_100BCFA0` + `sub_100BD080` hermit cards), the client recv table
`sub_10043D70`, and `scripts/HudLayout.res` + `scripts/HudAnimations.txt` +
`resource/ClientScheme.res`.

### Client field offsets (recv table `sub_10043D70`, confirmed)

| field | client offset |
|---|---|
| m_iEndurance | 3432 |
| m_iBleedCounter | 3436 |
| m_bNightVisionOn | **3449** |
| m_bGasMaskOn | 3450 |
| m_bLeftArmDeployed | 3451 |
| m_bHoldingFlare | 3452 |
| m_flSuitPower (in m_HL2Local) | 5168 |
| m_bFlashlightOn | 5286 |
| m_bDisplayHermitCard | 5287 |
| m_iUHBatteryCount | 5292 |
| m_iUHHermitCardsCount | 5296 |
| m_iUHHermitCurrentQuestCount | 5300 |
| m_iUHHermitTotalQuestCount | 5304 |
| battery charge float (inside m_HL2Local @+48) | 5212 |

### 1. Battery HUD — missing night-vision trigger (LOGIC BUG, fixed)

`sub_100BDF90` lights the gauge when:
`m_bFlashlightOn (5286) || m_bNightVisionOn (3449) || count changed (5292)`.

The first port only checked `m_bFlashlightOn || count changed`. Night vision
drains flashlight batteries (auto-off at empty, sub_102E3DE0), so the gauge
must stay lit while NV is active — the original does this, ours didn't.

Fixed: `CHudUHBattery::OnThink` now also checks `pPlayer->m_bNightVisionOn`.

### 2. Battery bar was continuous, original is chunked (fixed)

`sub_100BDC80` draws the charge bar as **discrete chunks**:
`chunkCount = BarHeight / (BarChunkHeight + BarChunkGap)` (= 23 / 3 = 7),
`enabledChunks = round(charge/100 * chunkCount)`, filled chunks at the bottom,
exhausted above (bottom-up). The port drew one continuous filled rect.

Fixed: `CHudUHBattery::Paint` now draws the chunked bar (7 × 2 px chunks,
1 px gap, bottom-up), matching the decompile.

### 3. Battery count text format (fixed)

Original formats `"   x%i"` (3 leading spaces) with the HudNumbers font. The
port printed `"x%i"`. Fixed to `"   x%i"`.

Position: the original draws it at `DrawSetTextPos(0,0)` (panel-local top-left,
overlapping the contour). The port draws at (BarInsetX+BarWidth+2, BarInsetY-8).
Left as-is (the original's (0,0) placement looks like an oddity; the port's
position is a sane interpretation). TODO if exact pixel parity is wanted.

### 4. Hermit cards — fade model differs (documented, not changed)

Original (`sub_100BCFA0` / `sub_100BD080`) is a **binary show/hide**: a show
flag + `SetAlpha(255)` while "changed within the last 3 s", hidden (alpha 0)
once stable 3 s. The port fades the panel gradually (`m_iAlpha -= 1` per think)
after the 3 s window. Functionally equivalent (appears on change, gone after a
few seconds); the port's crossfade is a minor visual divergence. Not a bug.

### 5. Missing CHudDotReticle (TODO)

The original client RTTI has `CHudDotReticle` (HudLayout.res `HudDotReticle`,
dotx/doty 8, xpos `c-8` ypos `c-8` — a centered dot reticle). The port does not
implement it. Out of scope for the inventory HUD work; note for later.

### 6. Doc/code mismatch (stale docs)

`docs/UNDERHELL.md` (battery stage 16) claims "our port fills one chunk per
battery (count-capped)". The code actually fills from `m_flUHBatteryCharge`
(the continuous 0..100 charge), which is what the original does too (offset
5212). The docs were stale; the code was correct. (Now also chunked — see §2.)

### Stamina / endurance — verified correct (no change)

- `sub_100CAC10` (stamina): reads m_flSuitPower (5168); `>= 35` → "StaminaNormal",
  `< 35 && > 0` → "StaminaLow", `<= 0` → no sequence. Port matches.
- `sub_100C8710` (endurance): reads m_iEndurance (3432); `>= 50` → High,
  `>= 20` → Medium, `> 0` → Low, `<= 0` → none. Port matches.
- Both fire the animation sequence every think (no early-out), value cached at
  `this[79]`. Port matches.
- Colour: both bars draw with the panel `FgColor`, animated by
  HudAnimations.txt (FgColor "0 128 255" blue ↔ "230 230 50" yellow ↔
  "DamagedFg" "180 0 0" red). Port uses `GetFgColor()` — correct.
- Fill direction: stamina horizontal left→right (BarChunkWidth 1, gap 0 → 210
  chunks); endurance vertical bottom-up (BarChunkHeight 1, gap 0 → 84 chunks).
  Port matches the vanilla CHudSuitPower chunk pattern.

The only remaining stamina/endurance nit: the exhausted-portion alpha uses a
`BarDisabledAlpha` default of 20 (the original modded panel used a
"HullDisabledAlpha"; vanilla suit power uses 70). Cosmetic only.

## Dot reticle (CHudDotReticle) + battery fade — implemented

### Battery fade — matched to decompile

`CHudUHBattery::OnThink` faded the gauge at 1 unit per think (int); the
original `sub_100BDF90` fades `GetAlpha() - 0.1` per think (float). Changed
`m_iAlpha` (int) → `m_flAlpha` (float) and the fade to `- 0.1f`, so the gauge
lingers the same way as the original instead of snapping away in ~4 s.

### CHudDotReticle — the "+use" dot

New client element `game/client/underhell/hud_dotreticle.{h,cpp}`, panel
"`HudDotReticle`". Decoded from the original:

- **Constructor** `sub_100BCC90`: registers the panel, 4 animation vars
  (`dotx`/`doty`/`dottall`/`dotwide`), `SetAlpha(128)`, `SetHiddenBits(4096)`.
- **Paint** `sub_100BC870`: draws the dot with
  `alpha = (3.0 - (curtime - flTriggerTime)) * 85` — full (255) at the trigger,
  linear fade to 0 over **3.0 s**; skipped while iron-sighted
  (`m_bIronSighted` @4140).
- **Trigger timestamp** is a client-local float at player offset **3456**,
  stamped by the free-aim input path (`update_freeaim %f %f %f` engine cmd in
  the same paint). The free-aim camera is still TODO.

HudLayout.res `HudDotReticle [$WIN32]`: `xpos c-8 ypos c-8 wide 16 tall 16`
(centred 16x16), `dotx 8 doty 8`, `PaintBackgroundType 2`, visible/enabled.

Port behaviour (self-contained, documented divergence): the +use press edge is
detected in `OnThink` (`m_nButtons & IN_USE`, latched) and stamps a panel-local
`m_flTriggerTime`; `Paint` draws a small centred dot (4x4 by default) fading
over 3.0 s and hidden while iron-sighted. The original's free-aim dependency
(`update_freeaim`, player-offset-3456 timestamp) is intentionally not ported —
the free-aim camera is a separate tracked TODO.

- `SetHiddenBits(1<<12)` = 4096: an Underhell custom hide bit beyond the
  vanilla `HIDEHUD_BITCOUNT` (12). Toggled by the mod's `SetStatusVisibility` /
  `SetHudVisibility` player inputs (also TODO — see VMF audit §D).
- The `hud_reticle_scale/minalpha/maxalpha/alpha_speed` ConVars
  (`sub_102B6B30/60/90/BC0`, defaults 1.0/125/255/700) belong to a *different*
  reticle (the zoom/crossbow reticle, `ZoomReticleColor`); the dot reticle's
  fade is hard-coded (85.0, 3.0 s) and does not use them.
- `dotwide`/`dottall` default to "1" in the original (the `.res` only sets
  `dotx`/`doty`; the paint's exact rect size is ambiguous in the decompile).
  The port defaults them to 4 for a visible dot — tune via the `.res` if needed.

## Reticle + battery/stamina/endurance fixes (decompile-verified, round 2)

### Dot reticle — it is a caret, not a square

The first reticle port drew a filled 4x4 square (the "semi-transparent square"
seen in game). Decoding `sub_100BC870` carefully, the original draws **two
filled rects**:

- `DrawSetColor(255,255,255,alpha)` then a **2x8** rect, then
  `DrawSetColor(0,0,0,alpha)` then a **3x8** rect at the same origin — i.e. a
  small crosshair caret (white 2x8 with a black 1px outline), not a square.

The fade is `alpha = (3.0 - (curtime - trigger)) * 85` (full at trigger, 0 at
3.0 s). Fixed to match: black 3x8 tick + white 2x8 tick at dotx/doty (8,8).

Reference cross-check: `eXeC64/NightmareHouse` (`src/game/client/hud_crosshair.cpp`)
implements the same "+use reticle" family — alpha ramping with
`hud_reticle_alpha_speed` (700/s), lit on `IN_USE` + a linger window. Underhell
keeps the same ConVars (`sub_102B6B30/60/90/BC0`: scale/minalpha/maxalpha/
alpha_speed) but its `CHudDotReticle` fade is the hard-coded 3.0 s / 85.0 ramp,
not the convar-driven one — the convars belong to the zoom/crosshair reticle
(`ZoomReticleColor`).

### Battery — contour drawn ON TOP of the bar

`sub_100BDC80` order: (1) chunked bar (filled HullColor + exhausted at
HullDisabledAlpha, bottom-up), (2) `hud_battery_contour` textured rect ON TOP,
(3) "   x<N>" count text. The first port drew the contour FIRST then the bar,
so the bar could overdraw the frame. Reordered to bar → contour → text.

### Stamina / endurance — missing icon sprites (added)

The ctors precache icon textures the first port never drew:
`sub_100CB4C0` (stamina) loads `sprites/hud/hud_stamina`; `sub_100C8F40`
(endurance) loads `sprites/hud/hud_endurance`. Both are the gauge outline art
drawn behind the chunked bar. Added:

- `CHudStamina`: `hud_stamina` at iconx 1 / icony -6 / iconwide 24 / icontall 24.
- `CHudEndurance`: `hud_endurance` at iconx 2 / icony 116 / iconwide 16 /
  icontall 134 (per HudLayout.res — the endurance icon is a tall vertical
  outline; exact placement needs an in-game glance since icontall 134 > panel
  height, the mod ships it that way).

### Sprite inventory (materials/Sprites/Hud/)

`hud_battery_contour`, `hud_battery_meter`, `hud_blooddrop`, `hud_endurance`,
`hud_flashlight`, `hud_hermitcards`, `hud_shoulderflashlight`, `hud_stamina`.
Note `hud_battery_meter` + `hud_flashlight` + `hud_shoulderflashlight` are not
referenced by the HUD ctors in the decompile (the battery bar is drawn as
chunks, not the meter texture) — `hud_flashlight` / `hud_shoulderflashlight`
belong to the (TODO) shoulder-flashlight viewmodel HUD.
## Diaphora reference (new asset in the hexrays repo) — how to use it

`klaxons1/underhell-hexrays` now ships a Diaphora export + a full re-decompile:
`Underhell/bin/diaphora/cliento.diaphora` (SQLite) and
`Cliento_diaphora.dll.c` (24 MB, Hex-Rays 9.1).

### What it is

- `cliento.diaphora`: Diaphora match of `Cliento.dll` (Underhell) vs
  `original/Client.dll` (vanilla OB). **11405 matched / 12685 unmatched**.
  Match types: best 10199 / multimatch 1199 / partial 7.
- `Cliento_diaphora.dll.c`: the full Hex-Rays 9.1 decompile of Cliento.dll with
  Diaphora names + RTTI class names applied (e.g. `CHudDotReticle::`vftable'`).

### Value (real)

1. **Better decompile reference** — Hex-Rays 9.1, single grep-able file,
   struct-typed in places. Prefer it over the old per-function `sub_*.cpp`.
2. **RTTI class attribution is correct** — the vtable owners
   (`CHudDotReticle`, `CHudUHBattery`, `CHudEndurance`, `CHudStamina`,
   `CHudBleeding`, `CHudUHHermitCards`) match the vtable analysis in § RTTI
   validation. Confirms the class mapping.
3. **`unmatched` = the Underhell delta** — a clean to-do map of the 12685
   functions that are mod-specific (HUD, inventory, weapons, gear).

### Caveat (critical): false positives in the modified regions

Diaphora matches by AST/pseudocode similarity, and Underhell REPLACED vanilla
code in the HUD/inventory/weapon regions. There, custom functions get spurious
matches to nearby unrelated vanilla functions — with ratio 1.0. Concrete cases:

| Underhell function | what it really is | Diaphora named it |
|---|---|---|
| `sub_100BDC80` | battery HUD paint (reads m_iUHBatteryCount@5292, draws chunked bar + "x<N>") | `CAsyncCaptionResourceManager::~CAsyncCaptionResourceManager` |
| `sub_100BE800` | bleeding HUD paint (m_iBleedCounter@3436 * 2.55) | `CHudCloseCaption::CHudCloseCaption` |
| `sub_100BCFA0` | hermit-cards think | `CUtlVector<CaptionLookup_t>::CopyArray` |
| `sub_100BD080` | hermit-cards paint | `CUtlVector<AsyncCaption_t>::RemoveAll` |

All "best"/"multimatch", ratio 1.0, all wrong. The tell: genuine matches carry
the region's consistent address shift (~0x13xxx); these false matches have
anomalous small deltas (-0x530 … -0x30).

**Rule**: never trust a Diaphora name in the HUD/inventory/weapon regions
without reading the code. Use it as a MAP (unmatched = delta, matched = vanilla
framework), not ground truth.

### Reticle re-confirmed (caret, not square)

The 9.1 decompile of the dot-reticle paint (`sub_100BC870`) confirms the first
fix: it draws `DrawSetColor(white, alpha)` + `DrawFilledRect(dotx, doty, 2, 8)`,
then `DrawSetColor(black, alpha)` + `DrawFilledRect(dotx, doty, 3, 8)` — a tiny
2-3 px × 8 px tick, NOT a filled square. `alpha = (3.0 - (curtime - trigger)) * 85`.
The exact screen origin is ambiguous from the decompile (the rect coords read
degenerate); verify pixel position in game.

## Battery HUD — field map re-verified against Diaphora 9.1 decompile

Used `Cliento_diaphora.dll.c` (Hex-Rays 9.1) to nail the CHudUHBattery member
offsets via the paint (`sub_100BDC80`) + ctor anim-var registrations
(`sub_100BE0A0..100BE520`). Confirms the port's bar logic is correct:

| member | offset | value (HudLayout.res) | role |
|---|---|---|---|
| charge (float, cached) | +220 | player m_flUHBatteryCharge | 0..100 |
| HullDisabledAlpha | +232 | "0" | exhausted-chunk alpha |
| HullColor | +237..240 | "2 127 252 192" | filled-chunk color |
| BarInsetX | +252 | 6 | chunk x0 |
| BarInsetY | +260 | 31 | chunk y0 (bar bottom anchor) |
| BarWidth | +268 | 14 | chunk x1 offset |
| BarHeight | +276 | 23 | chunkCount = BarHeight/(ChunkHeight+Gap) = 7 |
| BarChunkHeight | +284 | 2 | chunk y1 offset |
| BarChunkGap | +292 | 1 | y step = 2+1 = 3 |
| contourx/contoury | +300/+308 | 1/0 | contour rect x0/y0 |
| contourtall/contourwide | +316/+324 | 42/24 | contour rect y1/x1 |

Verified correct in the port:
- fill is bottom-up: `y = BarInsetY` (31), `y -= step` (3) per chunk, 7 chunks.
- filled color = HullColor (alpha 192); exhausted = HullColor.rgb +
  HullDisabledAlpha (0 → exhausted invisible, bar shrinks).
- draw order bar → contour (`hud_battery_contour`) → "   x%i" text.
- think: `m_bFlashlightOn(5286) || m_bNightVisionOn(3449) || count(5292) changed`
  → alpha 255; else `alpha -= 0.1`.

Two quirks found and matched:
- count text is drawn at `DrawSetTextPos(0,0)` (panel top-left), the 3 leading
  spaces in "   x%i" push the digits right — port now matches (was a guessed
  22,23).
- the contour rect is drawn as `DrawTexturedRect(contourx, contoury,
  contourwide, contourtall)` — contourwide/tall used as ABSOLUTE x1/y1, not
  added to x0/y0. The port adds them (1 px difference, imperceptible); left
  as-is.

## Diaphora filter script (devtools/bin/diaphora_filter.py)

Splits a `.diaphora` SQLite export into three tiers by address-delta:

- `reliable` — "best" matches with delta >= 0x10000 (the image shift; same
  function moved by the mod's inserted code). Trust these names.
- `verify` — multimatch/partial, or best with small/zero/negative delta. These
  are the false positives (e.g. battery paint -> caption dtor) + ambiguous.
  Read the code.
- `mod_delta` — the `unmatched` table: functions with no vanilla match (the
  mod's additions + any vanilla functions Diaphora failed to match).

Run: `python3 devtools/bin/diaphora_filter.py cliento.diaphora [--min-delta
0x10000] [--out DIR]`. On the client export it produced 6482 reliable / 4923
verify / 12685 mod_delta, and correctly routed all four known false positives
(sub_100BDC80/100BE800/100BCFA0/100BD080) into `verify`.

Note: the delta signal is not absolute — `best` + large positive delta is a
STRONG trust signal, but `verify` still contains some genuine matches (and
`unmatched` contains vanilla functions Diaphora simply couldn't match, e.g.
CAchievementNotificationPanel). Use the tiers as a triage map, not ground truth.

## Free-aim (weapon tilts toward the mouse) — NOT a standalone feature

The "weapon sways/tilts toward the mouse when not aiming" is a SIDE EFFECT of
the third-person OTS (over-the-shoulder) camera + aim-angle separation, NOT a
standalone first-person feature. A first port attempt (based on the VDC
"Over the Shoulder View" tutorial's OPTIONAL free-aim section, shipped in the
mod's `notes/`) was reverted after it felt wrong in game. Decode corrections:

### What the binary actually does (not the tutorial)

- `cam_ots_freeaim_enable` is read in `sub_10014D80`
  (`CBaseViewModel::CalcViewModelView`), where it gates the unsighted
  viewmodel tilt.
- `sub_100D7980`, reached from `CInput::MouseMove`, is the actual cursor
  update routine. It accumulates `m_yaw * mouse_x / 90` and
  `m_pitch * mouse_y / 90`, normalizes the resulting `Vector2D`, then caps its
  length at `cam_ots_freeaim_move_max` (**0.1** by default). Normal camera
  `ApplyMouse` still receives the full delta immediately afterwards: this is
  not the VDC tutorial's deadzone/overflow turn implementation.
- The other tutorial-era ConVars are registered with the original defaults
  (`interval_enable=0`, `move_threshold=0.05`, `speed_turn=1`,
  `speed_evenYawSpeed=0`, `autoturn_speed=250`); no additional read was found
  in the recovered free-aim path.
- `sub_10014D80` maps that cursor to pixel coordinates as
  `(cursor * 0.25 + 0.5) * screen_size`, calls `ScreenToWorld`, converts the
  ray using `VectorAngles`, and applies the result to the viewmodel only. It
  retains the viewmodel origin and skips the effect while iron-sighted.
- `update_freeaim %f %f %f` is a server ConCommand; the client dot-reticle paint
  `sub_100BC870` computes the same screen ray and sends the world target each
  frame.

### Port status

The client implementation now mirrors the confirmed visual path:
`CInput::MouseMove` retains the bounded cursor, while
`CBaseViewModel::CalcViewModelView` points an unsighted first-person model
through the corresponding `ScreenToWorld` ray. It intentionally does **not**
introduce unverified tutorial auto-turn/deadzone behaviour. Full OTS camera
angle separation and server-side `update_freeaim` target syncing remain
separate work; they are not necessary for the first-person weapon tilt.
