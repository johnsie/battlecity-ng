/*
 * Battle City - SDL2 implementations of the portability layer (compat.h / net.h).
 */
#include "compat.h"
#include "net.h"
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

int BCMessageBox(HWND parent, const char *text, const char *title, unsigned int flags) {
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
	int x = 0, y = 0;
	Uint32 buttons = SDL_GetMouseState(&x, &y);
	memset(mouse, 0, sizeof(*mouse));
	if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT))  mouse->rgbButtons[0] = 0x80;
	if (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) mouse->rgbButtons[1] = 0x80;
	*mouseWindowX = x;
	*mouseWindowY = y;
}
