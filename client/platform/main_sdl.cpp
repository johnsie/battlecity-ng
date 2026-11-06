/*
 * Battle City - SDL2 entry point (Linux, Windows, web).
 * Replaces the Win32 WinMain/WndProc: creates the window, translates SDL events into the
 * calls the game already understands, and runs the game loop.
 */
#ifdef _WIN32
#include <direct.h>
#endif
#include "CGame.h"
#include "platform/input_compat.h"
#include <SDL.h>
#include <string>

#ifdef __EMSCRIPTEN__
	#include <emscripten.h>
#endif

static CGame *g_game = NULL;
static SDL_Window *g_sdlWindow = NULL;
static int g_quitRequests = 0;
static bool g_quit = false;     // the application is closing (distinct from game.running: the engine can be stopped while dialogs stay usable)

// Finds the directory holding the game's images/sounds and makes it the working directory,
// because the game loads everything by relative name.
static bool FindAndEnterDataDir() {
	std::vector<std::string> candidates;
	if (const char *env = getenv("BC_DATA_DIR")) candidates.push_back(env);
	candidates.push_back(".");
	candidates.push_back("data");
	candidates.push_back("../data");
	candidates.push_back("../../data");
	if (char *base = SDL_GetBasePath()) {
		std::string b = base;
		SDL_free(base);
		candidates.push_back(b);
		candidates.push_back(b + "data");
		candidates.push_back(b + "../data");
		candidates.push_back(b + "../share/battlecity");
	}
	for (const std::string &dir : candidates) {
		std::string probe = dir + "/imgArrows.bmp";
		if (FILE *f = fopen(probe.c_str(), "rb")) {
			fclose(f);
#ifdef _WIN32
			_chdir(dir.c_str());
#else
			if (chdir(dir.c_str()) != 0) continue;
#endif
			return true;
		}
	}
	return false;
}

// ---- Test script runner: BC_TEST_SCRIPT=<file> drives the dialogs/keys headlessly (used for automated checks) ----
//   wait <frames> | text <controlId> <text> | click <controlId> | check <controlId> | key up|down|left|right on|off
//   | shot <file> | print <text> | quit
#include <fstream>
#include <sstream>
static std::vector<std::string> g_script;
static size_t g_scriptPos = 0;
static int g_scriptWait = 0;

