/*
 * Battle City - SDL2 implementations of the portability layer (compat.h / net.h).
 */
#include "compat.h"
#include "net.h"
#include "dialogs.h"
#include <string>
#include <iterator>
#include <SDL.h>
#include <sys/stat.h>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
	#include <direct.h>
#else
	#include <unistd.h>
#endif

unsigned long BCGetTicks() { return (unsigned long)SDL_GetTicks(); }

void BCSleep(unsigned int ms) { SDL_Delay(ms); }

int BCMessageBoxNative(HWND parent, const char *text, const char *title, unsigned int flags) {
	if (flags & MB_YESNO) {
		const SDL_MessageBoxButtonData buttons[] = {
			{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
			{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "No" },
		};
		SDL_MessageBoxData box = { SDL_MESSAGEBOX_INFORMATION, (SDL_Window *)parent, title ? title : "", text ? text : "", 2, buttons, NULL };
		int pressed = 2;
		SDL_ShowMessageBox(&box, &pressed);
		return pressed;   // 1 = Yes, as with Win32 IDYES
	}
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title ? title : "", text ? text : "", (SDL_Window *)parent);
	return 1;
}

void BCMakeDir(const char *path) {
#ifdef _WIN32
	_mkdir(path);
#else
	mkdir(path, 0755);
#endif
}

unsigned long BCGetCurrentDirectory(unsigned long size, char *buffer) {
	if (!getcwd(buffer, size)) { buffer[0] = 0; return 0; }
	return (unsigned long)strlen(buffer);
}

char *BCItoa(int value, char *buffer, int radix) {
	if (radix == 10) snprintf(buffer, 34, "%d", value);
	else if (radix == 16) snprintf(buffer, 34, "%x", value);
	else snprintf(buffer, 34, "%d", value);
	return buffer;
}

// ---------------------------------------------------------------------------------------------
// Minimal .ini reader/writer. Case-insensitive section and key names, like GetPrivateProfile*.
// "file" may be a bare name (relative to the working directory) or a path.
// ---------------------------------------------------------------------------------------------
namespace {
	struct IniLine { std::string raw; std::string section; std::string key; std::string value; bool isKey; };

	std::string Lower(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }
	std::string Trim(const std::string &s) {
		size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
		return a == std::string::npos ? "" : s.substr(a, b - a + 1);
	}

	std::vector<IniLine> ReadIni(const char *file) {
		std::vector<IniLine> lines;
		std::ifstream in(file);
		std::string raw, section;
		while (std::getline(in, raw)) {
			if (!raw.empty() && raw.back() == '\r') raw.pop_back();
			IniLine l = { raw, section, "", "", false };
			std::string t = Trim(raw);
			if (t.size() > 2 && t[0] == '[' && t.back() == ']') {
				section = t.substr(1, t.size() - 2);
				l.section = section;
			} else if (!t.empty() && t[0] != ';') {
				size_t eq = t.find('=');
				if (eq != std::string::npos) { l.key = Trim(t.substr(0, eq)); l.value = Trim(t.substr(eq + 1)); l.isKey = true; }
			}
			lines.push_back(l);
		}
		return lines;
	}
}

unsigned long BCProfileGetString(const char *section, const char *key, const char *def, char *out, unsigned long size, const char *file) {
	std::string result = def ? def : "";
	for (const IniLine &l : ReadIni(file)) {
		if (l.isKey && Lower(l.section) == Lower(section) && Lower(l.key) == Lower(key)) { result = l.value; break; }
	}
	if (size == 0) return 0;
	strncpy(out, result.c_str(), size - 1);
	out[size - 1] = 0;
	return (unsigned long)strlen(out);
}

unsigned int BCProfileGetInt(const char *section, const char *key, int def, const char *file) {
	char buffer[64], fallback[32];
	snprintf(fallback, sizeof(fallback), "%d", def);
	BCProfileGetString(section, key, fallback, buffer, sizeof(buffer), file);
	return (unsigned int)atoi(buffer);
}

bool BCProfileWriteString(const char *section, const char *key, const char *value, const char *file) {
	std::vector<IniLine> lines = ReadIni(file);
	bool inSection = false, written = false, sectionSeen = false;
	size_t lastInSection = 0;
	for (size_t i = 0; i < lines.size(); i++) {
		IniLine &l = lines[i];
		bool header = !l.isKey && Trim(l.raw).size() > 2 && Trim(l.raw)[0] == '[';
		if (header) inSection = (Lower(l.section) == Lower(section));
		if (inSection) { sectionSeen = true; lastInSection = i; }
		if (inSection && l.isKey && Lower(l.key) == Lower(key)) {
			l.raw = std::string(key) + "=" + value; written = true; break;
		}
	}
	if (!written) {
		IniLine n = { std::string(key) + "=" + value, section, key, value, true };
		if (sectionSeen) lines.insert(lines.begin() + lastInSection + 1, n);
		else {
			lines.push_back({ std::string("[") + section + "]", section, "", "", false });
			lines.push_back(n);
		}
	}
	std::ofstream out(file, std::ios::trunc);
	if (!out) return false;
	for (const IniLine &l : lines) out << l.raw << "\n";
	return true;
}

