/*
 * Battle City - SDL2 entry point (Linux, Windows, web).
 * Replaces the Win32 WinMain/WndProc: creates the window, translates SDL events into the
 * calls the game already understands, and runs the game loop.
 */
#include "CGame.h"
#include <SDL.h>
#include <string>

#ifdef __EMSCRIPTEN__
	#include <emscripten.h>
#endif

static CGame *g_game = NULL;
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

static void HandleEvent(const SDL_Event &e) {
	CGame &game = *g_game;
	BCDialogsProcessEvent(&e);
	bool dialog = BCDialogsActive();

	switch (e.type) {
		case SDL_QUIT:
			g_quit = true;
			game.running = 0;
			break;
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
				g_quit = true;
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

static void MainLoopIteration() {
	SDL_Event e;
	while (SDL_PollEvent(&e)) HandleEvent(e);
	if (g_game->running == 1) g_game->Engine->Frame();
	else g_game->Engine->IdleFrame();
#ifdef __EMSCRIPTEN__
	if (g_quit) emscripten_cancel_main_loop();
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
	if (game.Options->fullscreen == 0) flags |= SDL_WINDOW_RESIZABLE;
	SDL_Window *window = SDL_CreateWindow("Battle City", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                                      game.ResolutionX, game.ResolutionY, flags);
	if (!window) {
		BCMessageBox(NULL, SDL_GetError(), "Battle City: cannot create window", 0);
		SDL_Quit();
		return 1;
	}
	SDL_StartTextInput();

	game.Init((HWND)window, NULL);
	BCDialogsInit(window, game.DDraw->Renderer);

	// Optional: BC_SCREENSHOT=<file> BC_SCREENSHOT_FRAMES=<n> saves a frame and exits (used by automated tests)
	const char *shotPath = getenv("BC_SCREENSHOT");
	int shotFrames = getenv("BC_SCREENSHOT_FRAMES") ? atoi(getenv("BC_SCREENSHOT_FRAMES")) : 30;
	int frames = 0;
	// Optional: BC_TEST_DIALOG=<name> opens one dialog directly (used to screenshot each dialog)
	const char *testDialog = getenv("BC_TEST_DIALOG");

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(MainLoopIteration, 0, 1);
#else
	while (!g_quit) {
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