static void RunScript() {
	if (g_scriptPos >= g_script.size()) return;
	if (g_scriptWait > 0) { g_scriptWait--; return; }
	while (g_scriptPos < g_script.size()) {
		std::istringstream line(g_script[g_scriptPos++]);
		std::string cmd;
		line >> cmd;
		if (cmd.empty() || cmd[0] == '#') continue;
		if (cmd == "wait") { line >> g_scriptWait; return; }
		HWND dlg = BCDialogsActiveHandle();
		if (cmd == "text") { int id; line >> id; std::string rest; std::getline(line, rest); if (!rest.empty()) rest.erase(0, 1); if (dlg) SetDlgItemText(dlg, id, rest.c_str()); else printf("script: no dialog for text %d\n", id); }
		else if (cmd == "click") { int id; line >> id; if (dlg) SendMessage(dlg, WM_COMMAND, MAKELONG(id, BN_CLICKED), 0); else printf("script: no dialog for click %d\n", id); }
		else if (cmd == "chat") {   // types a chat line (Enter, text, Enter), e.g. "chat /load demo.city"
			std::string rest; std::getline(line, rest); if (!rest.empty()) rest.erase(0, 1);
			g_game->handleKey(13);
			for (char ch : rest) g_game->handleKey((unsigned char)ch);
			g_game->handleKey(13);
		}
		else if (cmd == "msg") { std::string rest; std::getline(line, rest); if (!rest.empty()) rest.erase(0, 1); BCMessageBox(NULL, rest.c_str(), "BattleCity", 0); }
		else if (cmd == "ask") { std::string rest; std::getline(line, rest); if (!rest.empty()) rest.erase(0, 1); BCMessageBoxAsync(rest.c_str(), "BattleCity", MB_YESNO, [](int r) { printf("script: answer=%d\n", r); }); }
		else if (cmd == "enter") { SDL_Event ev; SDL_zero(ev); ev.type = SDL_KEYDOWN; ev.key.windowID = SDL_GetWindowID(g_sdlWindow); ev.key.keysym.sym = SDLK_RETURN; ev.key.keysym.scancode = SDL_SCANCODE_RETURN; ev.key.state = SDL_PRESSED; SDL_PushEvent(&ev); ev.type = SDL_KEYUP; ev.key.state = SDL_RELEASED; SDL_PushEvent(&ev); }
		else if (cmd == "select") { int id, index; line >> id >> index; if (dlg) SendDlgItemMessage(dlg, id, LB_SETCURSEL, index, 0); }
		else if (cmd == "dbl") { int id; line >> id; if (dlg) SendDlgItemMessage(dlg, id, WM_LBUTTONDBLCLK, 0, 0); }
		else if (cmd == "pos") {
			CPlayer *me = g_game->Player[g_game->Winsock->MyIndex];
			{
				int mx = (int)me->X / 48, my = (int)me->Y / 48;
				printf("script: collision=%d tile=%d,%d map[tile]=%d around: ", g_game->Collision->CheckPlayerCollision(g_game->Winsock->MyIndex), mx, my, (int)g_game->Map->map[mx][my]);
				for (int dy = -1; dy <= 2; dy++) { for (int dx = -1; dx <= 1; dx++) printf("%d ", (int)g_game->Map->map[mx + dx][my + dy]); printf("| "); }
				printf("\n");
			}
			printf("script: sectorLoaded=%d drawCalls/frame=%u fps=%d state=%d me=%d pos=%.0f,%.0f moving=%d turning=%d dead=%d frozen=%d dir=%d\n", (int)g_game->InGame->HasSector[(int)me->X / 48 / SectorSize][(int)me->Y / 48 / SectorSize], g_game->DDraw->LastFrameDrawCalls, g_game->Engine->FPS, g_game->State, g_game->Winsock->MyIndex, (float)me->X, (float)me->Y, (int)me->isMoving, (int)me->isTurning, (int)me->isDead, (int)me->isFrozen, (int)me->Direction);
		}
		else if (cmd == "check") { int id; line >> id; if (dlg) CheckDlgButton(dlg, id, 1); }
		else if (cmd == "key") {
			std::string which, state; line >> which >> state;
			int dik = which == "up" ? DIK_UP : which == "down" ? DIK_DOWN : which == "left" ? DIK_LEFT : DIK_RIGHT;
			BCTestSetKey(dik, state == "on");
		}
		else if (cmd == "shot") { std::string path; line >> path; g_game->DDraw->SaveScreenshot(path.c_str()); }
		else if (cmd == "print") { std::string rest; std::getline(line, rest); printf("script:%s\n", rest.c_str()); }
		else if (cmd == "restart") { BCRestartApplication(); }
		else if (cmd == "quit") { g_quit = true; g_quitRequests = 2; return; }
		else printf("script: unknown command '%s'\n", cmd.c_str());
	}
}

static void HandleEvent(const SDL_Event &e) {
	CGame &game = *g_game;
#ifdef BC_FRAME_TIMING
	if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) printf("mouse button %d type %d at %d,%d (dialog active: %d)\n", e.button.button, e.type, e.button.x, e.button.y, BCDialogsActive());
	else if (e.type == SDL_MOUSEMOTION) { static int n = 0; if (++n % 10 == 1) printf("mouse move %d,%d\n", e.motion.x, e.motion.y); }
	else if (e.type == SDL_WINDOWEVENT) printf("window event %d\n", e.window.event);
	else if (e.type == SDL_TEXTINPUT) printf("text input '%s' (dialog active: %d)\n", e.text.text, BCDialogsActive());
	else if (e.type == SDL_KEYDOWN) printf("key down sym=%d (dialog active: %d)\n", (int)e.key.keysym.sym, BCDialogsActive());
