# Waypoints (pointers to important places)

The map is big and nothing in the game shows where things are, so the game points to the important local things. Anything out of view gets an arrow with a label and its distance in tiles at the edge of the screen (for example `HOME 62`); anything in view gets a small label above it. Press **N** to turn the pointers on and off (the choice is remembered).

| Label | What it points to | Shown to |
|-------|-------------------|----------|
| HOME | your Command Center (only when it is out of view) | everyone in the game |
| CRATE | a supply crate your city can pick up | the city it is for |
| DROP | the latest supply crate of another city (your city cannot pick it up, but it shows where the action is) | everyone else |
| RAID | the nearest raider attacking your city | the city being raided |
| BOSS | the bounty Warlord | everyone |
| ZONE | the "Hold the Zone" mission area | everyone |
| GATE | your next time-trial gate | the player on the trial |
| FIGHT | where the latest fight was (a tank died, or a raider or boss was hit), for 45 seconds, unless you are already there | everyone |

Only these few things are shown (at most eight), so the screen stays clear. Pointers keep out of the way of the info lines and the voice button, and never sit on top of one another.

## How it works
- **Server** (`server/CWaypoints.cpp`): once a second it sends each player in the game `smWaypoints`: a count, then per place a type and its x and y in map pixels (5 bytes each). Home, fights and nothing else work even with `BC_BOTS=0`; the objectives come from the bot system (`CBotManager::Objectives`). A fight is noted when a player dies (`CProcess::ProcessDeath`) or a raider or boss is hit (`CBotManager::BulletHitBot`).
- **Client** (`CDrawing::DrawWaypoints`): works out where each place is on screen (the view is mirrored: a larger world X is to the left, a larger Y is up), clamps out-of-view ones to the edge along the line from the middle of the screen, and draws a label and an arrow. Pointers are forgotten if no update arrives for five seconds.
- The packet was appended to the end of the server message list, so old clients ignore it.

## Test
`ci/waypoints-test.sh <bc-server> <map.dat>` (`tools/waypoints_test.py`): a player is told where their Command Center is, and when another player dies a far-away player is told where. `BC_TEST_SCENE=1 BC_TEST_WAYPOINTS=1` shows made-up pointers in the offline test scene.