// ---------------------------------------------------------------------------------------------
// Sockets
// ---------------------------------------------------------------------------------------------
bool bc_net_startup() {
#ifdef _WIN32
	WSADATA data;
	return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
	return true;
#endif
}

void bc_net_shutdown() {
#ifdef _WIN32
	WSACleanup();
#endif
}

void bc_set_nonblocking(bc_socket s) {
#ifdef _WIN32
	u_long on = 1;
	ioctlsocket(s, FIONBIO, &on);
#else
	fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
}

void bc_close_socket(bc_socket s) {
#ifdef _WIN32
	closesocket(s);
#else
	close(s);
#endif
}

bool bc_would_block() {
#ifdef _WIN32
	int e = WSAGetLastError();
	return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
#else
	return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINPROGRESS;
#endif
}

char *BCStrlwr(char *s) {
	for (char *c = s; *c; ++c) *c = (char)tolower((unsigned char)*c);
	return s;
}

// ---------------------------------------------------------------------------------------------
// Input snapshot
// ---------------------------------------------------------------------------------------------
#include "input_compat.h"

static SDL_Window *g_mouseWindow = NULL;
static int g_mouseRefW = 0, g_mouseRefH = 0;

void BCSetMouseReference(void *sdlWindow, int createdWidth, int createdHeight) {
	g_mouseWindow = (SDL_Window *)sdlWindow;
	g_mouseRefW = createdWidth;
	g_mouseRefH = createdHeight;
}

#ifdef __EMSCRIPTEN__
// SDL's own scaling of browser mouse coordinates depends on what size it believes the canvas is, which goes
// wrong as soon as the page layout, zoom or device pixel ratio changes. Track the pointer ourselves as a
// fraction of the canvas's on-screen rectangle instead; that is correct at any size and resolution.
#include <emscripten.h>
EM_JS(void, bc_track_mouse, (), {
	if (Module.bcMouse) return;
	Module.bcMouse = { fx: -1, fy: -1 };
	var update = function (e) {
		var c = Module['canvas'];
		if (!c) return;
		var r = c.getBoundingClientRect();
		if (r.width > 0 && r.height > 0) {
			Module.bcMouse.fx = (e.clientX - r.left) / r.width;
			Module.bcMouse.fy = (e.clientY - r.top) / r.height;
		}
	};
	window.addEventListener('mousemove', update, true);
	window.addEventListener('mousedown', update, true);
	window.addEventListener('mouseup', update, true);
});
EM_JS(double, bc_mouse_frac_x, (), { return Module.bcMouse ? Module.bcMouse.fx : -1; });
EM_JS(double, bc_mouse_frac_y, (), { return Module.bcMouse ? Module.bcMouse.fy : -1; });
#endif

void BCFixMouse(int *x, int *y) {
#ifdef __EMSCRIPTEN__
	if (!g_mouseWindow) return;
	bc_track_mouse();
	double fx = bc_mouse_frac_x(), fy = bc_mouse_frac_y();
	if (fx < 0 || fy < 0) return;    // no browser mouse event seen yet: keep SDL's value
	int w = 0, h = 0;
	SDL_GetWindowSize(g_mouseWindow, &w, &h);
	if (w > 0 && h > 0) {
		*x = (int)(fx * w);
		*y = (int)(fy * h);
	}
#else
	(void)x; (void)y;
#endif
}

static bool g_testKeys[256];
void BCTestSetKey(int dik, bool down) { if (dik >= 0 && dik < 256) g_testKeys[dik] = down; }

// A click can press and release between two polls (browsers deliver both events in one frame),
// which SDL_GetMouseState would never show as pressed. Latch presses from events and report each
// for at least one poll.
// The pointer position at the moment of the press is kept too: the game polls at its frame rate and the pointer
// may have moved a few pixels since, which was enough to pick the wrong row of a menu.
static bool g_mouseLatch[2];
static int g_latchX, g_latchY;
static bool g_latchHasPos;
void BCLatchMouseButton(int sdlButton, int x, int y) {
	if (sdlButton == SDL_BUTTON_LEFT) g_mouseLatch[0] = true;
	else if (sdlButton == SDL_BUTTON_RIGHT) g_mouseLatch[1] = true;
	else return;
	g_latchX = x; g_latchY = y; g_latchHasPos = true;
}

