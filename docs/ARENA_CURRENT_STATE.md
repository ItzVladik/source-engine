# Arena session implementation ledger — `arena/01a007ab-source-sdk-2007`

> ## MANDATORY VERIFICATION WARNING
> **This document and the changes it describes were produced by the dumbest LLM during an Arena session. Nothing in this file is proof of correctness, completeness, buildability, or 1:1 Underhell parity. Every entry requires source review, a clean client/server rebuild, and in-game comparison with the original before merge into `master`.**
>
> This warning is intentionally prominent because the session included speculative implementations, several follow-up corrections, and two reverted false interpretations. Treat this document as an audit ledger, not release notes.

## Scope and baseline

- Branch: `arena/01a007ab-source-sdk-2007`
- Baseline: `2c9f546ce0fb42c4b646e10a5ec71d0374c9bb16`
- Original server reference used: `Underhell/bin/diaphora/servero_diaphora.dll.c`
- The Windows Source SDK 2007/Orange Box game DLLs were **not built in this Linux Arena sandbox**.
- The branch requires a clean rebuild of both `client.dll` and `server.dll`.

## Explicit non-claims

The following are **not** complete or certified 1:1:

- `CNPC_CombineS` corpse/dismemberment state machine;
- all custom items and inventory use/drop behavior;
- `npc_infected`, Ace, Butcher, combine/citizen AI;
- SOCOM laser lifecycle;
- bullet time `CBullet` and common `FireBullets` integration;
- NPC-driven jeep entry/seat behavior;
- shader installation/runtime behavior for gasmask/night vision.

Several changes below improve a concrete discrepancy, but an improvement must not be misreported as a finished subsystem.

---

# Implemented/edited subsystems

## 1. Corpse, ragdoll, dismemberment, body dragging

**Primary files**

- `game/server/underhell/uh_ai.cpp`
- `game/server/physics_prop_ragdoll.{h,cpp}`
- `game/server/underhell/uh_carry_ragdoll.cpp`
- `game/server/hl2/hl2_player.{h,cpp}`
- `game/server/hl2/npc_combines.cpp`

### Added and changed

- Corpse `+use` support and pickup through stock `CPlayerPickupController`:
  - `FCAP_IMPULSE_USE` capability;
  - `CRagdollProp::Use` calls `CHL2_Player::PickupObject( body, false )`;
  - normal mass limit is bypassed for corpse dragging.
- Ragdolls are not treated as flesh by physgun logic (`VPhysicsIsFlesh() == false`).
- Drag blood follows decoded `DraggedThink` cadence:
  - update cadence: `0.25 s`;
  - decal only after >4 units of X/Y body motion;
  - stationary held bodies no longer continuously paint blood.
- Corpse carry penalties now follow decoded `sub_102E1350`/pickup shutdown:
  - player records `m_hCarryingRagdoll` and original `m_fSavedSensitivity`;
  - a full body is identified by `ragdoll.listCount > 10` (the old code incorrectly tested root-object mass);
  - speed is fixed to `hl2_normspeed / 3` and mouse sensitivity to `saved / uh_bodymousedamper`;
  - release restores `hl2_normspeed` and the exact saved sensitivity.
- Ragdoll hitgroup recovery walks ragdoll physics parent elements to map physics bones back to hitgroups.
- Dismemberment tracks head/arm/leg accumulated damage, helmet damage, and prevents duplicate transitions through bodygroup state.
- Corrected bodygroup lookup to tolerate `arms`/`Arms`, `legs`/`Legs`, etc.
- Corrected combine arm loss encoding:
  - regular combine soldier: authored `0/2/4/6` arm states;
  - four-state variants: `0/1/2/3` states.
- Heavy leg bodypart variants use `leftleg2/rightleg2` for combine heavy-leg bodygroup values.
- Added/preloaded severed bodypart models for standard combine, prison guard, PMC and heavy-leg variants.
- Detached pieces now use `CreateServerRagdollSubmodel`, rather than an independently spawned reference-pose `prop_ragdoll`.
- Detached limbs spawn at authored sever attachments (`ForeArm_L/R`, `Calf_L/R`) and retain source skin; arm pieces copy `Glove_L/Glove_R` state.
- Detached limbs inherit source ragdoll linear/angular velocity to reduce unstable first-frame launches.
- Source ragdoll constraints are **not** deliberately broken during limb removal; the source mesh is bodygroup-hidden and a separate sub-ragdoll is created.
- Original-style effects added/adjusted:
  - `Player.Splat` on actual sever;
  - `Player.HeadShot` plus `UTIL_BloodSpray` for head destruction;
  - `blood_zombie_split_spray` for arms;
  - `blood_advisor_puncture_withdraw` for legs;
  - body decals cleared on successful sever.
