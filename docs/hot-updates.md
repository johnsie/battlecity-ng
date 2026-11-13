# Hot updates (browser client)

When a new release is published, players who have the game open get it without logging in again or losing their place.

1. Every web release publishes a `VERSION` file beside the page. The page (`web/shell.html`) reads it when it loads and again every minute (and when the tab becomes visible). If it has changed, it sets `window.bcUpdatePending`.
2. The game (`HotUpdateTick` in `client/platform/main_sdl.cpp`) then waits for a quiet moment: in the game, alive, standing still for three seconds, not chatting, building or in a dialog. It tells the player ("A new version is ready...") and then:
   - sends `cmResumeLater` to the server,
   - stores the account name and password for the next page (`sessionStorage`, this tab only, read once),
   - reloads.
3. The server (`CPlayer::StartLinger`, `CProcess::ResumeSession`) keeps the player's seat in the game when the connection drops within 20 seconds of `cmResumeLater`. The player stays where they were, for up to 60 seconds (`BC_LINGER_SECONDS`), so a mayor's city is not torn down by the reload.
4. The new page logs in by itself (`AutoLoginTick`). The server sees the same account, hands back the seat, and sends the client straight into the game (no meeting room), at the position it had. Total interruption: the few seconds a reload takes.

## When the server itself is updated

A new server means a restart, which used to end the game for everybody (the world lived in memory only). Now:

- When the server is stopped (SIGTERM) it writes `dat/world.state` (`CServer::SaveWorld`, server/CWorldState.cpp): the cities of the mayors who are in the game, their buildings and items, and who is where (account, city, mayor or not, position, what the tank carries). The file is used once, and ignored when it is older than 15 minutes.
- The new server reads it at start (`LoadWorld`). The world is not rebuilt for "nobody is connected" for the next 2.5 minutes, and a restored city waits that long for its mayor before it is destroyed.
- When the client sees the connection drop (web or desktop) it no longer shows a message: it stores the login as for a hot update, reloads and logs in by itself (`BCAutoResume`, client/platform/main_sdl.cpp). If the server is not back yet the splash screen retries for about a minute. The server sees the account in the saved world and sends the client straight into the game (`CProcess::RestoreSeat`): same city and position, mayors are mayors again, carried items are back. If a second drop happens within 40 seconds, the player gets the old message instead, so a broken connection cannot loop.
- Things that are not kept: bullets in flight, bots and their events (they start again), the health of tanks.

Test: `ci/restart-test.sh <bc-server> <map.dat>` (`tools/restart_test.py`).

Outiva sign-ins have no password to store: the reload stores the marker `@outiva` instead, and the new page logs in with a fresh one-time ticket (`AutoLoginTick` waits for `window.bcTicket`, which the page renews from the stored Outiva sign-in, for up to 20 seconds). Guests have nothing to keep and log in again by hand.

If anything fails (no stored login, the seat expired, the server restarted) the player lands on the normal login screen. A connection that drops without the announcement behaves as before: the player leaves at once.

The desktop clients do not check for updates (the server's version check still forces them to update). `BC_TEST_UPDATE_AT=<n>` makes a desktop client behave as if an update arrived after n frames in the game, and `BC_RESUME_USER` / `BC_RESUME_PASS` log it in by itself, which is how the path is tested.

Test: `ci/resume-test.sh <bc-server> <map.dat>` (`tools/resume_test.py`).
