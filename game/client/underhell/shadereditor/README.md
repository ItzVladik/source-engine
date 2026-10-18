# Underhell Shader Editor bridge

This directory contains a minimal runtime bridge to the Source Shader Editor used by the original Underhell client.

- Interface ABI: `ShaderEditor005`
- Runtime DLL: `bin/shadereditor_2007.dll`
- Procedural shader DLL loaded by it: `bin/game_shader_generic_eshader_2007.dll`
- Upstream source: https://github.com/Biohazard90/source-shader-editor

Only the stable external interface and game-system lifecycle are kept here. The editor UI and model-preview implementation remain external, which keeps the client integration small and makes a future Source SDK 2013 port a matter of selecting the 2013 DLL/ABI adapter.

The interface declaration is derived from the upstream Source Shader Editor project and remains under the Source SDK license supplied with that project.