- Gear/helmet handling was expanded:
  - eye attachment is used for dropped head gear;
  - model-specific helmet item selection: guard, prison, PMC, worker;
  - added missing `item_gasmask_prison` entity registration;
  - prison corpses use `item_gasmask_prison`, others use guard gasmask where applicable.

### Important correction/revert

Commit `7200b4b` incorrectly made `+use` strip a helmet from a corpse. It was explicitly reverted by `0fc4916`. Current intended behavior is: **helmet removal is by shooting/damage, not by `+use`**.

### Still missing / high-risk

- Dismemberment was reworked against `underhell-hexrays` functions `sub_10031BF0`, `sub_101CE6F0`, `sub_101CDCC0`, `sub_1035AAA0`, `sub_101A7B70`, and the NPC-to-ragdoll transfer callers. It now keeps remaining-health counters, variant/type state, sever flags and authored gib paths, and transfers them to server ragdolls.
- Family-specific arm/head transitions, infected thresholds, heavy-leg health, corpse physics-bone remapping, helmet overflow, PMC headgear, weapon loss, exact proximal/distal attachment transforms and independent body-part ragdolls are implemented.
- `uh_ai.cpp` and `physics_prop_ragdoll.cpp` pass Linux permissive syntax checks. A standalone `ai_basenpc.cpp` check is blocked by unrelated GCC 12 errors in the legacy `tier1/functors.h`; Windows DLL compilation and in-game physics/animation comparison remain mandatory.
- Original client-ragdoll conversion at LRU retirement is still represented by the SDK server-ragdoll LRU/fade behavior rather than a proven byte-for-byte equivalent client conversion.

---

## 2. Bullet time

**Files**

- `game/server/underhell/uh_bullettime.{h,cpp}`
- `game/server/underhell/uh_weapons.cpp`
- `game/server/hl2/hl2_player.{h,cpp}`

### Added

- ConVars added/used:
  - `bt_enabled` default `0`, cheat/replicated;
  - `bt_timescale` default `0.3`;
  - `bt_enemybulletspeed` default `500`;
  - `bt_playerbulletspeed` default `2000`;
  - `bt_plr_speed` default `250`.
- `bt_enabled` callback changes `host_timescale`, player max speed, overlay and start/end sound playback.
- `impulse 110` toggles BT only while the player's original BT gate allows it.
- `EnableBT` only clears that gate; `DisableBT` sets it and schedules the actual shutdown one game-second later. This matches Chapter1_16, where EnableBT occurs at t=3, alias `bt` starts BT at t=4, and DisableBT at t=9 ends it at t=10 (about 20 real seconds at timescale 0.3).
- BT sound scripts are precached (`Player.bullettimestart`, loop/end).
- Visible bullet model selection follows decompiled ammo IDs:
  - IDs 3/4: `bt_9mm`;
  - ID 5: `bt_357`;
  - ID 7: pellet;
  - otherwise `bt_762`.
- Underhell gun fire currently performs normal hit-scan `FireBullets` and then creates visible BT bullet props.
- Current visible props are registered in `CUHBulletMotionSystem`, which reapplies velocity every `0.05` game seconds, matching the cadence of original `CBullet::Think` (`sub_101078D0`):
  - BT speed = enemy/player speed cvar × `bt_timescale`;
  - non-BT speed = `2500`.
- Artificial 120-second visual-bullet removal was removed.

### Still missing / high-risk

- This is **not** original `CBullet`.
- Current visible bullets are stock replicated `prop_physics`, not original networked `CBullet` server/client classes.
- Common original `FireBullets` extension (`sub_100EAFB0`) is not ported: per-pellet post-trace behavior, collision/damage/penetration sequencing and anti-recursion flag remain absent.
- Vanilla weapon fire and enemy weapon fire are not comprehensively integrated.
- The user reported BT duration/behavior as too short. The latest velocity lifecycle change addresses visible bullet freezing, but BT duration and all runtime behavior require testing against original.

---

## 3. NPC-driven jeep and mounted gun

**Files**

- `game/server/hl2/vehicle_jeep.{h,cpp}`
- `game/server/player.cpp`
- `game/server/vehicle_base.cpp`
- `game/server/hl2/npc_playercompanion.{h,cpp}`
- `game/server/npc_vehicledriver.{h,cpp}`
- `game/shared/vehicle_viewblend_shared.cpp`

