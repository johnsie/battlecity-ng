# Outiva console integration plan

Battle City needs a small HTTP integration surface around the existing game server. Keep it separate from the game protocol and do not expose citizen names, cities or positions.

## 1. Decide the status source

- Prefer the existing web/gateway process if it already knows whether the native server is reachable.
- Otherwise add a loopback-only native health/count source and have the public web tier translate it. Do not expose a native status port directly to the internet.
- Define an active player as an authenticated/entered player, excluding half-open connections and bots unless the UI explicitly labels them.

## 2. Public status contract

- Serve `GET /outiva-status.json` as `{"status":"available","players":N}`.
- Use `degraded` when the website responds but the shared game world is unavailable, and HTTP 503 only when a current result cannot be produced.
- Send `Cache-Control: public, max-age=15` and `Access-Control-Allow-Origin: https://outiva.com`.
- Never include names, city membership, coordinates, chat, addresses or account identifiers.

## 3. Tests and deployment

- Unit-test count calculation and sanitisation; integration-test server up, server down and timeout paths.
- Add a production smoke check and rate-limit or cache the upstream native query so console traffic cannot load the game server.
- Document the endpoint and rollback steps in `docs/deploy.md`.

## 4. Catalogue work

- Capture a 16:9 WebP gameplay image showing a tank and developed city, targeting 100–180 KB.
- Confirm keyboard-and-mouse, online multiplayer and optional Outiva-account metadata.
- Remove “Windows and Linux downloads (coming)” from the Outiva facts only when release downloads actually exist; otherwise replace it with an honest current status.
- Set the production `statusUrl` in Outiva after the endpoint and CORS test pass.

## Acceptance

- Status requests cannot reveal or infer an individual player's identity or city.
- A failed status source does not interfere with gameplay.
- The Outiva card accurately distinguishes available, degraded and unknown states.
