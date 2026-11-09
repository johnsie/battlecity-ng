# Battle City modern presentation roadmap

## 1. Product goal

Turn Battle City into a premium-feeling, stylised 2.5D online game without
discarding the persistent shared world, city simulation, solo systems, browser
client, or the recognisable right-hand command HUD.

"Premium" means that the game is immediately readable, responsive and
cohesive. It does not mean photorealism. The target is a strong art direction,
excellent combat feedback, a living world, polished sound and motion, and a
first session that teaches the player why founding and defending a city matters.

### Non-negotiable constraints

- Keep the authoritative C++ server and its gameplay rules.
- Keep native Windows and Linux clients and a browser client.
- Keep the right-hand command HUD as a recognisable part of Battle City. It may
  reflow at small sizes and gain optional compact modes, but it is not replaced
  by a generic floating HUD.
- Keep the existing SDL client releasable until a replacement reaches feature
  parity. Server protocol changes continue to force a coordinated release.
- Preserve support for the original assets as a fallback and for historical
  presentation modes.
- Never migrate accounts, persistence or live world state merely to improve the
  renderer.

## 2. Target experience

The visual target is an orthographic or lightly tilted 2.5D battlefield with
stylised tanks and architecture. Shapes and faction colours must remain readable
at gameplay scale. Materials, shadows and effects share a consistent light
direction. The world is detailed through motion and state rather than visual
noise: factories work, hospitals respond, turrets track, structures construct
and decay, and vehicle tracks and damage remain briefly in the environment.

The command HUD remains on the right. Its radar, health, inventory, city
controls, messages and leaderboard are reorganised into a coherent command
console with crisp scalable type, consistent spacing, animated state changes and
contextual detail. The familiar silhouette and functions stay intact.

## 3. Technical direction

### 3.1 Recommended client architecture

Build a Godot 4 client alongside the SDL client. Use the Compatibility renderer
and GDScript for the first production slice so native and WebGL 2 exports share
the same project. Keep all game authority on the existing server.

The modern client is split into these boundaries:

1. **Transport**: TCP on native builds and WebSocket in the browser.
2. **Protocol**: typed encode/decode functions generated from one schema.
3. **World model**: a presentation-safe mirror of authoritative server state.
4. **Presentation**: scenes, materials, animation, camera, particles and audio.
5. **Interface**: the retained command HUD, menus, chat and accessibility.

Godot scene and resource files should use text formats wherever possible. Source
art lives outside imported caches; `.godot/` and generated import artefacts are
not committed.

### 3.2 Why this is parallel rather than an immediate rewrite

- The current SDL client remains the reference for protocol behaviour.
- Each modern presentation system can be compared against a deterministic offline scene.
- Native and browser performance are measured from the first vertical slice.
- A failed experiment cannot block server releases or strand players.
- The new client may ship as a preview before becoming the default.

### 3.3 Protocol work

- Define packet identifiers, field types, signedness, lengths and direction in a
  machine-readable schema.
- Generate C++ declarations/serialisers and GDScript encode/decode code.
- Add golden-packet tests shared by server, SDL client and modern client.
- Preserve current byte layout during the first client migration.
- In a later coordinated protocol version, add sequence numbers, explicit byte
  order, capability negotiation and length framing where useful.
- Reject invalid lengths and ranges at the protocol boundary before data reaches
  gameplay code.

## 4. Production principles

- **Readability before spectacle.** Effects must never hide tanks, projectiles,
  pickups or mission geometry.
- **One source of truth.** Gameplay numbers remain on the server; presentation
  may predict or interpolate but not decide outcomes.
- **State drives presentation.** Damage, ownership, danger, production and
  objectives must be visible in the world, not only written in chat.
- **Layered quality.** Browser and low-end settings reduce particles, shadows and
  persistent decals without changing gameplay.
- **Accessible by construction.** Do not use colour alone; support scalable UI,
  reduced motion, screen-shake strength, high contrast and remapping.
- **Measure every milestone.** Use capture scenes, frame-time budgets, usability
  tasks and network simulations rather than subjective sign-off alone.

## 5. Workstreams

### A. Foundations and vertical slice

Deliver a standalone three-minute scenario: drive from a base, accept a mission,
fight one scout and one heavy, collect a supply crate, defend a building and see
the result in the command HUD.

- [ ] Create the parallel Godot project with native and web export presets.
- [ ] Reproduce the existing offline renderer scene deterministically.
- [ ] Connect to a development server without changing production defaults.
- [ ] Decode login, player, movement, bullet, item, building and chat packets.
- [ ] Implement interpolation, prediction boundaries and disconnect handling.
- [ ] Establish orthographic camera, world scale and coordinate conversion.
- [ ] Build one terrain biome, one player tank, two enemies, three buildings,
      four pickups and one destruction set to final target quality.