### Added/changed

- `Uh_Chapter1_16_d.vmf` was audited: Bryan enters through `EnterVehicleImmediatelyAsDriver`, the map toggles `ToggleGunMode`, and the map itself drives the jeep through `Throttle`, `Steer`, and `HandBrake` inputs.
- Removed the fabricated hidden `npc_vehicledriver`, which fought the VMF controls every frame. Bryan remains the visual driver while the player occupies the independent legacy gunner passenger slot.
- Restored separation of `m_hPlayer` and `m_hNPCDriver`; `GetDriver()` now reports the actual player passenger, while vehicle thinking independently accounts for an NPC controller.
- Player-at-mounted-gun state is map-controlled (`m_bPlayerAtGun`) and the gunner camera uses `vehicle_gunner_eyes`.
- Mounted jeep fire was reworked against server Hex-Rays:
  - mounted mode uses `AR2Tracer`, generic impact behavior, `muzzle_star_uh`, and `FuncTank.Fire` rather than the stock Gauss beam/explosion and `PropJeep.FireCannon`;
  - restored jeep `m_nAmmoType` and `sk_jeep_gauss_damage` through SDK 2007 `FireBulletsInfo_t::m_iDamage`;
  - fire interval is `0.1`, spread is `0.0087299999`, and the muzzle basis is the authored `muzzle` attachment;
  - decoded aim clamps are ±200 yaw and ±20 pitch; the model's pose-parameter ranges provide the final visual clamp.
- A compile correction changed invalid `FireBulletsInfo_t::m_flDamage` to `m_iDamage`.

### Still missing / high-risk

- Corrected the mounted-gun basis from stock `gun_ref` to Underhell's authored `muzzle` attachment in both aiming and firing; this addresses the sideways gun roll on entry.
- Mounted aiming/firing now gates on `EnableMountedGun` plus map-controlled `m_bPlayerAtGun`, matching `sub_103EBB80`, `sub_103EC540`, and `sub_103EAB30`.
- `BodyTarget` now selects `vehicle_gunner_eyes` in gun mode, matching `sub_103EA8F0`.
- The current changes still require a clean Windows DLL build and runtime validation on `Uh_Chapter1_16_d`; the chapter map’s rail/stuck-path behavior has not been tested here.

---

## 4. SOCOM laser

**Files**

- `game/server/underhell/uh_weapons.{h,cpp}`
- `game/server/hl2/weapon_rpg.{h,cpp}`
- `game/server/hl2/hl2_player.{h,cpp}`
- `game/client/underhell/uh_laser.cpp`

### Added/changed

- SOCOM secondary attack toggles a real `env_laserdot` path rather than a fake client beam.
- Server helper for laser-dot position updates was added to RPG laser-dot support.
- The dot is traced/updated during weapon frame handling; holstering removes it.
- Player laser state uses public `CHL2_Player` accessors after a private-member compile failure.
- Player laser behavior was restricted to SOCOM paths.

### Still missing / high-risk

- User reported bad laser behavior during reload/weapon switching. No runtime verification proves this is solved.
- Original SOCOM lifecycle (`sub_1027B9E0`, cleanup/update/create functions) remains only partially reconstructed.

---

## 5. Weapons, melee, kick and weapon scripts

**Files**

- `game/server/underhell/uh_weapons.{h,cpp}`
- `game/shared/weapon_parse.{h,cpp}`
- `game/server/underhell/uh_kick.cpp`

### Added/changed

- Weapon parse support includes `MeleeDelayedFire`, `MeleeRoF`, `MeleeRange`, `StaminaToDrain`, `PunchPitch`, `PunchYaw`, `UH_Weapon_Special`/fire mode.
- Script fire mode is applied after weapon data initialization.
- Underhell melee primary gate uses 15 stamina; attack drains configured stamina and chooses normal/exhausted voice based on 35 stamina.
- Unsupported vanilla melee secondary swing is disabled for Underhell melee weapons.
- Shotguns got pump scheduling/animation behavior.
- `uh_jake_kick` was changed toward decoded reach/hull fallback, front-facing gate, melee force and rumble behavior.
- Melee voice sound scripts were precached to fix reported runtime precache errors.

### Still missing / high-risk

- Delayed melee hit execution is not exact: base bludgeon swing is still immediate.
- Most per-weapon timing, recoil, reload, damage, penetration and animation data are estimates unless separately verified.

