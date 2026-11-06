/*
 * Battle City - Win32 dialog emulation on top of Dear ImGui.
 * See dialogs.h for the overview. Layouts come from dialogs_generated.cpp (generated from the .rc file).
 */
#include "dialogs.h"
#include "dialogs_defs.h"
#include "input_compat.h"
#include "../resource.h"
#include "../OutivaLogin.h"
#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <deque>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

// Win32 dialog units -> pixels (MS Sans Serif 8pt at 96 dpi)
const float DLU_X = 1.55f;
const float DLU_Y = 1.625f;

struct Dlg;

struct Ctl {
	const BCControlDef *def = nullptr;
	Dlg *dlg = nullptr;
	std::string text;               // label caption / edit contents
	std::vector<char> buffer;       // editable text buffer (edit controls)
	int limit = 0;                  // EM_LIMITTEXT
	int check = 0;                  // check / radio state
	std::vector<std::string> items; // list / combo items
	int cursel = -1;
	WNDPROC subclass = nullptr;
	bool scrollToBottom = false;
	bool refocus = false;
	bool isEditable() const { return def->kind == BCK_EDIT && !(def->flags & BCF_READONLY); }
	bool isLog() const { return def->kind == BCK_EDIT && (def->flags & BCF_READONLY) && (def->flags & BCF_MULTILINE); }
};

struct Dlg {
	const BCDialogDef *def = nullptr;
	DLGPROC proc = nullptr;
	std::vector<Ctl> ctls;          // never resized after creation: control handles stay valid
	bool open = false;
	bool focusFirst = false;
	int loginMode = 1;               // 0: Battle City account (name and password), 1: Outiva account, 2: guest
};

const float FONT_PIXELS = 11.5f;     // font size at scale 1 (the original dialogs' size at 1024x768)
const float UI_BOOST = 1.25f;        // dialogs are drawn this much larger than the original Win32 ones: they were hard to read
const float MIN_UI_SCALE = 1.0f;     // never smaller than this just because the window/canvas is small ...
const float MIN_FIT_SCALE = 0.6f;    // ... unless a dialog would not fit the window at all
float g_scale = 0.0f;                // output pixels per logical pixel; the UI is laid out and rasterised at this scale
float g_mouseScaleX = 1.0f;
float g_mouseScaleY = 1.0f;
bool g_fontBuilt = false;
ImGuiStyle g_baseStyle;

SDL_Window *g_window = nullptr;
SDL_Renderer *g_renderer = nullptr;
bool g_imguiReady = false;
std::map<int, Dlg *> g_dialogs;          // dialog id -> persistent instance (handles outlive the dialog)
std::map<void *, Dlg *> g_dlgHandles;
std::map<void *, Ctl *> g_ctlHandles;
Dlg *g_active = nullptr;

struct Message {
	std::string title, text;
	bool yesNo;
	std::function<void(int)> done;
	bool opened;
};
std::deque<Message> g_messages;

Dlg *AsDlg(HWND h) { auto it = g_dlgHandles.find(h); return it == g_dlgHandles.end() ? nullptr : it->second; }
Ctl *AsCtl(HWND h) { auto it = g_ctlHandles.find(h); return it == g_ctlHandles.end() ? nullptr : it->second; }

size_t EditCapacity(const Ctl &c) {
	size_t base = (c.def->flags & BCF_MULTILINE) ? 8192 : 512;
	return c.limit > 0 ? (size_t)c.limit + 1 : base;
}

void SetEditText(Ctl &c, const char *text) {
	c.text = text ? text : "";
	if (c.isEditable()) {
		size_t cap = EditCapacity(c);
		if (c.buffer.size() != cap) c.buffer.assign(cap, 0);
		strncpy(c.buffer.data(), c.text.c_str(), cap - 1);
		c.buffer[cap - 1] = 0;
		c.text = c.buffer.data();
	}
}

std::string StripMnemonics(const char *s) {
	std::string out;
	for (const char *p = s; *p; ++p) {
		if (*p == '&') { if (p[1] == '&') { out += '&'; ++p; } continue; }
		out += *p;
	}
	return out;
}

Ctl *FindCtl(Dlg *d, int id);
bool g_rememberLogin = true;      // "Remember me on this device" (on by default; unticking forgets the saved login)

void ResetDialog(Dlg *d) {
	for (Ctl &c : d->ctls) {
		c.text = c.def->text;
		c.check = 0;
		c.items.clear();
		c.cursel = -1;
		c.limit = 0;
		c.subclass = nullptr;
		c.scrollToBottom = false;
		c.refocus = false;
		c.buffer.clear();
		if (c.isEditable()) SetEditText(c, "");
	}
	d->loginMode = getenv("BC_TEST_LOGIN_TAB") ? atoi(getenv("BC_TEST_LOGIN_TAB")) : 1;   // (the variable is for screenshots)
	if (d->def->id == ID_LOGIN) {
		// Fill the form from the login remembered on this device
		std::string user, pass;
		bool remember = true;
		if (BCLoadSavedLogin(user, pass, remember)) {
			g_rememberLogin = remember;
			if (Ctl *u = FindCtl(d, IDUSER)) SetEditText(*u, user.c_str());
			if (Ctl *w = FindCtl(d, IDPASS)) SetEditText(*w, pass.c_str());
		}
	}
}

