# Battle City Modern Preview client

This is the parallel Godot 4 Modern Preview client described in
[`docs/premium-roadmap.md`](../docs/premium-roadmap.md). It does not replace the
SDL client yet and must not be pointed at production during early development.

The first checked-in scene is deliberately self-contained: it provides a moving
tank, projectiles, enemies, buildings, pickups, terrain variation, camera
response and the retained right-hand command HUD without requiring an account or
server. It is an executable presentation test bed, not a second implementation
of game authority.

## Run

Open this directory in Godot 4 and run the project, or use:

```sh
godot --path premium-client
```

The pinned development version is in `.godot-version`. With that version
available, the deterministic import/runtime/capture check is:

```sh
ci/smoke-test-premium-client.sh /path/to/godot
```

Controls: `WASD` or arrow keys to drive, mouse to aim, left mouse or Space to
fire, `F1` to toggle the diagnostics line, and Escape to release/capture the
mouse.

The project uses the Compatibility renderer because browser export is a core
target. No imported cache files belong in version control.

## Current boundary

- `scripts/premium_client.gd`: mock simulation and presentation orchestration.
- `scripts/battlefield.gd`: deterministic world renderer and lightweight VFX.
- `scripts/command_hud.gd`: retained command HUD prototype.
- `scripts/presentation_camera.gd`: smoothed camera and bounded impact impulse.

Next is a protocol schema and a development-server transport. Until then the
scene must remain offline-only.
