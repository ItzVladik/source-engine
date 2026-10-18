# Underhell port onto this engine (TF2 2018 / nillerusr source-engine)

Source: [Underhell-RE/source-sdk-2007](https://github.com/Underhell-RE/source-sdk-2007), full delta from first commit `d789ed3` (vanilla SDK 2007) to `905d675`.

That tree is Orange Box SDK 2007 (vcproj, episodic). This tree is a later engine (waf, TF2-era shared code). A raw patch of the 87 modified vanilla files does not apply. Port is **additive**:

## Landed

- All new Underhell sources:
  - `game/client/underhell/`
  - `game/server/underhell/`
  - `game/shared/underhell/`
- Dev docs from the SDK repo under `docs/`
- VPC registration for **hl2** and **episodic** client/server
- Player inventory / endurance / gear members + send/recv tables on `CHL2_Player` / `C_BaseHLPlayer`
- Activities: `ACT_WALK_UNARMED`, raised VM poses, silenced recoils, `ACT_KICK*`
- `MAX_VIEWMODELS` 2 → 3 (kick VM)
- Weapon script extras (`OneHanded`, `ExpOffset`, `UH_Weapon_Special`, `single_shot_silenced`)
- Viewmodel `m_bExpSighted` / `m_expFactor`
- Player hooks: spawn VMs, datadesc/inputs, ClientCommand inventory/objectives, impulse 101/110, bleed, endurance recharge, flashlight flags

## Second pass

- Custom cameras / scope RTs (`baseclientrendertargets`, `rendertexture`, `point_camera`)
- Monitor + scope draw, reflective-glass gate (`viewrender`, `g_bRenderingCameraView` in release)
- Mirror-only + glow on `CBaseEntity` / `C_BaseEntity` (`uh_rendermirrorsonly`, Glow inputs)
- OTS/free-aim cursor (`in_mouse` + `uh_freeaim.h`), local player shadows / render angles / flashlight origin
- Hidden-save proxy (`uh_hidden_save_proxy`, `uh_load_hidden_save`)

## Third pass

- Vehicle gunner seat (`m_bPlayerAtGun`, ToggleGunMode, gunner eyes)
- NPC Underhell AI/dismemberment members + datadesc + think/activate/precache
- SOUND_FMRADIO / COND_HEAR_FMRADIO
- Weapon walk-over ammo scavenge, silencer activities
- Bullet-time FireBullets intercept
- View bob, OTS camera, HUD zoom/stamina swap
- Batteries / healthkits into inventory
- Mirror-only DrawModel on C_BaseAnimating

## Fourth pass

- Jeep mounted gun: `EnableMountedGun`, muzzle attachment, particle muzzle, yaw 200, gunner-only aim/fire
- Ragdoll Use/pickup, FCAP_IMPULSE_USE, UH dismember transfer + `UH_RagdollDismember`
- Combine: SOUND_FMRADIO investigation, soldier shield spawn/drop, pain sentences, gib precache
- Flashlight flicker convars
- Flare glowstick netvars + client colours

## Fifth pass (from klaxons1/source-sdk-2013-ce)

- `env_message` priority slots, titles `@` refs, GlobalEnvMessageIndex
- `env_hudhint` InputHint / InputHintThroughParameter
- `prop_dynamic` OnPlayerUse + impulse use; flare pack pickup; blackout camera; `UHBreachDoor`
- `Throw_Nade` via `WeaponFrag_ThrowNow`
- `env_global` SetGlobalOn/Off/Dead by parameter
- Item output name `OnPlayerPickup`; UH blood decal materials kept resident

## Still not 1:1 vs SDK 2007 vanilla hunks

- BIK / sprite path
- Full flashlight/left-arm auto-switch path
- Shader Editor GL binary interface
- Ace / Butcher / infected vanilla hunks (NPCs live in `underhell/npc_*.cpp`)

Vanilla HL2/episodic should still build; Underhell maps need the episodic game (`-G episodic` / `hl2` with these VPCs).