Dlg *GetOrCreateDialog(int id) {
	auto it = g_dialogs.find(id);
	if (it != g_dialogs.end()) return it->second;
	for (int i = 0; i < g_dialogDefCount; i++) {
		if (g_dialogDefs[i].id != id) continue;
		Dlg *d = new Dlg();
		d->def = &g_dialogDefs[i];
		d->ctls.resize(d->def->controlCount);
		for (int k = 0; k < d->def->controlCount; k++) {
			d->ctls[k].def = &d->def->controls[k];
			d->ctls[k].dlg = d;
			g_ctlHandles[&d->ctls[k]] = &d->ctls[k];
		}
		g_dialogs[id] = d;
		g_dlgHandles[d] = d;
		return d;
	}
	return nullptr;
}

Ctl *FindCtl(Dlg *d, int id) {
	if (!d || id == -1) return nullptr;
	for (Ctl &c : d->ctls) if (c.def->id == id) return &c;
	return nullptr;
}

// Radio buttons form a group from one WS_GROUP radio up to (not including) the next WS_GROUP one
void SelectRadio(Dlg *d, Ctl *chosen) {
	size_t idx = chosen - &d->ctls[0];
	size_t first = idx, last = idx;
	while (first > 0 && d->ctls[first].def->kind == BCK_RADIO && !(d->ctls[first].def->flags & BCF_GROUP)) first--;
	while (last + 1 < d->ctls.size() && d->ctls[last + 1].def->kind == BCK_RADIO && !(d->ctls[last + 1].def->flags & BCF_GROUP)) last++;
	for (size_t i = first; i <= last; i++) if (d->ctls[i].def->kind == BCK_RADIO) d->ctls[i].check = 0;
	chosen->check = 1;
}

void Notify(Dlg *d, int id, int code, Ctl *from) {
	if (d->proc) d->proc(d, WM_COMMAND, (WPARAM)MAKELONG(id, code), (LPARAM)from);
}

