# Solo and small-group play: roadmap

Problem: most of the time there are no other players, so the world feels empty. This roadmap lists the ideas for making a quiet server fun, and what each one needs. Status is kept here; the design notes are in `docs/solo-play.md`.

Every feature runs in the server (new module `server/CBot.cpp`) unless a client change is listed. Bots occupy ordinary player slots (flagged `IsBot`, never counted in the public player count, never saved to the database) so unmodified clients can see and fight them.

| # | Idea | What it is | Where | Status |
|---|------|-----------|-------|--------|
| 1 | AI bot tanks | Scouts (laser, fast), heavies (rockets, slow, tough) and a boss. They wander, notice you, chase, aim, shoot and flee when hurt. Killable (server-side health), respawning. Count scales with how many humans are online and drops to zero when nobody is there. | server | done |
| 2 | Raids | Every 10-15 minutes a raid party attacks a human city's buildings. The city is warned in global chat; repelling it earns points. | server | done |
| 3 | Solo goals and progression | Daily and weekly challenges (kill counts) with point rewards, a daily cap on points from bots, promotions through the existing rank system. `/challenge` shows progress. | server | done |
| 4 | Things to find | Supply crates (medkit, cloak, rocket) dropped somewhere on the map with a hint of where, and a roaming bounty boss worth extra points and a drop. | server | done |
| 5 | Targets and practice | `/range` builds a ring of target dummies and times how long you take to clear them. `/trial` is a four-checkpoint time trial with personal bests. | server | done |
| 6 | Ambient life | Neutral wandering "snowman" drones to shoot for a point; weather in the client (rain, dust storms) that comes and goes. | server and client | done |
| 7 | A world that feels lived in | Recent events (raids, boss kills, records, last player online) are shown in the news on the login screen; a "ghost" tank replays the best recent run. | server | done |
| 8 | Community hooks | When the first player comes online, a Discord webhook (`BC_DISCORD_WEBHOOK`) posts a short message so others can join. Rate limited, off unless configured. | server | done |
| 9 | Co-op against the environment | Server-wide missions: Raider Sweep (destroy N raiders in time), Boss Hunt, and Hold the Zone (a capture point defended against waves). Everyone who took part shares the reward. | server | done |

## Rules the bots follow

- Bots exist only while at least one human is in the game. With nobody online the bots are removed, so an empty server costs nothing.
- Settings (environment variables, all optional): `BC_BOTS` (maximum fighting bots, default 4, `0` turns every bot feature off), `BC_BOT_CITY` (the city index bots belong to, default 63), `BC_DISCORD_WEBHOOK`.
- Bots never receive or lose points in the database. Points awarded to humans for bot kills are small and capped per day.
- Bots shoot with the same bullets and damage as players, so the existing client damage and death code works unchanged.
- State that must survive a restart (challenge progress, trial records, the ghost, recent events) lives in small text files in the server's working directory next to `map.dat`.

## Not done / follow-ups

- Capture the flag with bots was folded into the "Hold the Zone" mission; a flag that is carried across the map would need new client drawing.
- Radar markers for bounties and crates are announced in chat with a compass hint; drawing them on the radar needs a protocol change (and a forced client update).
- The ghost is a replay of one recorded run, not of the live session.
- Tuning (counts, damage, intervals) has only been tried with the headless bot tests, not with people.