---

## 6. Custom item/inventory work

**Files**

- `game/server/underhell/uh_items.{h,cpp}`
- `game/server/underhell/uh_player_inventory.cpp`
- `game/shared/underhell/uh_inventory.{h,cpp}`
- `game/client/underhell/c_inventory_panel.cpp`

### Added/changed

- Soda map-assigned skin is preserved; random skin is applied only when skin is zero.
- Bandage item use preserves the inventory slot and plays denial when no health/bleed effect is possible.
- Radio/cracker activation was adjusted: delayed initial activation, stable track selection, radio sound insertion, no fake timed cracker explosion, pickup restrictions while active.
- Failed active radio/cracker creation preserves inventory slot.
- Glowsticks now use the original extended `CFlare` network mode (`m_bGlowStick`, `m_nSkinNumber`): a coloured dynamic light follows the hidden player-parented holder with no flare sprite, smoke, burn touch or flare presentation. Dropping/replacing reveals only the physical glowstick model while its dedicated coloured light remains attached.
- Flare packs create the original `env_flare` on the left viewmodel `fuse`; sequence 1 deploys and sequence 4 stages the throw before release. Thrown flares use `CPhysicsProp::CreateFlare`, share the total 90-second burn from equip time, use `uh_flare_throw_scale 1200`, collision group 3 and angular velocity `(200,200,200)`, and can be picked back into inventory through `CPhysicsProp::Use`.
- Inventory world styling writes `m_nSkin` at decoded CBaseAnimating+848 instead of changing unrelated bodygroups. Lit glowsticks release their tracked pickup-capable physics prop.
- Grenade throws now play the authored throw sequence on viewmodel index 1 rather than incorrectly animating the active weapon.
- Kick processing honors the door `kickable` key and runs the dedicated unlock/open path for prop and brush doors.
- Added `item_gasmask_prison` due corpse gear drop requirements.
- Heavy armor, apple, banana, soda and basic armor received partial value/skin fixes.

### Still missing / high-risk

- `item_syringe` remains placeholder model/value behavior.
- `item_syringepack` and `item_bandagespack` were identified in original item-random tables but are still not implemented as entities.
- Generic inventory virtual-success/failure/drop/held-object lifecycle is not complete.
- Full item audit remains unfinished.

---

## 7. Free aim, HUD, player presentation and gear overlay

### Free aim

**Files:** `game/client/in_mouse.cpp`, `game/client/underhell/uh_freeaim.h`, `game/client/underhell/hud_dotreticle.*`, `game/shared/baseviewmodel_shared.cpp`, `game/server/hl2/hl2_player.*`

- Free-aim cvars/cursor movement and viewmodel screen-ray tilt were added.
- `update_freeaim x y z` client-to-server route stores a target vector on `CHL2_Player`.
- Dot reticle now stores its trigger timestamp on `C_BaseHLPlayer` and refreshes it from every `CreateMove` command containing `IN_USE`, matching client `sub_10044590`; the previously corrupted include line was also repaired.

**Not complete:** custom weapon shots/muzzle paths do not all consume the server free-aim target, so this is not full original free aim.

### HUD

- Battery HUD now uses original hidden bits, panel alpha lifecycle and integer fade behavior from `sub_100BDF90`.
- The charge bar reads the original `m_HL2Local.m_flFlashBattery`; flashlight and night vision share that authoritative charge, consume discrete batteries and no longer fight Episodic's rechargeable flashlight path.
- Flashlight on/off now updates the networked `m_bFlashlightOn` flag used by the HUD.
- `+use` item pickups fire `CItem::OnPlayerPickup` inside the successful MyTouch path, before `UTIL_Remove`, matching Diaphora. In `uh_house_0_tutorial_d`, four batteries plus two packs increment `Counter_Batteries_Drawers` to 6, start scene 11 and unlock the tutorial lever door.
- Added original `CHudGrenadeAmmo`: independent numeric grenade-ammo display, `sprites/hud/weapons/frag`, original hide mask and GrenadeIncreased/Decreased/Empty animation events.
- Hermit cards use binary 255/0 alpha at three seconds instead of gradual fade.
- Battery implementation and reticle still require in-game visual verification after rebuilding `client.dll` and `server.dll`. Disable and enabled bar geometry continues to come from the original `HudLayout.res` values.

### Mirror model

- Model swap initializes valid idle sequence/cycle/state to reduce mirror T-pose caused by stale sequence indices.

### Gasmask/night vision