void UpdateMouseScale() {
	int ww = 1, wh = 1, ow = 1, oh = 1;
	SDL_GetWindowSize(g_window, &ww, &wh);
	SDL_GetRendererOutputSize(g_renderer, &ow, &oh);
	g_mouseScaleX = ow / (float)std::max(ww, 1);
	g_mouseScaleY = oh / (float)std::max(wh, 1);
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Win32 API emulation
// ---------------------------------------------------------------------------------------------
INT_PTR BCDialogBox(HINSTANCE, const char *resourceId, HWND, DLGPROC proc) {
	Dlg *d = GetOrCreateDialog((int)(uintptr_t)resourceId);
	if (!d) return -1;
	d->proc = proc;
	ResetDialog(d);
	d->open = true;
	d->focusFirst = true;
	g_active = d;
	proc(d, WM_INITDIALOG, 0, 0);
	return 0;
}

bool EndDialog(HWND dialog, INT_PTR) {
	Dlg *d = AsDlg(dialog);
	if (!d) return false;
	d->open = false;
	if (g_active == d) g_active = nullptr;
	return true;
}

HWND GetDlgItem(HWND dialog, int id) { return FindCtl(AsDlg(dialog), id); }

UINT GetDlgItemText(HWND dialog, int id, char *buffer, int maxCount) {
	Ctl *c = FindCtl(AsDlg(dialog), id);
	if (!c || maxCount <= 0) { if (maxCount > 0) buffer[0] = 0; return 0; }
	std::string text = c->isEditable() && !c->buffer.empty() ? std::string(c->buffer.data()) : c->text;
	strncpy(buffer, text.c_str(), maxCount - 1);
	buffer[maxCount - 1] = 0;
	return (UINT)strlen(buffer);
}

bool SetDlgItemText(HWND dialog, int id, const char *text) {
	Ctl *c = FindCtl(AsDlg(dialog), id);
	if (!c) return false;
	if (c->isEditable()) SetEditText(*c, text);
	else c->text = text ? text : "";
	if (c->isLog()) c->scrollToBottom = true;
	return true;
}

UINT GetDlgItemInt(HWND dialog, int id, BOOL *translated, BOOL) {
	char buf[64];
	GetDlgItemText(dialog, id, buf, sizeof(buf));
	if (translated) *translated = buf[0] != 0;
	return (UINT)atoi(buf);
}

bool SetDlgItemInt(HWND dialog, int id, UINT value, BOOL isSigned) {
	char buf[32];
	if (isSigned) snprintf(buf, sizeof(buf), "%d", (int)value); else snprintf(buf, sizeof(buf), "%u", value);
	return SetDlgItemText(dialog, id, buf);
}

bool CheckDlgButton(HWND dialog, int id, UINT check) {
	Dlg *d = AsDlg(dialog);
	Ctl *c = FindCtl(d, id);
	if (!c) return false;
	if (c->def->kind == BCK_RADIO && check) SelectRadio(d, c);
	else c->check = check ? 1 : 0;
	return true;
}

UINT IsDlgButtonChecked(HWND dialog, int id) {
	Ctl *c = FindCtl(AsDlg(dialog), id);
	return c ? c->check : 0;
}

LRESULT SendDlgItemMessage(HWND dialog, int id, UINT msg, WPARAM wParam, LPARAM lParam) {
	return BCSendMessage(GetDlgItem(dialog, id), msg, wParam, lParam);
}

LRESULT BCSendMessage(HWND target, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (Ctl *c = AsCtl(target)) {
		switch (msg) {
			case EM_LIMITTEXT:
				c->limit = (int)wParam;
				if (c->isEditable()) SetEditText(*c, c->text.c_str());
				return 0;
			case LB_ADDSTRING:
			case CB_ADDSTRING: {
				std::string item = lParam ? (const char *)lParam : "";
				if (c->def->flags & BCF_SORT) {
					auto pos = std::upper_bound(c->items.begin(), c->items.end(), item, [](const std::string &a, const std::string &b) {
						return strcasecmp(a.c_str(), b.c_str()) < 0; });
					int index = (int)(pos - c->items.begin());
					c->items.insert(pos, item);
					return index;
				}
				c->items.push_back(item);
				return (LRESULT)c->items.size() - 1;
			}
			case LB_RESETCONTENT: c->items.clear(); c->cursel = -1; return 0;
			case LB_GETCURSEL:
			case CB_GETCURSEL: return c->cursel;
			case LB_SETCURSEL:
			case CB_SETCURSEL: c->cursel = ((int)wParam >= 0 && (int)wParam < (int)c->items.size()) ? (int)wParam : -1; return c->cursel;
			case LB_GETCOUNT: return (LRESULT)c->items.size();
			case LB_GETTEXT:
			case CB_GETLBTEXT:
				if ((int)wParam < 0 || (int)wParam >= (int)c->items.size()) return LB_ERR;
				strcpy((char *)lParam, c->items[wParam].c_str());
				return (LRESULT)c->items[wParam].size();
			case BM_GETCHECK: return c->check;
			case BM_SETCHECK: CheckDlgButton(c->dlg, c->def->id, (UINT)wParam); return 0;
			case WM_VSCROLL: c->scrollToBottom = true; return 0;
			case WM_LBUTTONDBLCLK: return c->subclass ? c->subclass(c, msg, wParam, lParam) : 0;
			default: return 0;
		}
	}
	if (Dlg *d = AsDlg(target)) {
		return d->proc ? d->proc(d, msg, wParam, lParam) : 0;
	}
	// Anything else is the main game window: only "please close" is meaningful
	if (msg == WM_CLOSE) {
		SDL_Event quit;
		SDL_zero(quit);
		quit.type = SDL_QUIT;
		SDL_PushEvent(&quit);
	}
	return 0;
}

bool GetScrollInfo(HWND control, int, SCROLLINFO *info) {
	Ctl *c = AsCtl(control);
	if (!c || !info) return false;
	info->nMin = 0;
	info->nMax = (int)std::count(c->text.begin(), c->text.end(), '\n');
	info->nPos = info->nMax;
	return true;
}

WNDPROC BCSetControlProc(HWND control, WNDPROC proc) {
	Ctl *c = AsCtl(control);
	if (!c) return nullptr;
	WNDPROC old = c->subclass;
	c->subclass = proc;
	return old;
}

// ---------------------------------------------------------------------------------------------
// Dear ImGui front end
// ---------------------------------------------------------------------------------------------
// (Re)builds the font at the right pixel size and scales the style, so text is rasterised 1:1 on screen
static void ApplyScale(float k) {
	ImGuiIO &io = ImGui::GetIO();
	if (!g_fontBuilt) {
		// Keep one reasonably high-resolution atlas and scale it for each dialog. Rebuilding
		// the TTF atlas when switching dialog sizes stalls the browser's UI thread.
		const float atlasPixels = 24.0f;
		ImFontConfig cfg;
		cfg.PixelSnapH = true;
		cfg.OversampleH = 1;
		cfg.OversampleV = 1;
		if (!io.Fonts->AddFontFromFileTTF("Vera.ttf", atlasPixels, &cfg)) {
			cfg.SizePixels = atlasPixels;
			io.Fonts->AddFontDefault(&cfg);
		}
		io.Fonts->Build();
		ImGui_ImplSDLRenderer2_DestroyDeviceObjects();
		g_fontBuilt = true;
	}
	io.FontGlobalScale = std::max(8.0f, FONT_PIXELS * k) / 24.0f;
	ImGuiStyle &style = ImGui::GetStyle();
	style = g_baseStyle;
	style.ScaleAllSizes(k);
	g_scale = k;
}

void BCDialogsInit(void *sdlWindow, void *sdlRenderer) {
	g_window = (SDL_Window *)sdlWindow;
	g_renderer = (SDL_Renderer *)sdlRenderer;
	if (!g_window || !g_renderer) return;
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef __EMSCRIPTEN__
	// SDL's browser cursor changes can hide the pointer while an ImGui text field is active.
	// Let the browser keep control of the visible cursor instead.
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
#endif
	ImGui::StyleColorsLight();
	ImGuiStyle &style = ImGui::GetStyle();
	style.WindowRounding = 0; style.FrameRounding = 0; style.ScrollbarRounding = 0; style.GrabRounding = 0;
	style.WindowBorderSize = 1;
	style.FrameBorderSize = 1;                       // outline edit boxes, check boxes and radio buttons: they were hard to see
	style.Colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.25f, 0.75f);
	g_baseStyle = style;
	ImGui_ImplSDL2_InitForSDLRenderer(g_window, g_renderer);
	ImGui_ImplSDLRenderer2_Init(g_renderer);
	UpdateMouseScale();
	g_imguiReady = true;
}