- [ ] Recreate the right-hand command HUD with radar, health, inventory, chat,
      buttons, contextual details and leaderboard.
- [ ] Export and smoke-test Windows, Linux and browser builds.
- [ ] Record frame time, memory, download size and reconnect results in CI.

Exit criteria:

- The slice is visibly superior to the SDL capture at the same gameplay scale.
- Input-to-motion and input-to-muzzle-flash latency are at most one rendered
  frame in a local session.
- Native builds hold 60 FPS at 1080p on the agreed minimum machine; the browser
  build holds 60 FPS at its quality target or degrades cleanly to 30 FPS.
- No visual effect changes server-authoritative collision or damage.
- All essential HUD functions are present and usable with mouse and keyboard.

### B. Art direction and asset pipeline

- [ ] Produce a visual bible: palette, faction colours, value ranges, silhouette
      rules, camera, light direction, material response and UI motion language.
- [ ] Define Blender scale, origin, naming, collision proxy and export rules.
- [ ] Define texture channel packing and desktop/web compression settings.
- [ ] Replace BMP colour-key transparency with alpha textures and documented
      colour space.
- [ ] Create automated asset validation for dimensions, pivots, animation names,
      missing materials and excessive texture sizes.
- [ ] Build modular terrain with variation masks, transitions, landmarks and
      biome-specific decoration; eliminate obvious tiling.
- [ ] Build tank families with faction variants, turret articulation, recoil,
      suspension response, tread motion and damage stages.
- [ ] Build every structure with construction, operating, damaged, critical and
      destruction states.
- [ ] Build pickups and mission props with unmistakable silhouettes at gameplay
      scale.
- [ ] Retain original assets in a legacy pack; do not silently upscale them and
      call that final art.

Exit criteria: every normal gameplay object is recognisable in silhouette and
all visible assets obey the same camera and lighting rules.

### C. Vehicle handling and combat feel

- [ ] Separate visual suspension/body response from authoritative movement.
- [ ] Add acceleration cues, braking weight, turning lean, tread slip, dust and
      surface-dependent tracks without altering server speed limits.
- [ ] Add turret tracking and anticipation while preserving current aim rules.
- [ ] Add weapon-specific anticipation, muzzle flash, recoil, projectile trail,
      impact, debris, decal and tail effects.
- [ ] Add directional damage indicators and readable friendly/enemy fire.
- [ ] Add brief local-only impact emphasis; never pause network simulation.
- [ ] Tune camera impulse by distance and weapon size, with an accessibility
      slider and a zero-shake option.
- [ ] Add staged tank damage: sparks, smoke, fire, degraded audio and destruction.
- [ ] Improve death and respawn presentation while keeping the current timer.
- [ ] Add controller response curves, vibration and full input remapping.

Exit criteria: blind play tests can distinguish primary weapon, rocket, plasma,
turret hit and vehicle destruction by sound and motion alone.

### D. Camera, lighting, VFX and weather

- [ ] Add camera smoothing, look-ahead, bounded combat zoom and objective framing.
- [ ] Support reduced motion and fixed-camera modes.
- [ ] Establish global time-of-day colour grading without harming visibility.
- [ ] Use consistent dynamic lights and normal/specular response for world assets.
- [ ] Add GPU particle pools with explicit per-quality budgets.
- [ ] Add persistent but capped tracks, scorch marks, rubble and shell casings.
- [ ] Make smoke react to wind and communicate damage direction.
- [ ] Rebuild rain and dust storms with foreground/background layers, surface
      response, audio and visibility-safe intensity.
- [ ] Add construction, healing, research, production, cloak, orb and mission
      effects with a common visual grammar.
- [ ] Add colour grading and exposure rules for low health, death and respawn;
      respect photosensitivity and high-contrast settings.

Exit criteria: effects remain legible with four players, a raid, turrets and
weather active, and stay inside the agreed GPU budget.

### E. Command HUD and menus

The right-hand HUD is retained. This workstream modernises it without erasing its
role or recognisable layout.

- [ ] Rebuild the panel with scalable vector/nine-slice pieces and theme tokens.
- [ ] Preserve radar at the top, primary status and inventory in the middle, and
      contextual information below; validate changes with existing players.
- [ ] Retain Staff, Map, Info, Points, Options, Help and Build functions.
- [ ] Add clear hover, focus, pressed, disabled, warning and notification states.
- [ ] Integrate objectives and city alerts without covering world action.
- [ ] Make chat readable, filterable and non-blocking; show voice activity and
      moderation state clearly.
