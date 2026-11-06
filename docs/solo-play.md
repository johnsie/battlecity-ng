# Solo and small-group play: how it works

The plan and status are in `docs/solo-play-roadmap.md`. This file explains the behaviour and the code.

## What a player sees

While at least one human is in the game, the server keeps the world populated:

- **Raiders** (pink scouts, green-camouflage heavies) spawn out of sight, wander, notice a player within about 900-1,000 pixels, chase, aim and shoot. Scouts use lasers, heavies rockets. A badly hurt bot runs away for a few seconds.
- **Snowmen** are neutral wandering drones. Shoot one for a point.
- **Safe zones**: bots stay about 1,500 pixels (30 tiles) away from any city that has a mayor, never fight there and never block a building plot. Only a raid (below) comes in.
- **Raids**: every 20-30 minutes a raiding party of 2-4 attacks a city that has buildings. The whole server is told, with a compass direction.
- **Missions** (about every 8-13 minutes, one at a time, shared by everyone): *Raider Sweep* (destroy N raiders in five minutes), *Boss Hunt* (destroy the Warlord in eight) and *Hold the Zone* (stay near a marked tile for 75 seconds while raiders attack). Everyone who took part gets 5 points.
- **Bounty Warlord**: a 150 HP boss with plasma, worth 10 points to the killer and 6 to each other player who hit it, plus a supply drop where it fell.
- **Supply crates**: a medkit, cloak or rocket appears on the map for one human's city (only that city can pick it up) with a hint such as "24 tiles north-west of you". It disappears after six minutes.
- **Challenges**: `/challenge` shows a daily goal (8, 12 or 16 raider kills depending on the day, +8 points) and a weekly goal (60 kills, +40). Kills pay 1 point (scout), 3 (heavy), 10 (boss) or 1 (snowman), capped at 60 points a day from bots. Promotions use the existing rank system.
- **Practice**: `/range` places a ring of target dummies around you and times how long you take to clear them; `/trial` sets four checkpoints and times the run. Personal bests are kept (`/trial best`).
- **A ghost** tank replays the longest recent run of the best player who left, and the login news lists recent events (raids, boss kills, records, the last player online).
- **Weather** (client only): rain and dust storms roll through on a five-minute schedule. `BC_WEATHER=rain|dust|off` forces or disables it.

Commands (typed in game chat, answered by the server and never broadcast): `/help`, `/bots`, `/challenge`, `/mission`, `/range`, `/trial` (`stop`, `best`), `/ghost`, `/events`.

## How it works

All of it is in `server/CBot.cpp` (`CBotManager`), driven from the main loop (`Server.Bots->Cycle()`).

- A bot occupies an ordinary player slot, taken from the top of the table (63, 62, ...) so real players, who take the lowest free slot, are not displaced. `CPlayer::IsBot` marks it. It is in the game state in a reserved city (`BC_BOT_CITY`, default 63) whose "mayor" is the Overseer, so humans can neither join nor start it. Bots have no socket: `CSocket::QueueSend` and the receive loop skip them.
- Clients are told with the same packets a human generates: `smPlayerData`, `smPointsUpdate`, `smJoinData`, then `smUpdate` every 120 ms (to players within radar range), `smShoot`, `smDeath`, `smRespawn`, `smClearPlayer`. Nothing in the client had to change, and old clients work.
- Movement follows the client model exactly (32 directions, one turn step per 50 ms, `sin/cos` with 3.14 for pi) so clients' dead reckoning stays in step. Terrain and buildings are tested with the client's own collision rules.
- Health is tracked on the server for bots only: `CBulletList::cycle` hands a bullet that hits a bot to `CBotManager::BulletHitBot`. Bots do not hurt each other. Humans' own clients still work out their own damage and send `cmDeath`, as before.
- Rewards go through `CAccount::AddPoints`. Bots are never saved to or loaded from the database, never counted in the public player count (`automated`), and never receive points (`ProcessDeath` skips them).
- The **Overseer** is a bot that is permanently "dead" at (0,0), so it is invisible. It speaks for the server through the global-chat packet (`Overseer (Global): ...`).
- With nobody in the game everything is removed and the timers reset, so an empty server does no work. `CServer::reset()` ignores bots when deciding whether the world may be rebuilt.
- State that must survive a restart lives in `bots_state.txt` (challenge progress, personal bests, recent events, last player online) and `ghost.dat` next to `map.dat`.
- The Discord hook (`BC_DISCORD_WEBHOOK`, https only) runs `curl` through `fork`/`exec` (never a shell) when the first player comes online, at most once every 30 minutes, with a name reduced to letters, digits, spaces, `_` and `-`. It does nothing on Windows.

## Settings

| Variable | Default | Meaning |
|----------|---------|---------|
| `BC_BOTS` | 3 | maximum fighting bots (1 to 12); `0` turns every feature off |
| `BC_BOT_CITY` | 63 | city index the bots belong to |
| `BC_DISCORD_WEBHOOK` | unset | https webhook for the "someone is online" message |
| `BC_BOTS_SAFE_RADIUS` | 1500 | pixels bots keep from a city that has a mayor; `0` removes the safe zone (tests) |
| `BC_BOTS_TEMPO` | 1 | test hook: scales the gaps between events (0.02 makes them seconds) |
| `BC_WEATHER` | unset | client: `rain`, `dust` or `off` |

How many fighters spawn depends on how many humans are online: all of `BC_BOTS` for one player, three quarters for two, half for three, a quarter for four or five, none for six or more.

## Tests

`ci/bots-test.sh <bc-server> <map.dat>` runs three headless cases at the same time, each against its own server on its own port, and each case moves on as soon as what it waits for has happened (about 25 seconds in all). Use the sanitizer build:

- `tools/bots_test.py`: bots are announced and move, chat commands are answered, a range can be cleared by shooting, a snowman pays a point, a trial starts, the public count excludes bots.
- `tools/bots_events_test.py` (with `BC_BOTS_TEMPO=0.02`): raid, mission, bounty and crate announcements, a crate is picked up, bots shoot, a ghost appears for the next player, the login news lists events.
- `tools/bots_off_test.py` (with `BC_BOTS=0`): nothing appears.

## Known limits

- Live updates (a new or destroyed building, say) reach the 3x3 sectors around a player; bots may stand in the outer sectors of a wide view and be seen before such updates arrive.
- A raid can only destroy buildings with no population; the server's bullet code lets populated buildings absorb shots, as it always has.
- Bounties and crates are announced in chat with a compass hint, not drawn on the radar (that needs a protocol change).
- The ghost replays one recorded run rather than live play.
- Numbers (health, damage, intervals, rewards) are first guesses.