void BCReadInput(char keyboard[256], DIMOUSESTATE *mouse, int *mouseWindowX, int *mouseWindowY) {
	static const struct { SDL_Scancode sdl; int dik; } map[] = {
		{ SDL_SCANCODE_1, DIK_1 }, { SDL_SCANCODE_2, DIK_2 }, { SDL_SCANCODE_3, DIK_3 },
		{ SDL_SCANCODE_4, DIK_4 }, { SDL_SCANCODE_5, DIK_5 }, { SDL_SCANCODE_6, DIK_6 },
		{ SDL_SCANCODE_7, DIK_7 }, { SDL_SCANCODE_8, DIK_8 }, { SDL_SCANCODE_9, DIK_9 },
		{ SDL_SCANCODE_0, DIK_0 }, { SDL_SCANCODE_TAB, DIK_TAB },
		{ SDL_SCANCODE_LCTRL, DIK_LCONTROL }, { SDL_SCANCODE_RCTRL, DIK_RCONTROL },
		{ SDL_SCANCODE_LSHIFT, DIK_LSHIFT }, { SDL_SCANCODE_RSHIFT, DIK_RSHIFT },
		{ SDL_SCANCODE_UP, DIK_UP }, { SDL_SCANCODE_DOWN, DIK_DOWN },
		{ SDL_SCANCODE_LEFT, DIK_LEFT }, { SDL_SCANCODE_RIGHT, DIK_RIGHT },
	};
	memset(keyboard, 0, 256);
	const Uint8 *state = SDL_GetKeyboardState(NULL);
	for (const auto &m : map) {
		if (state[m.sdl]) keyboard[m.dik] = (char)0x80;
	}
	for (int i = 0; i < 256; i++) if (g_testKeys[i]) keyboard[i] = (char)0x80;
	int x = 0, y = 0;
	Uint32 buttons = SDL_GetMouseState(&x, &y);
	BCFixMouse(&x, &y);
	memset(mouse, 0, sizeof(*mouse));
	if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT))  mouse->rgbButtons[0] = 0x80;
	if (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) mouse->rgbButtons[1] = 0x80;
	if (g_mouseLatch[0]) mouse->rgbButtons[0] = 0x80;
	if (g_mouseLatch[1]) mouse->rgbButtons[1] = 0x80;
#ifndef __EMSCRIPTEN__   // (the browser build gets exact positions from BCFixMouse)
	if ((g_mouseLatch[0] || g_mouseLatch[1]) && g_latchHasPos) { x = g_latchX; y = g_latchY; }   // where the button went down
#endif
	g_mouseLatch[0] = g_mouseLatch[1] = false;
	g_latchHasPos = false;
	*mouseWindowX = x;
	*mouseWindowY = y;
}


// ---- Small settings files (voice chat settings): a file next to the login on the desktop, localStorage in the browser ----
#ifdef __EMSCRIPTEN__
EM_JS(int, bc_text_load, (const char *key), {
	try {
		var v = localStorage.getItem('bc.' + UTF8ToString(key));
		if (!v) { Module.bcTextBytes = null; return 0; }
		Module.bcTextBytes = new TextEncoder().encode(v);
		return Module.bcTextBytes.length;
	} catch (e) { Module.bcTextBytes = null; return 0; }
});
EM_JS(int, bc_text_byte, (int i), { return Module.bcTextBytes ? Module.bcTextBytes[i] : 0; });
EM_JS(void, bc_text_begin, (), { Module.bcTextOut = []; });
EM_JS(void, bc_text_push, (int b), { Module.bcTextOut.push(b & 255); });
EM_JS(void, bc_text_commit, (const char *key), {
	try { localStorage.setItem('bc.' + UTF8ToString(key), new TextDecoder().decode(new Uint8Array(Module.bcTextOut))); } catch (e) {}
});
#endif

std::string BCLoadText(const char *key) {
	std::string text;
#ifdef __EMSCRIPTEN__
	int n = bc_text_load(key);
	for (int i = 0; i < n && i < 4096; i++) text += (char)bc_text_byte(i);
#else
	std::string path;
	if (char *pref = SDL_GetPrefPath("Outiva", "BattleCity")) { path = pref; SDL_free(pref); }
	std::ifstream in(path + key, std::ios::binary);
	if (in) text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
#endif
	return text;
}