void BCDialogsShutdown() {
	if (!g_imguiReady) return;
	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	g_imguiReady = false;
}

bool BCLoginRemember() { return g_rememberLogin; }

void BCSetLoginLayout(bool active) {
#ifdef __EMSCRIPTEN__
	EM_ASM({ if (window.bcSetLoginLayout) window.bcSetLoginLayout(!!$0); }, active ? 1 : 0);
#else
	(void)active;
#endif
}

bool BCDialogsActive() { return g_active != nullptr || !g_messages.empty(); }

bool BCMessagesPending() { return !g_messages.empty(); }

void BCMessageBoxAsync(const char *text, const char *title, unsigned int flags, std::function<void(int)> done) {
	if (!g_imguiReady) {
		int r = BCMessageBoxNative(nullptr, text, title, flags);
		if (done) done(r);
		return;
	}
	g_messages.push_back({ (title && *title) ? title : "BattleCity", text ? text : "", (flags & MB_YESNO) != 0, done, false });
}

int BCMessageBox(HWND, const char *text, const char *title, unsigned int flags) {
	BCMessageBoxAsync(text, title, flags, nullptr);
	return 1;
}

HWND BCDialogsActiveHandle() { return g_active; }

void BCDialogsCloseActive() {
	if (g_active) { g_active->open = false; g_active = nullptr; }
}

void BCDialogsProcessEvent(const void *sdlEvent) {
	if (!g_imguiReady || (!g_active && g_messages.empty())) return;
	const SDL_Event &e = *(const SDL_Event *)sdlEvent;
	ImGuiIO &io = ImGui::GetIO();
	switch (e.type) {
		case SDL_MOUSEMOTION: {
			int mx = e.motion.x, my = e.motion.y;
			BCFixMouse(&mx, &my);
			io.AddMousePosEvent(mx * g_mouseScaleX, my * g_mouseScaleY);
			break;
		}
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP: {
			int bx = e.button.x, by = e.button.y;
			BCFixMouse(&bx, &by);
			io.AddMousePosEvent(bx * g_mouseScaleX, by * g_mouseScaleY);
			// Let the SDL backend track button state and mouse source (mouse vs touch emulation).
			// Some browser SDL events have no registered window ID, so retain the direct
			// fallback for those events after giving the backend a chance to handle them.
			if (!ImGui_ImplSDL2_ProcessEvent(&e)) {
				int b = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_RIGHT ? 1 : e.button.button == SDL_BUTTON_MIDDLE ? 2 : -1;
				if (b >= 0) io.AddMouseButtonEvent(b, e.type == SDL_MOUSEBUTTONDOWN);
			}
			break;
		}
		case SDL_MOUSEWHEEL: io.AddMouseWheelEvent((float)e.wheel.x, (float)e.wheel.y); break;
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_RESIZED) UpdateMouseScale();
			ImGui_ImplSDL2_ProcessEvent(&e);
			break;
		default: ImGui_ImplSDL2_ProcessEvent(&e); break;
	}
}