- Client overlay material names follow original `shader/nightvision` and `shader/gasmask` path.
- Likely remaining issue is custom shader DLL/material installation, not confirmed gameplay code.

---

## 8. NPC work

### Infected

- Corrected original internal variant order and FGD-key mapping:
  - inmate 0, worker 1, doctor 2, uniform 3, urban 4, rural 5, guard 6, office 7.
- Rebuilt per-family skin/head/arm/glove/helmet/respirator randomization from `sub_101A6620`, including the distinct 0/1/2/3 versus 0..7 arm-state families.
- Corrected original defaults (`uh_infected_door_dist 37`, efficiency 1), SpeedModifier semantics, Zombine sound set, duplicate NPCInit, efficiency/cower handling and short door probe.
- Existing custom schedules cover climb unstick, door bash, radio investigation/destruction, sprint slots and melee; shared dismemberment transfers infected part state into corpses. Speculative cower selection and the fabricated forward door hull probe were removed because they are not present in the matched Diaphora functions.
- Removed an incorrect fabricated head-hit call in `OnTakeDamage_Alive`; base `TraceAttack` remains responsible for actual hitgroup routing.

**Still runtime-sensitive:** exact animation availability, radio navigation edge cases, equipment choice and door-break timing require in-game comparison across all eight models.

### Ace

- `npc_ace` base entity, cloak inputs and basic combat/convars were added.
- **Known structural error:** original Ace derives from `CAI_PlayerAlly`; current code derives from `CNPC_CombineS`. This requires rewrite.

### Butcher

- `npc_butcher` base and required abstract zombie virtual methods were added; this fixed a reported abstract-class compiler error.
- Custom full schedule/task/charge/physics behavior is incomplete.

---

# Build/runtime errors fixed during this session

- Butcher abstract `CNPC_BaseZombie` virtual-method compiler errors.
- SOCOM private `m_bLaserToggleState` access compiler error via public accessors.
- Unprecached melee voice script errors.
- Unprecached bullet-time start/end script sound errors.
- Jeep `FireBulletsInfo_t::m_flDamage` compiler error corrected to SDK 2007 `m_iDamage`.

# Reverted mistakes during this session

- `7200b4b` added helmet removal by corpse `+use`; reverted in `0fc4916` because shooting is the intended helmet-removal mechanic.
- Jeep mounted-gun edits briefly removed charged cannon implementation; later commits restored it. The branch must be clean-built and tested, not trusted from history alone.

# Commit index

The full session commit trail is available with:

```bash
git log --oneline 2c9f546..arena/01a007ab-source-sdk-2007
```

Important recent commits:

| Commit | Subject |
|---|---|
| `363eb09` | Restore bullet-time bullet velocity lifecycle |
| `0a982ff` | Fix jeep bullet damage field for SDK 2007 |
| `f96a780` | Implement original mounted jeep gun fire path |
| `7850393` | Inherit corpse motion for detached limbs |
| `f698f4e` | Transfer glove state to detached corpse arms |
| `8922741` | Select and precache combine heavy-leg corpse gibs |
| `e3f1ab9` | Restore prison gasmask corpse drop |
| `0fc4916` | Revert incorrect helmet recovery from corpse use |
| `9b4f1d1` | Jeep gunner seat gate and bullet tracer replication |
| `d4824bb` | Add `update_freeaim` client/server path |
| `e21136b` | Correct infected variant ID mapping |

# Mandatory verification checklist before any real merge

1. Clean rebuild `client.dll` and `server.dll` with the target Source SDK project.
2. Run a compile pass that includes `vehicle_jeep.cpp`, `uh_ai.cpp`, `uh_bullettime.cpp`, all Underhell entities and both game DLL projects.
3. Test corpse shots for each model family: normal combine, prison guard, PMC, worker/infected and heavy-leg variants.
4. Test corpse carry/drop, speed restoration, ragdoll collisions and detached-limb stability.
5. Test `impulse 110`, BT duration, start/end, player/enemy shots, pellet shots, collision behavior and performance.
6. Test `Uh_Chapter1_16_d`: NPC driver starts, player can enter gunner role, camera is correct, gun fires, charged cannon is intact, no duplicate tracer/beam.
7. Test SOCOM laser through reload, holster, weapon switch, death and map transition.
8. Test all item random pool entries and inventory full/denied/use/drop behavior.
9. Compare screenshots/logs against original Underhell, not only decompiler output.
10. Remove or rewrite speculative code before claiming a subsystem is 1:1.