- [ ] Provide expanded, standard and compact panel modes, with standard as the
      recognisable default.
- [ ] Support 16:9, 16:10, ultrawide and small browser canvases without clipping.
- [ ] Replace legacy modal flows with consistent panels while preserving every
      account, personnel, mayor, admin and recovery action.
- [ ] Add UI scale, high contrast, colour-blind-safe markers, subtitle/caption
      controls, keyboard navigation and screen-reader-friendly web shell labels.

Exit criteria: a new player can find health, inventory, map, current objective,
city state and build controls without instruction, while returning players still
identify it immediately as Battle City's command HUD.

### F. Audio and music

- [ ] Establish buses for UI, engines, weapons, impacts, ambience, weather,
      voice and music with separate controls.
- [ ] Build RPM/load engine layers and tread loops by terrain.
- [ ] Add weapon tails, distance filtering, obstruction approximations and
      variation sets to prevent repetition.
- [ ] Give pickups, mission changes, city warnings and HUD actions a consistent
      audio language.
- [ ] Build biome ambience and spatial activity for city buildings.
- [ ] Replace MIDI-only presentation with licensed adaptive stems while retaining
      a legacy music option if licensing permits.
- [ ] Add dynamic mix ducking for voice, critical warnings and major events.
- [ ] Add captions for gameplay-critical non-speech sounds.
- [ ] Measure loudness and peak levels across native and browser output.

Exit criteria: combat remains understandable with music disabled, and voice chat
remains intelligible during the busiest supported encounter.

### G. Living cities and world storytelling

- [ ] Visualise construction progress rather than spawning finished structures.
- [ ] Show factory production, hospital response, research activity, turret
      tracking, power/state changes and city income trends in-world.
- [ ] Add population traffic and non-colliding ambient activity within strict
      density budgets.
- [ ] Show ownership and faction identity through flags, lighting, paint and
      signage rather than labels alone.
- [ ] Add persistent damage stages and repair activity.
- [ ] Give command centres a clear defensive state and cinematic orb delivery.
- [ ] Add regional landmarks, readable routes and named locations to reduce the
      current map's visual repetition.
- [ ] Turn recent world events into environmental and HUD storytelling.

Exit criteria: players can infer whether a city is healthy, threatened,
productive or damaged by looking at it for five seconds.

### H. Onboarding, missions and progression presentation

- [ ] Replace the first login dead-end with a short guided arrival sequence.
- [ ] Teach movement, firing, pickups, repair, chat and the purpose of cities in
      playable steps.
- [ ] Give each solo activity a clear start, progress, success and failure flow.
- [ ] Present raids, boss hunts, hold zones, trials and challenges through the
      world and HUD rather than chat commands alone.
- [ ] Add pre-event warnings, participant state and post-event summaries.
- [ ] Present rank, streak and weekly score progress without manipulative reward
      patterns.
- [ ] Add contextual help that links to the retained Help function.
- [ ] Instrument onboarding completion and abandonment with privacy-safe counts.

Exit criteria: a first-time player reaches a meaningful combat-and-city success
without needing Discord, the wiki or slash-command documentation.

### I. Networking and multiplayer presentation

- [ ] Preserve server authority and validate all modern-client packets.
- [ ] Add timestamped snapshots and sequence handling in a coordinated protocol
      version after parity.
- [ ] Tune interpolation by measured jitter instead of one fixed blend window.
- [ ] Reconcile local movement without visible warps under expected latency.
- [ ] Add latency/loss simulation tests for movement, firing, pickups and deaths.
- [ ] Show connection degradation unobtrusively and explain reconnect state.
- [ ] Preserve hot web updates and resume behaviour.
- [ ] Add replayable packet captures with personal/chat data removed.

Exit criteria: two remote players see positions and combat outcomes consistently
at the supported latency/loss envelope, with no presentation-driven authority.

### J. Performance, compatibility and quality levels

- [ ] Define Low, Medium, High and Ultra profiles plus an automatic first-run
      recommendation.
- [ ] Budget CPU, GPU, draw calls, particles, visible lights, texture memory,
      audio voices and web download size per profile.
- [ ] Add object pooling and visibility/sector culling.
- [ ] Test Windows, Linux, Chromium, Firefox and Safari-compatible browser paths.
- [ ] Add deterministic screenshot scenes for clear, rain, dust, raid, city,
      inventory, build mode, death and each dialog.
- [ ] Add image-difference review artefacts without making antialiasing noise a
      release blocker.
- [ ] Capture frame-time percentiles; average FPS alone is insufficient.
- [ ] Add crash reporting that excludes credentials and chat by default.

Exit criteria: all target platforms pass functional smoke tests and published
minimum hardware meets its frame-time budget.