#endif
	BCDialogsProcessEvent(&e);
	bool dialog = BCDialogsActive();

	if (e.type == SDL_MOUSEBUTTONDOWN && !dialog) BCLatchMouseButton(e.button.button, e.button.x, e.button.y);

	switch (e.type) {
		case SDL_QUIT:
			g_quit = true;
			g_quitRequests++;
			game.running = 0;
			break;
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
				g_quit = true;
				g_quitRequests++;
				game.running = 0;
			} else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
				game.Input->HasFocus = 1;
				game.InGame->Minimized = 0;
			} else if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
				game.Input->HasFocus = 0;
			}
			break;
		case SDL_TEXTINPUT:
			// Printable characters (what WM_CHAR used to deliver)
			if (!dialog) {
				for (const char *c = e.text.text; *c; ++c) game.handleKey((unsigned char)*c);
			}
			break;
		case SDL_KEYDOWN:
			// Control keys that arrive as WM_CHAR on Windows
			if (!dialog) {
				switch (e.key.keysym.sym) {
					case SDLK_RETURN:
					case SDLK_KP_ENTER: game.handleKey(13); break;
					case SDLK_BACKSPACE: game.handleKey(8); break;
					case SDLK_ESCAPE: game.handleKey(27); break;
					default: break;
				}
			}
			break;
	}
}

// Quit once requested, but let a pending message ("you have been disconnected" ...) be read first;
// asking twice forces it.
static bool QuitNow() { return g_quit && (!BCMessagesPending() || g_quitRequests >= 2); }


// ---- Hot updates ----------------------------------------------------------------------------------------------------
// The browser page watches for a newer release (see web/shell.html). When one is ready the game waits for a quiet moment
// (standing still, not chatting or building, not dead), tells the server it is about to reload (cmResumeLater: the server
// keeps the player's seat in the game for a minute), remembers who is logged in, and reloads. The new page logs in by
// itself and the server puts the player straight back where they were, so an update costs a few seconds, not the session.
#include "resource.h"
#ifdef __EMSCRIPTEN__
EM_JS(int, bc_update_pending, (), { return window.bcUpdatePending ? 1 : 0; });
EM_JS(void, bc_store_resume, (const char *u, const char *p), {
	try { sessionStorage.setItem('bc.resume', JSON.stringify({ u: UTF8ToString(u), p: UTF8ToString(p) })); } catch (e) {}
});
#endif

static bool UpdatePending() {
#ifdef __EMSCRIPTEN__
	return bc_update_pending() != 0;
#else
	// Test hook for the desktop client: BC_TEST_UPDATE_AT=<n> makes an update "arrive" after n frames in the game
	static const char *at = getenv("BC_TEST_UPDATE_AT");
	static int ticks = 0;
	if (!at || g_game->State != STATE_GAME) return false;
	return ++ticks > atoi(at);
#endif
}

static void StoreResume(const char *user, const char *pass) {
#ifdef __EMSCRIPTEN__
	bc_store_resume(user, pass);
#else
	// the desktop client re-executes itself with the same environment
#ifdef _WIN32
	_putenv_s("BC_RESUME_USER", user);
	_putenv_s("BC_RESUME_PASS", pass);
#else
	setenv("BC_RESUME_USER", user, 1);
	setenv("BC_RESUME_PASS", pass, 1);
#endif
#endif
}

static void HotUpdateTick() {
	static Uint32 reloadAt = 0, idleSince = 0, lastTick = 0;
	static bool told = false;
	static float lastX = -1, lastY = -1, lastShot = -1;
	if (!UpdatePending()) return;
	CGame &g = *g_game;
	Uint32 now = SDL_GetTicks();
	if (reloadAt) {                                    // the announcement is on its way: give it a moment to leave
		if (now >= reloadAt) {
			g.Winsock->CloseWinsock();                 // (a re-executed desktop client would otherwise keep the old connection open)
			BCRestartApplication();
		}
		return;
	}
	if (g.State != STATE_GAME || g.Winsock->MyIndex < 0 || g.Winsock->MyIndex >= MAX_PLAYERS) return;
	if (now - lastTick < 100) return;
	lastTick = now;
	CPlayer *me = g.Player[g.Winsock->MyIndex];
	if (!told) {
		told = true;
		g.InGame->AppendInfo("A new version is ready. It installs the next time you stop for a moment, and you come back to the same spot.");
	}
	bool busy = me->isDead || me->isMoving || me->isTurning || g.InGame->IsChatting || g.InGame->ShowBuildMenu || g.InGame->IsBuilding != 0
	            || BCDialogsActive() || g.Voice->Transmitting() || me->X != lastX || me->Y != lastY || g.Input->LastShotValue() != lastShot;
	lastX = me->X; lastY = me->Y; lastShot = g.Input->LastShotValue();
	if (busy) { idleSince = 0; return; }
	if (!idleSince) idleSince = now;
	if (now - idleSince < 3000) return;                // standing still for three seconds
	g.Winsock->SendData(cmResumeLater, " ");
	StoreResume(g.Login->user, g.Login->pass);
	reloadAt = now + 500;
}

