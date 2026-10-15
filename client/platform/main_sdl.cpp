/*
 * Battle City - SDL2 entry point (Linux, Windows, web).
 * Replaces the Win32 WinMain/WndProc: creates the window, translates SDL events into the
 * calls the game already understands, and runs the game loop.
 */
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
			printf("script: drawCalls/frame=%u fps=%d state=%d me=%d pos=%.0f,%.0f moving=%d turning=%d dead=%d frozen=%d dir=%d\n", g_game->DDraw->LastFrameDrawCalls, g_game->Engine->FPS, g_game->State, g_game->Winsock->MyIndex, (float)me->X, (float)me->Y, (int)me->isMoving, (int)me->isTurning, (int)me->isDead, (int)me->isFrozen, (int)me->Direction);
		}
		else if (cmd == "check") { int id; line >> id; if (dlg) CheckDlgButton(dlg, id, 1); }
		else if (cmd == "key") {
			std::string which, state; line >> which >> state;
			int dik = which == "up" ? DIK_UP : which == "down" ? DIK_DOWN : which == "left" ? DIK_LEFT : DIK_RIGHT;
			BCTestSetKey(dik, state == "on");
		}
		else if (cmd == "shot") { std::string path; line >> path; g_game->DDraw->SaveScreenshot(path.c_str()); }
		else if (cmd == "print") { std::string rest; std::getline(line, rest); printf("script:%s\n", rest.c_str()); }
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
#endif
	BCDialogsProcessEvent(&e);
	bool dialog = BCDialogsActive();

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

static void MainLoopIteration() {
	Uint32 t0 = SDL_GetTicks();
	SDL_Event e;
	while (SDL_PollEvent(&e)) HandleEvent(e);
	Uint32 t1 = SDL_GetTicks();
	RunScript();
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
			else if (n == "help") game.Help->ShowHelpDialog();
			else if (n == "manage") game.Manage->ShowManageDlg();
			else if (n == "recover") game.Recover->ShowRecoverDlg();
			else if (n == "admin") game.Admin->ShowAdminDlg();
			else if (n == "adminedit") game.AdminEdit->ShowAdminEditDlg();
			else if (n == "connection") game.ConnectionManager->showConnectionManager();
		}
		if (shotPath && ++frames >= shotFrames) {
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
