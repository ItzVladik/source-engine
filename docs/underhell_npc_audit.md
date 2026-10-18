# Underhell NPC implementation audit

Date: 2026-08-18  
Branch baseline: `serveror.dll` / `Cliento.dll` from `underhell-hexrays`

## Evidence and status legend

- **Verified** — compared with ordinary split Hex-Rays and Diaphora; relevant code is implemented.
- **Partial** — the core path exists, but named original schedules/tasks or edge cases remain.
- **Stock** — Source SDK implementation is present and no Underhell-specific replacement has been identified. This does **not** mean every stock function was proven byte-for-byte.
- **Global UH layer** — stock NPC also receives the reconstructed `CAI_BaseNPC` extensions in `underhell/uh_ai.cpp`.

## Underhell-specific NPCs

| Class | Status | Implemented | Remaining differences |
|---|---|---|---|
| `npc_infected` | Partial, advanced | Eight model variants, melee-only behavior, sprinting, cower inputs, radio interest, door bash, climb handling, bodyparts/helmets and global dismemberment integration | Some schedule arbitration and animation timing are reconstructed rather than proven instruction-for-instruction; glowstick/radio attraction internals still use safe SDK adaptations |
| `npc_ace` | Partial | Assassin model fallback, health/kick/speed convars, cloak state, cloak sound, hips electrical effect, hidden held weapon, separate pain timer, regeneration below half health, dissolve sound and inputs | Original super-jump/landing force and radius, assault speech/squad selection, exact cloak rendering flags, death item-drop policy and several tactical paths remain incomplete |
| `npc_butcher` | Partial, core boss functional | Exact health/speed/damage/cooldown convars; original model, classic headcrab and soldier torso/legs; sounds and shockwave precache; AI IDs 76–81, tasks 250–259, schedules 100/105; target/start parsing; scheduled charge; collision-only damage; door breach; cower and enable/disable inputs | Physics-object search/shove/throw, summon task, opportunity throw, nearest-node/path tolerance tasks, charge crash/stop activities, exact flinch selection and all door-obstruction schedule transitions are registered but not all task bodies are reconstructed |

## `npc_combine_s`

Status: **Partial, substantially extended**.

Verified and implemented:

- normal/elite health and kick values;
- exact pain sentence selection and one-second cooldown (`sub_1033FA10`);
- exact `COMBINE_DIE` death sentence (`sub_1033FB20`);
- normal/prison/PMC bodypart model precache;
- generic Underhell bodypart health, severing and ragdoll transfer;
- health, grenade and elite AR2-altfire drop paths;
- `Shield` keyvalue;
- random riot/ballistic shield for type 1, deterministic types 2–5;
- shield attachment to model attachment `Shield`;
- shield-bearing kick damage multiplier;
- shield detachment on death;
- basic PMC skin and armor randomization.

Remaining differences from `CNPC_CombineS::Spawn` / death handling:

- exact per-model `Armor`, `Helmet`, `Legs` and `gasmask` bodygroup random distributions need model-QC verification;
- exact shield collision/damage interception is not yet reconstructed — the shield is visible and dropped, but does not yet implement the original dedicated blocking entity behavior;
- exact PMC helmet/headset/cap and guard/prison helmet drop combinations are currently handled by the global dismemberment layer rather than the complete original death matrix;
- shield type 1 is forced to a shield-compatible armor bodygroup for map reliability; the original condition depended on its preselected armor variant;
- prison heavy-leg health multiplication is approximated by the global bodygroup-based limb setup.

## Global Underhell behavior applied to NPCs

`game/server/underhell/uh_ai.cpp` extends `CAI_BaseNPC`, so many otherwise stock NPCs differ from vanilla SDK through:

- `uh_fos` and `uh_viewdistance` sensing controls;
- temporary squads (`SquadTemp`);
- body spotting and `OnSpot*` outputs;
- bodygroup parsing;
- severable arms/legs/head;
- helmet/bodypart item spawning;
- ragdoll collision mode and server-ragdoll transfer;
- gib/bodypart health counters;
- mirror-only rendering support where authored.

These features are not necessarily meaningful for every model; unsupported bodygroups fail safely.

## NPC class coverage from the Underhell FGD

### Custom or explicitly modified

- `npc_ace` — Partial
- `npc_butcher` — Partial
- `npc_infected` — Partial
- `npc_combine_s` — Partial

### Present as Source SDK classes, with the global UH layer where applicable

The following are present in the checkout and currently use their Source SDK 2007/Episodic implementation unless noted above:

- `npc_advisor`
- `npc_alyx`
- `npc_antlion`
- `npc_antlion_grub`
- `npc_antlion_template_maker`
- `npc_antlionguard`
- `npc_apcdriver`
- `npc_barnacle`
- `npc_barney`
- `npc_blob`
- `npc_breen`
- `npc_bullseye`
- `npc_citizen`
- `npc_combine_camera`
- `npc_combine_cannon`
- `npc_combinedropship`
- `npc_combinegunship`
- `npc_cranedriver`
- `npc_crow`
- `npc_dog`
- `npc_eli`
- `npc_enemyfinder`
- `npc_enemyfinder_combinecannon`
- `npc_fastzombie`
- `npc_fastzombie_torso`
- `npc_fisherman`
- `npc_furniture`
- `npc_gman`
- `npc_grenade_frag`
- `npc_headcrab`
- `npc_headcrab_black`
- `npc_headcrab_fast`
- `npc_heli_avoidbox`
- `npc_heli_avoidsphere`
- `npc_heli_nobomb`
- `npc_helicopter`
- `npc_hunter`
- `npc_hunter_maker`
- `npc_ichthyosaur`
- `npc_kleiner`
- `npc_launcher`
- `npc_magnusson`
- `npc_maker`
- `npc_manhack`
- `npc_metropolice`
- `npc_missiledefense`
- `npc_monk`
- `npc_mossman`
- `npc_pigeon`
- `npc_poisonzombie`
- `npc_puppet`
- `npc_rollermine`
- `npc_seagull`
- `npc_sniper`
- `npc_spotlight`
- `npc_stalker`
- `npc_strider`
- `npc_template_maker`
- `npc_turret_ceiling`
- `npc_turret_floor`
- `npc_turret_ground`
- `npc_vehicledriver`
- `npc_vortigaunt`
- `npc_zombie`
- `npc_zombie_torso`
- `npc_zombine`

These classes are **not yet individually decompiled and compared method-by-method**. Their status is therefore Stock/Unverified, not “1:1 verified.”

### FGD compatibility names requiring explicit confirmation

- `npc_clawscanner` and `npc_cscanner` are both linked by `npc_scanner.cpp`.
- `npc_combine_camera` is linked by `npc_combinecamera.cpp`.
- `npc_spotlight` is linked by `npc_spotlight.cpp`.
- `npc_crabsynth` and `npc_mortarsynth` are exposed by legacy FGD content but no concrete factory was found in this checkout; they appear to be cut/placeholder entities.

## Recommended next verification order

1. Finish Butcher task bodies and physics-object throw behavior.
2. Finish Ace super-jump, landing shockwave and exact cloak flags.
3. Verify Combine shield collision interception and helmet drop matrix.
4. Audit NPCs that are placed in shipped maps before unused FGD classes: `npc_citizen`, `npc_combine_s`, turrets, `npc_zombie` family, `npc_antlionguard`, `npc_hunter`, `npc_strider`.
5. Run a Windows save/restore and map-transition test for every custom NPC datamap.