// After such a reload the new page has the account in its environment (BC_RESUME_USER/PASS): log in without the dialog
static void AutoLoginTick() {
	static int phase = 0;                              // 0 first try, 1 waiting for the answer, 2 second try, 3 finished
	static Uint32 mark = 0;
	if (phase >= 3) return;
	const char *u = getenv("BC_RESUME_USER"), *pw = getenv("BC_RESUME_PASS");
	if (!u || !*u || !pw || !*pw) { phase = 3; return; }
	CGame &g = *g_game;
	Uint32 now = SDL_GetTicks();
	HWND dlg = BCDialogsActiveHandle();
	if (phase == 0 || phase == 2) {
		if (g.State != STATE_LOGIN || !dlg) return;
		if (!mark) { mark = now; return; }
		if (now - mark < 1200) return;                 // let the connection and the version check finish
		SetDlgItemText(dlg, IDUSER, u);
		SetDlgItemText(dlg, IDPASS, pw);
		SendMessage(dlg, WM_COMMAND, MAKELONG(1, BN_CLICKED), 0);
		phase = (phase == 0) ? 1 : 3;
		mark = now;
		return;
	}
	if (g.State != STATE_LOGIN) { phase = 3; return; }  // in
	if (dlg) { phase = 3; return; }                     // the server refused: the dialog is back for the player
	if (now - mark > 5000) { g.Login->ShowLoginDlg(); phase = 2; mark = 0; }   // no answer: try once more
}

static void MainLoopIteration() {
	Uint32 t0 = SDL_GetTicks();
	SDL_Event e;
	while (SDL_PollEvent(&e)) HandleEvent(e);
	Uint32 t1 = SDL_GetTicks();
	RunScript();
	AutoLoginTick();
	HotUpdateTick();
	if (g_game->DDraw && g_game->DDraw->Renderer && !getenv("BC_FIXED_VIEW")) {
		int ow = 0, oh = 0;
		SDL_GetRendererOutputSize(g_game->DDraw->Renderer, &ow, &oh);
		g_game->FitViewToWindow(ow, oh);
	}
	if (g_game->running == 1) g_game->Engine->Frame();
	else g_game->Engine->IdleFrame();
#ifdef BC_FRAME_TIMING
	Uint32 t2 = SDL_GetTicks();
	static int frames = 0;
	if (t2 - t0 > 100 || ++frames % 120 == 0) printf("frame %d: events %ums, engine %ums\n", frames, t1 - t0, t2 - t1);
#endif
#ifdef __EMSCRIPTEN__
	if (QuitNow()) {
		emscripten_cancel_main_loop();
		EM_ASM({ if (window.bcEnded) window.bcEnded(); });
	}
#endif
}