namespace {

void DrawControl(Dlg *d, Ctl &c, int index, float originY) {
	const BCControlDef &def = *c.def;
	ImGui::PushID(index);
	ImVec2 pos(def.x * DLU_X * g_scale, originY + def.y * DLU_Y * g_scale);
	ImVec2 size(def.w * DLU_X * g_scale, def.h * DLU_Y * g_scale);
	ImGui::SetCursorPos(pos);

	switch (def.kind) {
		case BCK_LABEL:
		case BCK_LABEL_CENTER:
		case BCK_LABEL_RIGHT: {
			std::string text = StripMnemonics(c.text.c_str());
			if (text.empty()) break;
			if (def.kind != BCK_LABEL) {
				float tw = ImGui::CalcTextSize(text.c_str()).x;
				if (tw < size.x) ImGui::SetCursorPos(ImVec2(pos.x + (def.kind == BCK_LABEL_CENTER ? (size.x - tw) / 2 : size.x - tw), pos.y));
			}
			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (size.x > 0 ? size.x : 1000.0f));
			ImGui::TextUnformatted(text.c_str());
			ImGui::PopTextWrapPos();
			break;
		}
		case BCK_BUTTON:
		case BCK_DEFBUTTON: {
			std::string label = StripMnemonics(c.text.c_str()) + "###b";
			if (def.kind == BCK_DEFBUTTON) ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 1));
			bool clicked = ImGui::Button(label.c_str(), size);
			if (def.kind == BCK_DEFBUTTON) ImGui::PopStyleColor();
			if (clicked) Notify(d, def.id, BN_CLICKED, &c);
			break;
		}
		case BCK_CHECK: {
			std::string label = StripMnemonics(c.text.c_str()) + "###c";
			bool v = c.check != 0;
			if (ImGui::Checkbox(label.c_str(), &v)) { c.check = v ? 1 : 0; Notify(d, def.id, BN_CLICKED, &c); }
			break;
		}
		case BCK_RADIO: {
			std::string label = StripMnemonics(c.text.c_str()) + "###r";
			if (ImGui::RadioButton(label.c_str(), c.check != 0)) { SelectRadio(d, &c); Notify(d, def.id, BN_CLICKED, &c); }
			break;
		}
		case BCK_EDIT: {
			if (c.isLog()) {
				ImGui::BeginChild("##log", size, true);
				ImGui::PushTextWrapPos(0.0f);
				ImGui::TextUnformatted(c.text.c_str());
				ImGui::PopTextWrapPos();
				if (c.scrollToBottom) { ImGui::SetScrollHereY(1.0f); c.scrollToBottom = false; }
				ImGui::EndChild();
				break;
			}
			if (c.buffer.empty() && c.isEditable()) SetEditText(c, c.text.c_str());
			ImGuiInputTextFlags flags = 0;
			if (def.flags & BCF_PASSWORD) flags |= ImGuiInputTextFlags_Password;
			if (def.flags & BCF_READONLY) flags |= ImGuiInputTextFlags_ReadOnly;
			if (def.flags & BCF_NUMBER) flags |= ImGuiInputTextFlags_CharsDecimal;
			bool multiline = (def.flags & BCF_MULTILINE) && !(def.flags & BCF_WANTRETURN) && def.h > 20;
			if (c.refocus || (d->focusFirst && c.isEditable())) { ImGui::SetKeyboardFocusHere(); c.refocus = false; d->focusFirst = false; }
			bool entered = false;
			if (c.isEditable()) {
				if (multiline) {
					ImGui::InputTextMultiline("##e", c.buffer.data(), c.buffer.size(), size, flags);
				} else {
					ImGui::SetNextItemWidth(size.x);
					entered = ImGui::InputText("##e", c.buffer.data(), c.buffer.size(), flags | ImGuiInputTextFlags_EnterReturnsTrue);
				}
				c.text = c.buffer.data();
			} else {
				ImGui::SetNextItemWidth(size.x);
				ImGui::InputText("##e", (char *)c.text.c_str(), c.text.size() + 1, flags);
			}
			if (entered) {
				if (c.subclass) {
					c.subclass(&c, WM_CHAR, 13, 0);     // e.g. "send chat line"
					c.refocus = true;
				} else {
					// Enter presses the default button, as in a Win32 dialog
					for (Ctl &other : d->ctls) {
						if (other.def->kind == BCK_DEFBUTTON) { Notify(d, other.def->id, BN_CLICKED, &other); break; }
					}
				}
			}
			break;
		}
		case BCK_LIST: {
			if (ImGui::BeginListBox("##l", size)) {
				for (int i = 0; i < (int)c.items.size() && d->open; i++) {
					ImGui::PushID(i);
					if (ImGui::Selectable(c.items[i].c_str(), c.cursel == i, ImGuiSelectableFlags_AllowDoubleClick)) c.cursel = i;
					if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
						c.cursel = i;
						if (c.subclass) c.subclass(&c, WM_LBUTTONDBLCLK, 0, 0);
					}
					ImGui::PopID();
				}
				ImGui::EndListBox();
			}
			break;
		}
		case BCK_COMBO: {
			ImGui::SetNextItemWidth(size.x);
			const char *preview = (c.cursel >= 0 && c.cursel < (int)c.items.size()) ? c.items[c.cursel].c_str() : "";
			if (ImGui::BeginCombo("##cb", preview)) {
				for (int i = 0; i < (int)c.items.size(); i++) {
					if (ImGui::Selectable(c.items[i].c_str(), c.cursel == i)) {
						c.cursel = i;
						Notify(d, def.id, CBN_SELCHANGE, &c);
					}
				}
				ImGui::EndCombo();
			}
			break;
		}
	}
	ImGui::PopID();
}

static float g_loginPad = 24.0f;   // right margin of the login form (matches its left padding)

bool DrawLoginEdit(Dlg *d, Ctl *c, const char *hint, bool password, bool focus) {
	if (!c) return false;
	if (c->buffer.empty()) SetEditText(*c, c->text.c_str());
	ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
	if (password) flags |= ImGuiInputTextFlags_Password;
	ImGui::PushID(c->def->id);
	ImGui::SetNextItemWidth(-g_loginPad);
	if (focus && d->focusFirst) { ImGui::SetKeyboardFocusHere(); d->focusFirst = false; }
	bool entered = ImGui::InputTextWithHint("##field", hint, c->buffer.data(), c->buffer.size(), flags);
	c->text = c->buffer.data();
	ImGui::PopID();
	return entered;
}

void DrawLoginTab(Dlg *d, const char *label, int mode, float width) {
	bool active = d->loginMode == mode;
	if (active) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.18f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.18f, 0.25f, 1.0f));
	} else {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.055f, 0.075f, 0.10f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.09f, 0.13f, 0.18f, 1.0f));
	}
	if (ImGui::Button(label, ImVec2(width, 40.0f * g_scale))) {
		d->loginMode = mode;
		d->focusFirst = true;
	}
	ImGui::PopStyleColor(2);
}

void DrawLoginSubmit(Dlg *d, const char *label) {
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.91f, 0.18f, 0.13f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.25f, 0.18f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.78f, 0.12f, 0.09f, 1.0f));
	if (ImGui::Button(label, ImVec2(-g_loginPad, 43.0f * g_scale))) Notify(d, IDOK, BN_CLICKED, FindCtl(d, IDOK));
	ImGui::PopStyleColor(3);
}