void BCSaveText(const char *key, const std::string &text) {
#ifdef __EMSCRIPTEN__
	bc_text_begin();
	for (unsigned char c : text) bc_text_push(c);
	bc_text_commit(key);
#else
	std::string path;
	if (char *pref = SDL_GetPrefPath("Outiva", "BattleCity")) { path = pref; SDL_free(pref); }
	std::ofstream out(path + key, std::ios::binary | std::ios::trunc);
	if (out) out << text;
#endif
}


// ---- Remembered login ----
// Stored on this device only, and only while the "Remember me" box is ticked (unticking erases it).
// Format: "R=<0|1>\n<user>\n<password>\n". The password is the game's own (legacy, 15 characters) one, kept as
// plain text: browser localStorage is origin-private, the desktop file is created readable by the user alone.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(int, bc_login_load, (), {
	try {
		var v = localStorage.getItem('bc.login');
		if (!v) { Module.bcLoginBytes = null; return 0; }
		Module.bcLoginBytes = new TextEncoder().encode(v);
		return Module.bcLoginBytes.length;
	} catch (e) { Module.bcLoginBytes = null; return 0; }
});
EM_JS(int, bc_login_byte, (int i), { return Module.bcLoginBytes ? Module.bcLoginBytes[i] : 0; });
EM_JS(void, bc_login_begin, (), { Module.bcLoginOut = []; });
EM_JS(void, bc_login_push, (int b), { Module.bcLoginOut.push(b & 255); });
EM_JS(void, bc_login_commit, (), {
	try { localStorage.setItem('bc.login', new TextDecoder().decode(new Uint8Array(Module.bcLoginOut))); } catch (e) {}
});
#else
static std::string SavedLoginPath() {
	std::string path;
	if (char *pref = SDL_GetPrefPath("Outiva", "BattleCity")) { path = pref; SDL_free(pref); }
	return path + "login.dat";
}
#endif

static bool ParseSavedLogin(const std::string &text, std::string &user, std::string &pass, bool &remember) {
	size_t a = text.find('\n');
	if (a == std::string::npos || text.compare(0, 2, "R=") != 0) return false;
	size_t b = text.find('\n', a + 1);
	if (b == std::string::npos) return false;
	remember = text.substr(0, a) != "R=0";
	user = text.substr(a + 1, b - a - 1);
	size_t c = text.find('\n', b + 1);
	pass = text.substr(b + 1, c == std::string::npos ? std::string::npos : c - b - 1);
	return true;
}

bool BCLoadSavedLogin(std::string &user, std::string &pass, bool &remember) {
	std::string text;
#ifdef __EMSCRIPTEN__
	int n = bc_login_load();
	for (int i = 0; i < n && i < 256; i++) text += (char)bc_login_byte(i);
#else
	std::ifstream in(SavedLoginPath(), std::ios::binary);
	if (!in) return false;
	text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
#endif
	return ParseSavedLogin(text, user, pass, remember);
}

void BCStoreLogin(const std::string &user, const std::string &pass, bool remember) {
	// When the box is unticked keep only the choice itself, never the name or password
	std::string text = std::string(remember ? "R=1\n" : "R=0\n") + (remember ? user : "") + "\n" + (remember ? pass : "") + "\n";
#ifdef __EMSCRIPTEN__
	bc_login_begin();
	for (unsigned char c : text) bc_login_push(c);
	bc_login_commit();
#else
	std::string path = SavedLoginPath();
	if (path == "login.dat") return;          // no writable preferences folder
#ifndef _WIN32
	std::ofstream touch(path, std::ios::binary | std::ios::trunc);   // create, then restrict to the owner
	touch.close();
	chmod(path.c_str(), 0600);
#endif
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out << text;
#endif
}

// ---- Restart after a lost connection ----
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
static int g_argc = 0;
static char **g_argv = nullptr;
void BCSetCommandLine(int argc, char **argv) { g_argc = argc; g_argv = argv; }

void BCRestartApplication() {
#ifdef __EMSCRIPTEN__
	EM_ASM({ location.reload(); });   // not emscripten_run_script: it uses eval(), which the site's Content-Security-Policy forbids
#elif defined(_WIN32)
	char path[MAX_PATH];
	if (GetModuleFileNameA(NULL, path, MAX_PATH)) {
		STARTUPINFOA si; PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));
		if (CreateProcessA(path, GetCommandLineA(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
			CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
		}
	}
	exit(0);
#else
	if (g_argv) {
		SDL_Quit();   // release the window and audio device before the process image is replaced
		execv("/proc/self/exe", g_argv);
	}
	exit(0);
#endif
}
