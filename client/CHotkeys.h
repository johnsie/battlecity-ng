// Battle City - keyboard shortcuts and the shortcut editor (in the spirit of StarCraft's hotkey editor).
// Every command has up to two keys. Bindings live in one small text file ("hotkeys": next to the login on the
// desktop, localStorage in the browser) so they survive updates. The editor works on a copy until Save is pressed.
#ifndef BC_CHOTKEYS_H
#define BC_CHOTKEYS_H

#include <functional>
#include <string>

enum HotAction {
	// held while playing (read every frame)
	HK_UP, HK_DOWN, HK_LEFT, HK_RIGHT, HK_FIRE, HK_FLARE, HK_CENTER,
	// one press does something
	HK_PICKUP, HK_DROP, HK_BOMB, HK_ORB, HK_MEDKIT, HK_CLOAK,
	HK_INFO, HK_POINTS, HK_MAP, HK_POINTERS, HK_CHAT, HK_RELOAD,
	// voice chat
	HK_VOICE, HK_PTT,
	// building (mayors)
	HK_BUILDMENU, HK_DEMOLISH,
	HK_BUILD0,                       // HK_BUILD0 + n starts placing building n of the build menu (same order as buildNames)
	HK_BUILD_COUNT = 26,
	HK_COUNT = HK_BUILD0 + HK_BUILD_COUNT
};

enum HotCategory { HC_MOVE, HC_COMBAT, HC_ITEMS, HC_INTERFACE, HC_VOICE, HC_BUILD, HC_COUNT };

class CHotkeys {
public:
	static CHotkeys &Get();

	// Is any key of this (held type) command down? `state` is SDL_GetKeyboardState().
	bool Down(int action, const unsigned char *state) const;
	// The one-press command a key is bound to, or -1 (held commands are not returned: they are polled)
	int ActionForKey(int scancode) const;
	// Keys of a command for messages, e.g. "U" or "Enter / Num Enter"; "unbound" when it has none
	std::string KeyText(int action) const;

	// The editor is a full-screen ImGui page: it replaces the options dialog until Save or Cancel, then calls onClose
	void OpenEditor(std::function<void()> onClose);

	static const char *Name(int action);
	static std::string KeyName(int scancode);

private:
	CHotkeys();
	void Load();
	void Save();
	void SetDefaults(int (*to)[2]);
	void SetPreset(int preset);
	bool Draw(float scale);
	void Assign(int action, int slot, int scancode);
	bool Differs(int action) const;

	int live[HK_COUNT][2];
	int edit[HK_COUNT][2];
	bool loaded;
	bool open;
	int selected, listenAct, listenSlot, hoverAct;
	std::string note;
	std::function<void()> onClose;
};

#endif