bool DrawLoginLink(const char *label) {
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.13f, 0.19f, 0.26f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.23f, 0.31f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.79f, 0.88f, 1.0f));
	bool clicked = ImGui::SmallButton(label);
	ImGui::PopStyleColor(4);
	return clicked;
}

void DrawModernLogin(Dlg *d) {
	ImGuiIO &io = ImGui::GetIO();
	const float margin = 14.0f;
	const bool wide = io.DisplaySize.x >= 760.0f && io.DisplaySize.y >= 500.0f;
	const float width = std::min(wide ? 900.0f : 520.0f, io.DisplaySize.x - margin * 2.0f);
	const float height = std::min(wide ? 560.0f : 620.0f, io.DisplaySize.y - margin * 2.0f);
	const float padding = wide ? 30.0f : 20.0f;
	g_loginPad = padding;

	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.052f, 0.073f, 0.985f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.29f, 0.37f, 0.9f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.91f, 0.94f, 0.97f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.075f, 0.105f, 0.14f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.10f, 0.14f, 0.19f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.12f, 0.16f, 0.22f, 1.0f));
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
	                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
	                         ImGuiWindowFlags_NoScrollWithMouse;
	if (ImGui::Begin("Battle City sign in###modern-login", nullptr, flags)) {
		if (wide) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.075f, 0.105f, 0.135f, 1.0f));
			ImGui::BeginChild("##login-hero", ImVec2(width * 0.38f, height), false, ImGuiWindowFlags_NoScrollbar);
			ImGui::SetCursorPos(ImVec2(34.0f, 38.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.27f, 0.18f, 1.0f));
			ImGui::SetWindowFontScale(1.7f);
			ImGui::TextUnformatted("BATTLE CITY");
			ImGui::SetWindowFontScale(1.0f);
			ImGui::PopStyleColor();
			ImGui::SetCursorPos(ImVec2(34.0f, 112.0f));
			ImGui::SetWindowFontScale(1.45f);
			ImGui::TextWrapped("Build your city. Rally your team. Rule the map.");
			ImGui::SetWindowFontScale(1.0f);
			ImGui::SetCursorPos(ImVec2(34.0f, 218.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f, 0.74f, 0.80f, 1.0f));
			ImGui::PushTextWrapPos(width * 0.38f - 34.0f);
			ImGui::TextWrapped("Play as a guest straight away, or sign in with Outiva to keep your points, history and city.");
			ImGui::PopTextWrapPos();
			ImGui::SetCursorPos(ImVec2(34.0f, height - 62.0f));
			ImGui::TextUnformatted("LIVE  -  bc.outiva.com");
			ImGui::PopStyleColor();
			ImGui::EndChild();
			ImGui::PopStyleColor();
			ImGui::SameLine(0, 0);
		}

		float panelWidth = wide ? width * 0.62f : width;
		ImGui::BeginChild("##login-form", ImVec2(panelWidth, height), false);
		if (!wide && height >= 560.0f) {
			d->focusFirst = false;
			ImGui::SetScrollY(0.0f);
		}
		ImGui::SetCursorPos(ImVec2(padding, padding));
		if (!wide) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.27f, 0.18f, 1.0f));
			ImGui::SetWindowFontScale(1.45f);
			ImGui::TextUnformatted("BATTLE CITY");
			ImGui::SetWindowFontScale(1.0f);
			ImGui::PopStyleColor();
			ImGui::Dummy(ImVec2(0, 8.0f));
		}

		float available = panelWidth - padding * 2.0f - 4.0f;
		float tabWidth = (available - 16.0f) / 3.0f;
		ImGui::SetCursorPosX(padding);
		DrawLoginTab(d, "Outiva", 1, tabWidth);
		ImGui::SameLine(0, 8.0f);
		DrawLoginTab(d, "Guest", 2, tabWidth);
		ImGui::SameLine(0, 8.0f);
		DrawLoginTab(d, "Old account", 0, tabWidth);
		ImGui::SetCursorPosX(padding);
		ImGui::Dummy(ImVec2(0, 16.0f));

		Ctl *user = FindCtl(d, IDUSER);
		Ctl *pass = FindCtl(d, IDPASS);
		std::string outivaCallsign = OutivaCallsign();
		bool hasOutiva = !OutivaTicket().empty();

		ImGui::SetCursorPosX(padding);
		ImGui::BeginGroup();
		ImGui::PushItemWidth(available);
		if (d->loginMode == 0) {
			ImGui::SetWindowFontScale(1.45f);
			ImGui::TextUnformatted("Welcome back");
			ImGui::SetWindowFontScale(1.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.71f, 0.78f, 1.0f));
			ImGui::TextWrapped("Sign in with your existing Battle City character.");
			ImGui::PopStyleColor();
			ImGui::Dummy(ImVec2(0, 12.0f));
			ImGui::TextUnformatted("Character name");
			bool enter = DrawLoginEdit(d, user, "Your character name", false, wide);
			ImGui::Dummy(ImVec2(0, 8.0f));
			ImGui::TextUnformatted("Password");
			enter = DrawLoginEdit(d, pass, "Your password", true, false) || enter;
			ImGui::Dummy(ImVec2(0, 6.0f));
			ImGui::Checkbox("Remember me on this device", &g_rememberLogin);
			ImGui::Dummy(ImVec2(0, 8.0f));
			if (enter) Notify(d, IDOK, BN_CLICKED, FindCtl(d, IDOK));
			if (d->open) DrawLoginSubmit(d, "Enter Battle City");
			if (d->open && hasOutiva) {
				// Signed in with Outiva: an old character can be moved over to the Outiva account (its password is checked once)
				ImGui::Dummy(ImVec2(0, 8.0f));
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.38f, 0.62f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.47f, 0.75f, 1.0f));
				if (ImGui::Button("Link this character to my Outiva account", ImVec2(-g_loginPad, 34.0f * g_scale))) Notify(d, IDLINKLOGIN, BN_CLICKED, nullptr);
				ImGui::PopStyleColor(2);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.52f, 0.60f, 0.68f, 1.0f));
				ImGui::TextWrapped("After this, signing in with Outiva plays this character.");
				ImGui::PopStyleColor();
			}
			if (d->open) {
				ImGui::Dummy(ImVec2(0, 10.0f));
				if (DrawLoginLink("Create character")) Notify(d, 3, BN_CLICKED, FindCtl(d, 3));
				ImGui::SameLine();
				if (DrawLoginLink("Forgot password")) Notify(d, 5, BN_CLICKED, FindCtl(d, 5));
				ImGui::SameLine();
				if (DrawLoginLink("Edit character")) Notify(d, 4, BN_CLICKED, FindCtl(d, 4));
			}
		} else if (d->loginMode == 2) {
			ImGui::SetWindowFontScale(1.45f);
			ImGui::TextUnformatted("Play as a guest");
			ImGui::SetWindowFontScale(1.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.71f, 0.78f, 1.0f));
			ImGui::TextWrapped("Jump straight in. Nothing is saved: no points, history or ranking. Guests can join other players' cities but cannot found one. \"(nonreg)\" is added to your name.");
			ImGui::PopStyleColor();
			ImGui::Dummy(ImVec2(0, 12.0f));
			ImGui::TextUnformatted("Guest name");
			bool enter = DrawLoginEdit(d, user, "Up to 6 characters", false, wide);
			ImGui::Dummy(ImVec2(0, 14.0f));
			if (enter) Notify(d, IDGUESTLOGIN, BN_CLICKED, nullptr);
			if (d->open) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.91f, 0.18f, 0.13f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.25f, 0.18f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.78f, 0.12f, 0.09f, 1.0f));
				if (ImGui::Button("Play as guest", ImVec2(-g_loginPad, 43.0f * g_scale))) Notify(d, IDGUESTLOGIN, BN_CLICKED, nullptr);
				ImGui::PopStyleColor(3);
			}
		} else {
			ImGui::SetWindowFontScale(1.45f);
			ImGui::TextUnformatted(hasOutiva ? "Welcome back" : "Continue with Outiva");
			ImGui::SetWindowFontScale(1.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.71f, 0.78f, 1.0f));
			ImGui::TextWrapped(hasOutiva
				? "You are signed in with Outiva. Your points, history and city are saved to your Outiva character."
				: "Sign in with your Outiva account to keep your points, history and city. No separate Battle City password is needed.");
			ImGui::PopStyleColor();
			ImGui::Dummy(ImVec2(0, 14.0f));
			ImGui::PushStyleColor(ImGuiCol_Button, hasOutiva ? ImVec4(0.91f, 0.18f, 0.13f, 1.0f) : ImVec4(0.12f, 0.38f, 0.62f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hasOutiva ? ImVec4(1.0f, 0.25f, 0.18f, 1.0f) : ImVec4(0.16f, 0.47f, 0.75f, 1.0f));
			std::string label = hasOutiva ? (outivaCallsign.empty() ? std::string("Play") : "Play as " + outivaCallsign) : std::string("Continue with Outiva");
			if (ImGui::Button(label.c_str(), ImVec2(-g_loginPad, 46.0f * g_scale)))
				Notify(d, IDTICKETLOGIN, BN_CLICKED, nullptr);
			ImGui::PopStyleColor(2);
			ImGui::Dummy(ImVec2(0, 12.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.52f, 0.60f, 0.68f, 1.0f));
			ImGui::TextWrapped("Battle City never sees your Outiva password. Your name here is your Outiva callsign.");
			ImGui::PopStyleColor();
		}
		ImGui::PopItemWidth();
		if (d->open) {
			ImGui::Dummy(ImVec2(0, 12.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.62f, 0.69f, 1.0f));
			if (DrawLoginLink("Exit game")) Notify(d, IDCANCEL, BN_CLICKED, FindCtl(d, IDCANCEL));
			ImGui::SameLine();
			ImGui::TextUnformatted("Battle City NG");
			ImGui::PopStyleColor();
		}
		ImGui::EndGroup();
		// ImGui scrolls a newly opened child to its first keyboard-nav item. On a
		// phone-sized but tall viewport all content already fits, so keep the brand
		// and account selector visible instead of starting halfway down the form.
		if (!wide && height >= 560.0f) ImGui::SetScrollY(0.0f);
		ImGui::EndChild();
	}
	ImGui::End();
	ImGui::PopStyleColor(6);
	ImGui::PopStyleVar(3);
}

} // namespace