### K. Release and community transition

- [ ] Ship the modern client as the opt-in Modern Preview against staging first.
- [ ] Run protocol compatibility, account, reconnect and world-state tests.
- [ ] Publish side-by-side controls and known limitations.
- [ ] Gather structured feedback from existing players specifically on HUD
      recognition, combat clarity and visual identity.
- [ ] Run a public preview without retiring the SDL client.
- [ ] Make the modern browser client default only after parity and soak tests.
- [ ] Keep one rollback release and documented deployment procedure.
- [ ] Decide the long-term SDL client status only after adoption data exists.

## 6. Milestones

| Milestone | Scope | Release gate |
|---|---|---|
| P0: Baseline | Capture scenes, budgets, visual bible, protocol inventory | Repeatable SDL baseline in CI |
| P1: Connected slice | Workstream A vertical slice and retained HUD | Native and browser slice passes gates |
| P2: Combat alpha | Tanks, weapons, camera, VFX and combat audio | Full encounter readable under load |
| P3: City alpha | Terrain, buildings, construction and mayor tools | Found/build/defend loop reaches parity |
| P4: Feature parity | Accounts, social, missions, admin and all HUD flows | Automated parity checklist passes |
| P5: Preview | Staging and opt-in public builds | Soak, security and accessibility checks pass |
| P6: Modern default | New client is the default browser/native experience | Adoption and rollback gates pass |
| P7: Content finish | Full asset/audio replacement and world variety | Original art remains only by choice |

### Current status (2026-11-09)

- **P0 Baseline: in progress.** The existing SDL offline scene is the reference
  capture. The modern project now has its own deterministic 1600x900 capture.
- **P1 Connected slice: presentation foundation implemented; connection not yet
  implemented.** `premium-client/` contains the Compatibility-renderer project,
  mock battlefield, drivable and aimable tank, projectiles, camera response and
  the retained command HUD prototype.
- Godot 4.7.2 is pinned and the Linux/Windows/Web export presets are checked in.
- `ci/smoke-test-premium-client.sh` imports, runs and captures the project without
  using an account or game server.
- The classic browser client's sign-in menu now exposes **Modern Preview**. The
  preview has its own keyboard-navigable menu, a return route to the classic
  online client, reduced-motion and high-contrast HUD controls.
- GitLab and GitHub web jobs export the Modern Preview under `/modern/`, package
  it with the classic client, and require both routes during deployment checks.
- No server gameplay, account data or network protocol has been changed. The web
  deployment now gains only the additive `/modern/` preview route.

## 7. Immediate implementation backlog

These tasks are deliberately small enough to review independently:

1. **Done:** add this roadmap and link it from the project roadmap.
2. **Done:** add a versioned `premium-client/` Godot project using the
   Compatibility renderer, with generated/imported directories ignored.
3. **Done (foundation):** recreate the current test scene with placeholder
   geometry and a permanent right-hand command HUD.
4. **Done (foundation):** add an offline mock world so presentation work never
   needs a production login.
5. Inventory `NetMessages.h` into a protocol-schema draft and golden fixtures.
6. Implement native TCP and browser WebSocket connection interfaces.
7. Add CI syntax/import checks once the Godot version is pinned.
8. Create first-run capture artefacts for 1024x768 and 1600x900.
9. Produce and approve the visual bible before commissioning the full asset set.
10. Replace one tank, one projectile, one building and one terrain patch to prove
    the complete art-to-runtime pipeline.

## 8. Risks and decisions

| Risk | Mitigation |
|---|---|
| A visually impressive slice never reaches feature parity | Keep parity checklist and migrate vertical systems, not isolated screens |
| Browser rendering limits reduce quality | Design Compatibility-renderer quality first; scale effects, not gameplay |
| Two clients double maintenance temporarily | Freeze protocol churn during parity work and generate protocol code |
| Existing players reject a generic redesign | Retain the command HUD and validate visual direction with them early |
| Art production becomes the bottleneck | Approve a visual bible and representative asset set before bulk production |
| Effects damage combat readability | Enforce effect budgets, contrast rules and reduced-effects tests |
| New client accidentally reaches production | Require explicit development server configuration until preview release |
| Engine/editor files produce noisy diffs | Prefer text scenes/resources and exclude caches/import products |

## 9. Definition of done

The roadmap is complete when the modern client is the tested default on web,
Windows and Linux; every current player, mayor, social, solo and administration
flow is supported; the retained command HUD is recognisable and accessible; the
world uses a coherent final art and audio set; first-time players can understand
the game inside the game; and the original SDL client can be retired or retained
by an explicit, evidence-based decision rather than necessity.