int main(int argc, char *argv[]) {
	BCSetCommandLine(argc, argv);
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	if (!FindAndEnterDataDir()) {
		BCMessageBox(NULL, "Cannot find the game data (imgArrows.bmp ...). Run from the game directory or set BC_DATA_DIR.", "Battle City", 0);
		SDL_Quit();
		return 1;
	}

	g_game = new CGame();            // loads options.ini from the data directory
	CGame &game = *g_game;

	Uint32 flags = SDL_WINDOW_SHOWN;
	// Resizable + high-DPI: the game scales to the window, and the dialogs render at the real pixel size.
	// (In the browser the canvas buffer follows the canvas's CSS size.)
	flags |= SDL_WINDOW_ALLOW_HIGHDPI;
	if (game.Options->fullscreen == 0) flags |= SDL_WINDOW_RESIZABLE;
	int windowW = game.ResolutionX, windowH = game.ResolutionY;
	if (const char *size = getenv("BC_WINDOW_SIZE")) sscanf(size, "%dx%d", &windowW, &windowH);   // used by tests
	SDL_Window *window = SDL_CreateWindow("Battle City", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                                      windowW, windowH, flags);
	if (!window) {
		BCMessageBox(NULL, SDL_GetError(), "Battle City: cannot create window", 0);
		SDL_Quit();
		return 1;
	}
	g_sdlWindow = window;
	BCSetMouseReference(window, windowW, windowH);
	SDL_StartTextInput();

	game.Init((HWND)window, NULL);
	BCDialogsInit(window, game.DDraw->Renderer);

	if (const char *scriptPath = getenv("BC_TEST_SCRIPT")) {
		std::ifstream in(scriptPath);
		for (std::string l; std::getline(in, l); ) g_script.push_back(l);
	}
	// Optional: BC_SCREENSHOT=<file> BC_SCREENSHOT_FRAMES=<n> saves a frame and exits (used by automated tests)
	const char *shotPath = getenv("BC_SCREENSHOT");
	int shotFrames = getenv("BC_SCREENSHOT_FRAMES") ? atoi(getenv("BC_SCREENSHOT_FRAMES")) : 30;
	int frames = 0;
	// Optional: BC_TEST_DIALOG=<name> opens one dialog directly (used to screenshot each dialog)
	const char *testDialog = getenv("BC_TEST_DIALOG");

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(MainLoopIteration, 0, 1);
#else
	while (!QuitNow()) {
		MainLoopIteration();
		if (testDialog && frames == 10) {
			std::string n = testDialog;
			BCDialogsCloseActive();
			game.Dialog->StartDialog = 0;
			if (n == "login") game.Login->ShowLoginDlg();
			else if (n == "new") game.NewAccount->ShowNewDlg();
			else if (n == "edit") game.NewAccount->ShowEditDlg();
			else if (n == "personality") game.Personality->ShowPersonalityDlg();
			else if (n == "interview") game.Interview->ShowInterviewDlg();
			else if (n == "meeting") game.Meeting->ShowMeetingDlg();
			else if (n == "personnel") game.Personnel->ShowPersonnelDialog();
			else if (n == "options") game.Options->ShowOptionsDialog();
			else if (n == "voice") game.Voice->OpenSettings();
			else if (n == "help") game.Help->ShowHelpDialog();
			else if (n == "manage") game.Manage->ShowManageDlg();
			else if (n == "recover") game.Recover->ShowRecoverDlg();
			else if (n == "admin") game.Admin->ShowAdminDlg();
			else if (n == "adminedit") game.AdminEdit->ShowAdminEditDlg();
			else if (n == "connection") game.ConnectionManager->showConnectionManager();
		}
		// BC_TEST_SCENE=1: build an offline in-game scene (a tank near lava and rocks, buildings, items, blasts) to screenshot the renderer
		if (getenv("BC_TEST_SCENE") && frames == 5) {
			CGame &g = game;
			BCDialogsCloseActive();
			g.Dialog->StartDialog = 0;
			int mx = 256, my = 256, best = getenv("BC_TEST_DRIVE") ? 1000000 : -1;
			for (int y = 20; y < 490; y += 3) for (int x = 20; x < 490; x += 3) {
				int n = 0;
				for (int dy = -8; dy <= 8; dy++) for (int dx = -10; dx <= 10; dx++) n += (g.Map->map[x + dx][y + dy] == MAP_SQUARE_LAVA) * 2 + (g.Map->map[x + dx][y + dy] == MAP_SQUARE_ROCK);
				if (getenv("BC_TEST_DRIVE") ? (n < best) : (n > best)) { best = n; mx = x; my = y; }
			}
			g.Winsock->MyIndex = 0;
			CPlayer *me = g.Player[0];
			me->isInGame = 1; me->isDead = false; me->X = mx * 48 + 10; me->Y = my * 48 + 10; me->HP = 10; me->refHP1 = (me->HP ^ 2 - 31) * 2; me->refHP2 = -(me->HP); me->Direction = 4; me->Tank = 0; me->Member = 0; me->isCloaked = false; me->isMayor = 0; me->City = 0; me->NameString = "Tester"; me->TownString = "Testville";
			CPlayer *o = g.Player[1];
			o->isInGame = 1; o->isDead = false; o->X = me->X - 150; o->Y = me->Y + 60; o->Direction = 8; o->Tank = 0; o->Member = 0; o->isCloaked = false; o->isMayor = 0; o->NameString = "Rival"; o->TownString = "Elsewhere"; o->City = 1; o->HP = 40;
			memset(g.InGame->HasSector, 1, sizeof(g.InGame->HasSector));
			g.Build->newBuilding(mx - 4, my - 3, 0, 0, 1000 + 1);
			g.Build->newBuilding(mx + 3, my - 3, 100, 0, 1000 + 2);
			g.Build->newBuilding(mx - 4, my + 3, 300, 0, 1000 + 3);
			g.Build->newBuilding(mx - 14, my - 2, 100, 0, 1000 + 4);   // far to one side, to check the widescreen reach
			g.Build->newBuilding(mx + 14, my - 2, 100, 0, 1000 + 5);
			g.Build->newBuilding(mx - 17, my + 2, 100, 0, 1000 + 6);
			g.Build->newBuilding(mx + 12, my + 6, 100, 0, 1000 + 7);
			g.Item->newItem(mx + 2, my + 2, ITEM_TYPE_ORB, 0, 2000, 0);
			g.Item->newItem(mx + 3, my + 2, ITEM_TYPE_MEDKIT, 0, 2001, 0);
			g.Item->newItem(mx + 4, my + 2, ITEM_TYPE_BOMB, 0, 2002, 1);
			g.Item->newItem(mx - 1, my + 3, ITEM_TYPE_PLASMA, 0, 2003, 0);
			g.Explode->newExplosion((int)me->X - 160, (int)me->Y - 40, 2);
			g.Explode->newExplosion((int)me->X + 120, (int)me->Y + 20, 1);
			g.State = STATE_GAME;
			g.Draw->ApplyResolution(); g.Draw->resetPlayerOffset();
		}
		if (getenv("BC_TEST_SCENE") && frames > 5) { BCDialogsCloseActive(); game.State = STATE_GAME; if (getenv("BC_TEST_DRIVE")) { game.Player[0]->X -= 5.0f; game.Player[0]->Y += 2.0f; } }
		// BC_TEST_MAP=1: the mouse mapping must invert how the frame is scaled to the window
		if (getenv("BC_TEST_MAP") && frames == 20) {
			CGame &g = game;
			int ow = 0, oh = 0;
			SDL_GetRendererOutputSize(g.DDraw->Renderer, &ow, &oh);
			float fit = std::min((float)ow / g.ResolutionX, (float)oh / g.ResolutionY);
			bool integerFit = fit >= 1.0f && (fit - std::floor(fit)) / fit < 0.08f;
			float sc = integerFit ? std::floor(fit) : fit;
			int dw = (int)(g.ResolutionX * sc + 0.5f), dh = (int)(g.ResolutionY * sc + 0.5f), dx = (ow - dw) / 2, dy = (oh - dh) / 2;
			printf("script: window %dx%d game %dx%d scale %.3f integer=%d dst at %d,%d\n", ow, oh, g.ResolutionX, g.ResolutionY, sc, (int)integerFit, dx, dy);
			int pts[4][2] = { { 10, 10 }, { 300, 400 }, { 700, 100 }, { g.ResolutionX - 50, g.ResolutionY - 50 } };
			for (auto &pt : pts) {
				int wx = dx + (int)(pt[0] * sc + sc / 2), wy = dy + (int)(pt[1] * sc + sc / 2), gx, gy;
				g.DDraw->WindowToGame(wx, wy, &gx, &gy);
				printf("script:   game %d,%d is drawn at window %d,%d and maps back to %d,%d (error %d,%d)\n", pt[0], pt[1], wx, wy, gx, gy, gx - pt[0], gy - pt[1]);
			}
		}
		// BC_TEST_PLACE=1: put a building cursor at a fixed mouse position and a real building on the tile a click there would use;
		// they must coincide on screen
		if (getenv("BC_TEST_PLACE") && getenv("BC_TEST_SCENE") && frames == 20) {
			CGame &g = game;
			g.Player[0]->isMayor = 1;
			g.InGame->IsBuilding = 4;     // a house
			g.Input->LastMouseX = 300; g.Input->LastMouseY = 420;
			int tx, ty;
			g.Input->TileUnderMouse(g.Input->LastMouseX, g.Input->LastMouseY, &tx, &ty);
			g.Build->newBuilding(tx, ty, 4, 0, 5000);
			printf("script: click at %d,%d builds on tile %d,%d (tank tile %d,%d)\n", g.Input->LastMouseX, g.Input->LastMouseY, tx, ty, (int)g.Player[0]->X / 48, (int)g.Player[0]->Y / 48);
		}
		// BC_TEST_MENU=1: open the build menu in the scene and click each row, printing what each click selected
		if (getenv("BC_TEST_MENU") && getenv("BC_TEST_SCENE") && frames == 20) {
			CGame &g = game;
			g.Player[0]->isMayor = 1;
			g.InGame->CanBuild[1] = 1; g.InGame->CanBuild[3] = 1; g.InGame->CanBuild[4] = 1;
			int menuX = g.Draw->MaxMapX - 180, menuY = 344;
			for (int row = 0; row < 5; row++) {
				g.InGame->ShowBuildMenu = 1; g.InGame->IsBuilding = 0; g.Draw->BuildMenuX = menuX; g.Draw->BuildMenuY = menuY;
				int y = menuY - BUILD_MENU_ROW * row + BUILD_MENU_ROW / 2;   // Demolish is the bottom row (row 0), then the items upward; click the middle of each
				g.Input->TestClick(menuX + 40, y);
				printf("script: menu click row %d at y=%d -> IsBuilding=%d (menu shown=%d)\n", row, y, (int)g.InGame->IsBuilding, (int)g.InGame->ShowBuildMenu);
			}
		}
		if (getenv("BC_TEST_VOICE") && getenv("BC_TEST_SCENE") && frames == 25) {
			printf("voice self test: %s\n", game.Voice->SelfTest() ? "PASS" : "FAIL");
			game.Voice->serverEnabled = true; game.Voice->on = true;
		}
		// BC_TEST_MENU=3: pointer over the chat area
		if (getenv("BC_TEST_MENU") && getenv("BC_TEST_SCENE") && getenv("BC_TEST_MENU")[0] == '3') game.Input->MouseOverChat = 1;
		// BC_TEST_MENU=2: leave the menu open for a screenshot
		if (getenv("BC_TEST_MENU") && getenv("BC_TEST_SCENE") && getenv("BC_TEST_MENU")[0] == '2' && frames > 20) {
			game.InGame->ShowBuildMenu = 1; game.Draw->BuildMenuX = game.Draw->MaxMapX - 180; game.Draw->BuildMenuY = 344;
			game.Input->LastMouseX = game.Draw->MaxMapX - 120; game.Input->LastMouseY = 344 - BUILD_MENU_ROW * 2 + 9;   // hover the third row
		}
		if (getenv("BC_TEST_SCENE") && frames > 5 && frames % 40 == 0 && frames < 400) g_game->Explode->newExplosion((int)g_game->Player[0]->X - 160, (int)g_game->Player[0]->Y - 40, (frames / 40) & 1 ? 1 : 2);
		if (shotPath) frames++;
		if (shotPath && frames >= shotFrames) {
			game.DDraw->SaveScreenshot(shotPath);
			break;
		}
		SDL_Delay(1);
	}
#endif

	game.running = 0;
	BCDialogsShutdown();
	game.Winsock->CloseWinsock();
	delete g_game;
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