// Draws the front message as a modal popup; returns true when it was answered (and removes it)
static void DrawMessage(float titleH) {
	Message &m = g_messages.front();
	std::string id = m.title + "###message";
	if (!m.opened) { ImGui::OpenPopup(id.c_str()); m.opened = true; }
	ImGuiIO &io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(280.0f * g_scale, 0), ImVec2(420.0f * g_scale, io.DisplaySize.y));
	int answer = 0;
	if (ImGui::BeginPopupModal(id.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
		ImGui::PushTextWrapPos(400.0f * g_scale);
		ImGui::TextUnformatted(m.text.c_str());
		ImGui::PopTextWrapPos();
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		float w = 80.0f * g_scale;
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		if (m.yesNo) {
			if (ImGui::Button("Yes", ImVec2(w, 0))) answer = 1;
			ImGui::SameLine();
			if (ImGui::Button("No", ImVec2(w, 0))) answer = 2;
		} else {
			if (ImGui::Button("OK", ImVec2(w, 0))) answer = 1;
		}
		// Keyboard: Enter = Yes/OK, Escape = No/OK
		if (!answer && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) answer = 1;
		if (!answer && ImGui::IsKeyPressed(ImGuiKey_Escape)) answer = m.yesNo ? 2 : 1;
		if (answer) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	if (answer) {
		std::function<void(int)> done = m.done;
		g_messages.pop_front();
		if (done) done(answer);
	}
}

void BCDialogsRender() {
	if (!g_imguiReady || (!g_active && g_messages.empty())) return;
	Dlg *d = g_active;

	// Draw at native output resolution (not scaled up from the game's logical 1024x768) so text stays sharp
	int lw = 0, lh = 0, ow = 0, oh = 0;
	SDL_RenderGetLogicalSize(g_renderer, &lw, &lh);
	SDL_GetRendererOutputSize(g_renderer, &ow, &oh);
	if (lw <= 0 || lh <= 0) { lw = ow; lh = oh; }
	float k = std::min(ow / (float)lw, oh / (float)lh);                 // how much the game itself is scaled
	float ui = std::max(MIN_UI_SCALE, k) * UI_BOOST;                    // dialogs stay readable when the window is small
	bool modernLogin = d && d->def->id == ID_LOGIN;
	if (modernLogin) {
		ui = std::max(1.15f, std::min(1.5f, std::min(ow / 760.0f, oh / 520.0f) * 1.25f));
	} else if (d) {                                                     // ... but must fit inside the window
		float fitW = ow * 0.97f / (d->def->w * DLU_X);
		float fitH = (oh * 0.97f - 40.0f) / (d->def->h * DLU_Y);
		ui = std::min(ui, std::min(fitW, fitH));
	}
	ui = std::max(MIN_FIT_SCALE, std::floor(ui * 16.0f + 0.5f) / 16.0f);   // quantised so it does not thrash
	if (std::fabs(ui - g_scale) > 0.001f) ApplyScale(ui);
	SDL_RenderSetLogicalSize(g_renderer, 0, 0);

	ImGui_ImplSDLRenderer2_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::GetIO().DisplaySize = ImVec2((float)ow, (float)oh);
	ImGui::GetIO().DisplayFramebufferScale = ImVec2(1, 1);
	ImGui::NewFrame();
#ifdef BC_FRAME_TIMING
	{
		static int n = 0;
		if (++n % 60 == 0) {
			int ww = 0, wh = 0; SDL_GetWindowSize(g_window, &ww, &wh);
			int ow = 0, oh = 0; SDL_GetRendererOutputSize(g_renderer, &ow, &oh);
			ImGuiIO &dio = ImGui::GetIO();
			printf("imgui: display %.0fx%.0f logical %dx%d window %dx%d output %dx%d mouse %.0f,%.0f down=%d wantCapture=%d wantText=%d textActive=%d anyActive=%d\n",
			       dio.DisplaySize.x, dio.DisplaySize.y, lw, lh, ww, wh, ow, oh, dio.MousePos.x, dio.MousePos.y, (int)dio.MouseDown[0], (int)dio.WantCaptureMouse, (int)dio.WantTextInput, (int)SDL_IsTextInputActive(), (int)ImGui::IsAnyItemActive());
		}
	}
#endif

	float titleH = ImGui::GetFrameHeight();
	bool keepOpen = true;
	if (modernLogin) {
		DrawModernLogin(d);
	} else if (d) {
	ImVec2 size(d->def->w * DLU_X * g_scale, d->def->h * DLU_Y * g_scale + titleH);
	ImGui::SetNextWindowPos(ImVec2((ImGui::GetIO().DisplaySize.x - size.x) / 2, (ImGui::GetIO().DisplaySize.y - size.y) / 2), ImGuiCond_Always);
	ImGui::SetNextWindowSize(size, ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
	                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	if (ImGui::Begin((std::string(d->def->caption) + "###dlg").c_str(), d->def->sysmenu ? &keepOpen : nullptr, flags)) {
		for (int i = 0; i < (int)d->ctls.size() && d->open; i++) DrawControl(d, d->ctls[i], i, titleH);
	}
	ImGui::End();
	ImGui::PopStyleVar();
	if (!keepOpen && d->open && d->proc) d->proc(d, WM_CLOSE, 0, 0);   // title-bar close button
	}   // if (d)

	if (!g_messages.empty()) DrawMessage(titleH);

	ImGui::Render();
	ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), g_renderer);
	SDL_RenderSetLogicalSize(g_renderer, lw, lh);     // back to the game's logical resolution
}
