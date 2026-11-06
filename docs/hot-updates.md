# Hot updates (browser client)

When a new release is published, players who have the game open get it without logging in again or losing their place.

1. Every web release publishes a `VERSION` file beside the page. The page (`web/shell.html`) reads it when it loads and again every minute (and when the tab becomes visible). If it has changed, it sets `window.bcUpdatePending`.
2. The game (`HotUpdateTick` in `client/platform/main_sdl.cpp`) then waits for a quiet moment: in the game, alive, standing still for three seconds, not chatting, building or in a dialog. It tells the player ("A new version is ready...") and then:
   - sends `cmResumeLater` to the server,
   - stores the account name and password for the next page (`sessionStorage`, this tab only, read once),
   - reloads.
3. The server (`CPlayer::StartLinger`, `CProcess::ResumeSession`) keeps the player's seat in the game when the connection drops within 20 seconds of `cmResumeLater`. The player stays where they were, for up to 60 seconds (`BC_LINGER_SECONDS`), so a mayor's city is not torn down by the reload.
4. The new page logs in by itself (`AutoLoginTick`). The server sees the same account, hands back the seat, and sends the client straight into the game (no meeting room), at the position it had. Total interruption: the few seconds a reload takes.

If anything fails (no stored login, the seat expired, the server restarted) the player lands on the normal login screen. A connection that drops without the announcement behaves as before: the player leaves at once.

The desktop clients do not check for updates (the server's version check still forces them to update). `BC_TEST_UPDATE_AT=<n>` makes a desktop client behave as if an update arrived after n frames in the game, and `BC_RESUME_USER` / `BC_RESUME_PASS` log it in by itself, which is how the path is tested.

Test: `ci/resume-test.sh <bc-server> <map.dat>` (`tools/resume_test.py`).
