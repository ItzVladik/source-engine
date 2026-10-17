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

## Second pass (this engine)

- Custom cameras / scope RTs (`baseclientrendertargets`, `rendertexture`, `point_camera`)
- Monitor + scope draw, reflective-glass gate (`viewrender`, `g_bRenderingCameraView` in release)
- Mirror-only + glow on `CBaseEntity` / `C_BaseEntity` (`uh_rendermirrorsonly`, Glow inputs)
- OTS/free-aim cursor (`in_mouse` + `uh_freeaim.h`), local player shadows / render angles / flashlight origin
- Hidden-save proxy (`uh_hidden_save_proxy`, `uh_load_hidden_save`)

## Still not 1:1 vs SDK 2007 vanilla hunks

These remain in the SDK repo if you need a third pass:

- Combine / Ace / Butcher / infected AI edits in vanilla `npc_combine*` (new NPCs are in `underhell/npc_*.cpp`)
- Jeep gunner / bullet-time tracers in `vehicle_jeep*`
- Dismemberment / ragdoll extras in `physics_prop_ragdoll`, `props.cpp`
- `EnvMessage` / `EnvHudHint` (paths are `EnvMessage.cpp` here)
- Full flashlight/left-arm auto-switch path (flags are wired; original deny/switch logic is only partially applied)
- Shader Editor runtime is compiled; engine GL path may still need the original binary interface

Vanilla HL2/episodic should still build; Underhell maps need the episodic game (`-G episodic` / `hl2` with these VPCs).
