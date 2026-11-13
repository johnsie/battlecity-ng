// Battle City - keyboard shortcuts and the shortcut editor. See CHotkeys.h.
#include "CHotkeys.h"
#include "platform/dialogs.h"
#include <SDL.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <vector>

std::string BCLoadText(const char *key);
void BCSaveText(const char *key, const std::string &text);

namespace {

struct Def {
	const char *id;
	const char *name;
	const char *desc;
	int category;
	bool held;            // read every frame (movement, firing ...) rather than triggered by a key press
	int keys[2];          // default bindings (SDL scancodes, 0 = none)
};

const Def DEFS[HK_COUNT] = {
	{ "up",      "Drive forward",   "Drives your tank forward. Hold it down to keep moving.", HC_MOVE, true, { SDL_SCANCODE_UP, 0 } },
	{ "down",    "Reverse",         "Backs your tank up.", HC_MOVE, true, { SDL_SCANCODE_DOWN, 0 } },
	{ "left",    "Turn left",       "Turns your tank to the left.", HC_MOVE, true, { SDL_SCANCODE_LEFT, 0 } },
	{ "right",   "Turn right",      "Turns your tank to the right.", HC_MOVE, true, { SDL_SCANCODE_RIGHT, 0 } },
	{ "fire",    "Fire",            "Shoots your laser or Cougar missile. A missile only fires while you stand still.", HC_COMBAT, true, { SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT } },
	{ "flare",   "Fire flares",     "Throws flares behind you to confuse missiles. Needs a Flare item.", HC_COMBAT, true, { SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL } },
	{ "center",  "Look around",     "Hold it and use the drive and turn keys to scroll the view. Let go to snap back to your tank.", HC_MOVE, true, { SDL_SCANCODE_TAB, 0 } },
	{ "pickup",  "Pick up",         "Picks up the item under your tank.", HC_ITEMS, false, { SDL_SCANCODE_U, SDL_SCANCODE_F6 } },
	{ "drop",    "Drop item",       "Drops the item selected in your inventory.", HC_ITEMS, false, { SDL_SCANCODE_D, 0 } },
	{ "bomb",    "Drop bomb",       "Arms a bomb from your inventory and drops it where you stand.", HC_ITEMS, false, { SDL_SCANCODE_B, 0 } },
	{ "orb",     "Drop orb",        "Drops an orb from your inventory.", HC_ITEMS, false, { SDL_SCANCODE_O, 0 } },
	{ "medkit",  "Use medkit",      "Restores your tank to full health.", HC_ITEMS, false, { SDL_SCANCODE_H, 0 } },
	{ "cloak",   "Use cloak",       "Hides you from enemy radar.", HC_ITEMS, false, { SDL_SCANCODE_C, 0 } },
	{ "info",    "Info panel",      "Shows or hides the info panel.", HC_INTERFACE, false, { SDL_SCANCODE_I, 0 } },
	{ "points",  "Points panel",    "Shows or hides the points panel.", HC_INTERFACE, false, { SDL_SCANCODE_P, SDL_SCANCODE_F1 } },
	{ "map",     "Minimap",         "Opens and closes the minimap. Your tank stops while it is open.", HC_INTERFACE, false, { SDL_SCANCODE_M, 0 } },
	{ "pointers","Pointers",        "Arrows at the screen edge that point to your base, crates, raids and fights.", HC_INTERFACE, false, { SDL_SCANCODE_N, 0 } },
	{ "chat",    "Chat",            "Opens the chat line. Enter sends the message, Esc cancels it.", HC_INTERFACE, false, { SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER } },
	{ "reload",  "Reload graphics", "Reloads the game's pictures if something looks broken.", HC_INTERFACE, false, { SDL_SCANCODE_RIGHTBRACKET, 0 } },
	{ "voice",   "Voice chat on/off","Turns voice chat on or off.", HC_VOICE, false, { SDL_SCANCODE_V, 0 } },
	{ "ptt",     "Push to talk",    "Hold it to talk when voice chat is set to push to talk.", HC_VOICE, true, { SDL_SCANCODE_GRAVE, 0 } },
	{ "buildmenu","Build menu",     "Opens the build menu at the pointer (mayors). Press it again to close the menu or put a building back.", HC_BUILD, false, { SDL_SCANCODE_F, 0 } },
	{ "demolish","Demolish",        "Puts the wrecking cursor on the pointer; click a building to demolish it. Press it again to put it away.", HC_BUILD, false, { SDL_SCANCODE_X, 0 } },
	{ "b0", "Build Hospital", "Puts a Hospital on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b1", "Build House", "Puts a House on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b2", "Build Bazooka Research", "Puts a Bazooka Research on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b3", "Build Bazooka Factory", "Puts a Bazooka Factory on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b4", "Build Turret Research", "Puts a Turret Research on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b5", "Build Turret Factory", "Puts a Turret Factory on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b6", "Build Cloak Research", "Puts a Cloak Research on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b7", "Build Cloak Factory", "Puts a Cloak Factory on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b8", "Build MedKit Research", "Puts a MedKit Research on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b9", "Build MedKit Factory", "Puts a MedKit Factory on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b10", "Build Plasma Turret Research", "Puts a Plasma Turret Research on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b11", "Build Plasma Turret Factory", "Puts a Plasma Turret Factory on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b12", "Build Mine Research", "Puts a Mine Research on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b13", "Build Mine Factory", "Puts a Mine Factory on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b14", "Build Orb Research", "Puts a Orb Research on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b15", "Build Orb Factory", "Puts a Orb Factory on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b16", "Build Time Bomb Research", "Puts a Time Bomb Research on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b17", "Build Time Bomb Factory", "Puts a Time Bomb Factory on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b18", "Build Sleeper Research", "Puts a Sleeper Research on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b19", "Build Sleeper Factory", "Puts a Sleeper Factory on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b20", "Build Wall Research", "Puts a Wall Research on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b21", "Build Wall Factory", "Puts a Wall Factory on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b22", "Build DFG Research", "Puts a DFG Research on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b23", "Build DFG Factory", "Puts a DFG Factory on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b24", "Build Flare Gun Research", "Puts a Flare Gun Research on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
	{ "b25", "Build Flare Gun Factory", "Puts a Flare Gun Factory on the cursor; click to place it. Mayors only, and only what you can build.", HC_BUILD, false, { 0, 0 } },
};

const char *const CATEGORY_NAMES[HC_COUNT] = { "MOVEMENT", "COMBAT", "ITEMS", "INTERFACE", "VOICE", "BUILDING" };
const ImU32 CATEGORY_COLORS[HC_COUNT] = {
	IM_COL32(86, 156, 236, 255), IM_COL32(232, 90, 80, 255), IM_COL32(96, 196, 120, 255),
	IM_COL32(238, 186, 70, 255), IM_COL32(176, 120, 230, 255), IM_COL32(64, 200, 204, 255),
};

float g_s = 1.0f;     // pixels per logical pixel for the frame being drawn

bool Reserved(int sc) { return sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0; }   // 1-0 pick the tank and are not rebindable

ImU32 Alpha(ImU32 c, int a) { return (c & 0x00FFFFFFu) | ((ImU32)a << 24); }

// ---- icons: drawn with lines and shapes so they stay sharp at any size ----------------------------------------------

void Arc(ImDrawList *dl, ImVec2 c, float r, float a0, float a1, ImU32 col, float th) {
	const int n = 18;
	ImVec2 pts[n + 1];
	for (int i = 0; i <= n; i++) {
		float a = a0 + (a1 - a0) * i / n;
		pts[i] = ImVec2(c.x + cosf(a) * r, c.y + sinf(a) * r);
	}
	dl->AddPolyline(pts, n + 1, col, 0, th);
}

ImVec2 Pt(ImVec2 c, float r, float x, float y) { return ImVec2(c.x + x * r, c.y + y * r); }

void DrawIcon(ImDrawList *dl, int a, ImVec2 c, float r, ImU32 col) {
	const float PI = 3.14159265f;
	float th = std::max(1.5f, r * 0.16f);
	if (a >= HK_BUILD0) {               // buildings: hospital (cross), house, then a flask for each research and a factory for each factory
		int n = a - HK_BUILD0;
		ImU32 wall = Alpha(col, 90);
		if (n == 0 || n == 1) {
			dl->AddRectFilled(Pt(c, r, -0.7f, -0.2f), Pt(c, r, 0.7f, 0.85f), wall);
			dl->AddRect(Pt(c, r, -0.7f, -0.2f), Pt(c, r, 0.7f, 0.85f), col, 0, 0, th);
			dl->AddTriangleFilled(Pt(c, r, -0.9f, -0.15f), Pt(c, r, 0.9f, -0.15f), Pt(c, r, 0.0f, -0.95f), col);
			if (n == 0) {
				dl->AddRectFilled(Pt(c, r, -0.12f, 0.15f), Pt(c, r, 0.12f, 0.75f), IM_COL32(230, 60, 60, 255));
				dl->AddRectFilled(Pt(c, r, -0.35f, 0.38f), Pt(c, r, 0.35f, 0.54f), IM_COL32(230, 60, 60, 255));
			} else {
				dl->AddRectFilled(Pt(c, r, -0.15f, 0.35f), Pt(c, r, 0.15f, 0.85f), col);
			}
		} else if (n % 2 == 0) {            // research: a flask
			dl->AddRectFilled(Pt(c, r, -0.2f, -0.9f), Pt(c, r, 0.2f, -0.3f), col);
			dl->AddTriangleFilled(Pt(c, r, -0.2f, -0.3f), Pt(c, r, 0.2f, -0.3f), Pt(c, r, 0.85f, 0.85f), wall);
			dl->AddTriangleFilled(Pt(c, r, -0.2f, -0.3f), Pt(c, r, -0.85f, 0.85f), Pt(c, r, 0.85f, 0.85f), wall);
			dl->AddLine(Pt(c, r, -0.2f, -0.3f), Pt(c, r, -0.85f, 0.85f), col, th);
			dl->AddLine(Pt(c, r, 0.2f, -0.3f), Pt(c, r, 0.85f, 0.85f), col, th);
			dl->AddLine(Pt(c, r, -0.85f, 0.85f), Pt(c, r, 0.85f, 0.85f), col, th);
		} else {                            // factory: sawtooth roof and a chimney
			dl->AddRectFilled(Pt(c, r, -0.9f, -0.1f), Pt(c, r, 0.9f, 0.85f), wall);
			dl->AddRect(Pt(c, r, -0.9f, -0.1f), Pt(c, r, 0.9f, 0.85f), col, 0, 0, th);
			dl->AddTriangleFilled(Pt(c, r, -0.9f, -0.1f), Pt(c, r, -0.3f, -0.1f), Pt(c, r, -0.9f, -0.6f), col);
			dl->AddTriangleFilled(Pt(c, r, -0.3f, -0.1f), Pt(c, r, 0.3f, -0.1f), Pt(c, r, -0.3f, -0.6f), col);
			dl->AddRectFilled(Pt(c, r, 0.5f, -0.85f), Pt(c, r, 0.75f, -0.1f), col);
		}
		return;
	}
	switch (a) {
		case HK_BUILDMENU: {                // the menu: a list of rows with a house on top
			for (int i = 0; i < 3; i++) {
				float y = -0.5f + i * 0.55f;
				dl->AddRectFilled(Pt(c, r, -0.9f, y - 0.18f), Pt(c, r, 0.9f, y + 0.18f), i == 0 ? col : Alpha(col, 110), 2.0f);
			}
			break;
		}
		case HK_DEMOLISH: {                 // a building with a red cross over it
			dl->AddRectFilled(Pt(c, r, -0.7f, -0.3f), Pt(c, r, 0.7f, 0.85f), Alpha(col, 90));
			dl->AddRect(Pt(c, r, -0.7f, -0.3f), Pt(c, r, 0.7f, 0.85f), col, 0, 0, th);
			dl->AddLine(Pt(c, r, -0.85f, -0.85f), Pt(c, r, 0.85f, 0.85f), IM_COL32(236, 64, 56, 255), th * 1.8f);
			dl->AddLine(Pt(c, r, 0.85f, -0.85f), Pt(c, r, -0.85f, 0.85f), IM_COL32(236, 64, 56, 255), th * 1.8f);
			break;
		}
		case HK_UP: case HK_DOWN: case HK_LEFT: case HK_RIGHT: {
			float fx = a == HK_LEFT ? -1.f : a == HK_RIGHT ? 1.f : 0.f;
			float fy = a == HK_UP ? -1.f : a == HK_DOWN ? 1.f : 0.f;
			float nx = -fy, ny = fx;
			ImVec2 tip(c.x + fx * r, c.y + fy * r);
			ImVec2 b1(c.x + fx * 0.05f * r + nx * 0.85f * r, c.y + fy * 0.05f * r + ny * 0.85f * r);
			ImVec2 b2(c.x + fx * 0.05f * r - nx * 0.85f * r, c.y + fy * 0.05f * r - ny * 0.85f * r);
			dl->AddTriangleFilled(tip, b1, b2, col);
			dl->AddLine(ImVec2(c.x - fx * 0.9f * r, c.y - fy * 0.9f * r), ImVec2(c.x + fx * 0.1f * r, c.y + fy * 0.1f * r), col, th * 2.0f);
			break;
		}
		case HK_FIRE: {
			dl->AddCircle(c, r * 0.6f, col, 24, th);
			for (int i = 0; i < 4; i++) {
				float ax = (i == 0) - (i == 2), ay = (i == 1) - (i == 3);
				dl->AddLine(Pt(c, r, ax * 0.3f, ay * 0.3f), Pt(c, r, ax * 1.0f, ay * 1.0f), col, th);
			}
			dl->AddCircleFilled(c, r * 0.14f, col);
			break;
		}
		case HK_FLARE: {
			for (int i = 0; i < 8; i++) {
				float an = i * PI / 4;
				float r0 = 0.4f, r1 = (i % 2) ? 0.75f : 1.0f;
				dl->AddLine(Pt(c, r, cosf(an) * r0, sinf(an) * r0), Pt(c, r, cosf(an) * r1, sinf(an) * r1), IM_COL32(255, 176, 64, 255), th);
			}
			dl->AddCircleFilled(c, r * 0.3f, IM_COL32(255, 230, 140, 255));
			break;
		}
		case HK_CENTER: {
			for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) {
				ImVec2 k = Pt(c, r, sx * 0.9f, sy * 0.9f);
				dl->AddLine(k, Pt(c, r, sx * 0.9f, sy * 0.4f), col, th);
				dl->AddLine(k, Pt(c, r, sx * 0.4f, sy * 0.9f), col, th);
			}
			dl->AddCircleFilled(c, r * 0.2f, col);
			break;
		}
		case HK_PICKUP: case HK_DROP: {
			ImVec2 pts[4] = { Pt(c, r, -0.85f, 0.05f), Pt(c, r, -0.85f, 0.85f), Pt(c, r, 0.85f, 0.85f), Pt(c, r, 0.85f, 0.05f) };
			dl->AddPolyline(pts, 4, col, 0, th);
			if (a == HK_PICKUP) {
				dl->AddLine(Pt(c, r, 0, 0.5f), Pt(c, r, 0, -0.55f), col, th * 1.6f);
				dl->AddTriangleFilled(Pt(c, r, 0, -1.0f), Pt(c, r, -0.45f, -0.45f), Pt(c, r, 0.45f, -0.45f), col);
			} else {
				dl->AddLine(Pt(c, r, 0, -0.95f), Pt(c, r, 0, 0.0f), col, th * 1.6f);
				dl->AddTriangleFilled(Pt(c, r, 0, 0.55f), Pt(c, r, -0.45f, 0.0f), Pt(c, r, 0.45f, 0.0f), col);
			}
			break;
		}
		case HK_BOMB: {
			ImVec2 bc = Pt(c, r, -0.1f, 0.2f);
			dl->AddCircleFilled(bc, r * 0.68f, IM_COL32(48, 50, 60, 255), 24);
			dl->AddCircle(bc, r * 0.68f, col, 24, th);
			dl->AddCircleFilled(Pt(c, r, -0.3f, -0.05f), r * 0.14f, IM_COL32(255, 255, 255, 120));
			dl->AddLine(Pt(c, r, 0.3f, -0.35f), Pt(c, r, 0.65f, -0.8f), col, th);
			dl->AddCircleFilled(Pt(c, r, 0.7f, -0.85f), r * 0.2f, IM_COL32(255, 200, 60, 255));
			break;
		}
		case HK_ORB: {
			dl->AddCircleFilled(c, r * 0.82f, Alpha(col, 200), 28);
			dl->AddCircle(c, r * 0.82f, col, 28, th);
			dl->AddCircleFilled(Pt(c, r, -0.28f, -0.28f), r * 0.22f, IM_COL32(255, 255, 255, 190));
			break;
		}
		case HK_MEDKIT: {
			dl->AddRectFilled(Pt(c, r, -0.9f, -0.75f), Pt(c, r, 0.9f, 0.75f), IM_COL32(240, 240, 240, 255), r * 0.2f);
			dl->AddRectFilled(Pt(c, r, -0.18f, -0.55f), Pt(c, r, 0.18f, 0.55f), IM_COL32(214, 48, 48, 255));
			dl->AddRectFilled(Pt(c, r, -0.55f, -0.18f), Pt(c, r, 0.55f, 0.18f), IM_COL32(214, 48, 48, 255));
			break;
		}
		case HK_CLOAK: {
			for (int i = 0; i < 8; i++) Arc(dl, c, r * 0.85f, i * PI / 4, i * PI / 4 + PI / 7, col, th * 1.2f);
			dl->AddCircleFilled(c, r * 0.4f, Alpha(col, 90), 20);
			break;
		}
		case HK_INFO: {
			dl->AddCircle(c, r * 0.9f, col, 28, th);
			ImVec2 ts = ImGui::CalcTextSize("i");
			dl->AddText(ImVec2(c.x - ts.x / 2, c.y - ts.y / 2), col, "i");
			break;
		}
		case HK_POINTS: {
			ImVec2 pts[10];
			for (int i = 0; i < 10; i++) {
				float an = -PI / 2 + i * PI / 5, rr = (i % 2) ? 0.42f : 1.0f;
				pts[i] = Pt(c, r, cosf(an) * rr, sinf(an) * rr);
			}
			for (int i = 0; i < 10; i++) dl->AddTriangleFilled(c, pts[i], pts[(i + 1) % 10], col);
			break;
		}
		case HK_MAP: {
			ImVec2 p0 = Pt(c, r, -0.9f, -0.8f), p1 = Pt(c, r, 0.9f, 0.8f);
			dl->AddRect(p0, p1, col, 2.0f, 0, th);
			dl->AddLine(Pt(c, r, -0.3f, -0.8f), Pt(c, r, -0.3f, 0.8f), Alpha(col, 140), 1.0f);
			dl->AddLine(Pt(c, r, 0.3f, -0.8f), Pt(c, r, 0.3f, 0.8f), Alpha(col, 140), 1.0f);
			dl->AddLine(Pt(c, r, -0.9f, -0.27f), Pt(c, r, 0.9f, -0.27f), Alpha(col, 140), 1.0f);
			dl->AddLine(Pt(c, r, -0.9f, 0.27f), Pt(c, r, 0.9f, 0.27f), Alpha(col, 140), 1.0f);
			dl->AddCircleFilled(Pt(c, r, 0.3f, -0.27f), r * 0.2f, IM_COL32(236, 80, 70, 255));
			break;
		}
		case HK_POINTERS: {
			dl->AddCircle(c, r * 0.92f, Alpha(col, 160), 28, th * 0.8f);
			float an = -PI / 4, ca = cosf(an), sa = sinf(an);
			auto rot = [&](float x, float y) { return Pt(c, r, x * ca - y * sa, x * sa + y * ca + 0.0f); };
			// a dart pointing up-right: rotate the "up" dart by 45 degrees clockwise
			float cw = PI / 4; ca = cosf(cw); sa = sinf(cw);
			ImVec2 tip = rot(0, -0.75f), l = rot(-0.4f, 0.5f), m = rot(0, 0.2f), rr = rot(0.4f, 0.5f);
			dl->AddTriangleFilled(tip, l, m, col);
			dl->AddTriangleFilled(tip, rr, m, col);
			break;
		}
		case HK_CHAT: {
			dl->AddRectFilled(Pt(c, r, -0.95f, -0.75f), Pt(c, r, 0.95f, 0.4f), Alpha(col, 70), r * 0.3f);
			dl->AddRect(Pt(c, r, -0.95f, -0.75f), Pt(c, r, 0.95f, 0.4f), col, r * 0.3f, 0, th);
			dl->AddTriangleFilled(Pt(c, r, -0.45f, 0.38f), Pt(c, r, -0.75f, 0.9f), Pt(c, r, 0.0f, 0.38f), col);
			for (int i = -1; i <= 1; i++) dl->AddCircleFilled(Pt(c, r, i * 0.4f, -0.17f), r * 0.1f, col);
			break;
		}
		case HK_RELOAD: {
			float a0 = 0.35f * PI, a1 = 1.95f * PI, rr = r * 0.72f;
			Arc(dl, c, rr, a0, a1, col, th * 1.3f);
			ImVec2 e(c.x + cosf(a1) * rr, c.y + sinf(a1) * rr);
			ImVec2 tg(-sinf(a1), cosf(a1)), nm(cosf(a1), sinf(a1));
			ImVec2 tip(e.x + tg.x * r * 0.45f, e.y + tg.y * r * 0.45f);
			dl->AddTriangleFilled(tip, ImVec2(e.x + nm.x * r * 0.38f, e.y + nm.y * r * 0.38f), ImVec2(e.x - nm.x * r * 0.38f, e.y - nm.y * r * 0.38f), col);
			break;
		}
		case HK_VOICE: case HK_PTT: {
			dl->AddRectFilled(Pt(c, r, -0.28f, -0.95f), Pt(c, r, 0.28f, 0.15f), col, r * 0.28f);
			Arc(dl, Pt(c, r, 0, 0.0f), r * 0.55f, 0.0f, PI, col, th);
			dl->AddLine(Pt(c, r, 0, 0.55f), Pt(c, r, 0, 0.9f), col, th);
			dl->AddLine(Pt(c, r, -0.3f, 0.9f), Pt(c, r, 0.3f, 0.9f), col, th);
			if (a == HK_VOICE) {
				Arc(dl, Pt(c, r, 0.15f, -0.4f), r * 0.55f, -0.6f, 0.6f, Alpha(col, 170), th * 0.8f);
			} else {
				dl->AddRect(Pt(c, r, 0.45f, 0.35f), Pt(c, r, 1.0f, 0.95f), col, 2.0f, 0, th * 0.8f);
				dl->AddCircleFilled(Pt(c, r, 0.725f, 0.65f), r * 0.1f, col);
			}
			break;
		}
	}
}

// ---- the keyboard picture -------------------------------------------------------------------------------------------

struct KeyDef { int sc; const char *label; float x, y, w; };

const std::vector<KeyDef> &Keyboard() {
	static std::vector<KeyDef> keys;
	if (!keys.empty()) return keys;
	auto row = [&](float y, float x, std::initializer_list<KeyDef> list) {
		for (KeyDef k : list) { k.x = x; k.y = y; keys.push_back(k); x += k.w; }
	};
	#define K(sc, label, w) KeyDef{ SDL_SCANCODE_##sc, label, 0, 0, w }
	row(0, 0, { K(F1,"F1",1.25f), K(F2,"F2",1.25f), K(F3,"F3",1.25f), K(F4,"F4",1.25f), K(F5,"F5",1.25f), K(F6,"F6",1.25f),
	            K(F7,"F7",1.25f), K(F8,"F8",1.25f), K(F9,"F9",1.25f), K(F10,"F10",1.25f), K(F11,"F11",1.25f), K(F12,"F12",1.25f) });
	row(1.2f, 0, { K(GRAVE,"`",1), K(1,"1",1), K(2,"2",1), K(3,"3",1), K(4,"4",1), K(5,"5",1), K(6,"6",1), K(7,"7",1), K(8,"8",1),
	               K(9,"9",1), K(0,"0",1), K(MINUS,"-",1), K(EQUALS,"=",1), K(BACKSPACE,"Bksp",2) });
	row(2.2f, 0, { K(TAB,"Tab",1.5f), K(Q,"Q",1), K(W,"W",1), K(E,"E",1), K(R,"R",1), K(T,"T",1), K(Y,"Y",1), K(U,"U",1), K(I,"I",1),
	               K(O,"O",1), K(P,"P",1), K(LEFTBRACKET,"[",1), K(RIGHTBRACKET,"]",1), K(BACKSLASH,"\\",1.5f) });
	row(3.2f, 0, { K(CAPSLOCK,"Caps",1.75f), K(A,"A",1), K(S,"S",1), K(D,"D",1), K(F,"F",1), K(G,"G",1), K(H,"H",1), K(J,"J",1), K(K,"K",1),
	               K(L,"L",1), K(SEMICOLON,";",1), K(APOSTROPHE,"'",1), K(RETURN,"Enter",2.25f) });
	row(4.2f, 0, { K(LSHIFT,"Shift",2.25f), K(Z,"Z",1), K(X,"X",1), K(C,"C",1), K(V,"V",1), K(B,"B",1), K(N,"N",1), K(M,"M",1),
	               K(COMMA,",",1), K(PERIOD,".",1), K(SLASH,"/",1), K(RSHIFT,"Shift",2.75f) });
	row(5.2f, 0, { K(LCTRL,"Ctrl",1.5f), K(LALT,"Alt",1.5f), K(SPACE,"Space",9), K(RALT,"Alt",1.5f), K(RCTRL,"Ctrl",1.5f) });
	#undef K
	keys.push_back(KeyDef{ SDL_SCANCODE_UP, "Up", 16.5f, 4.2f, 1 });
	keys.push_back(KeyDef{ SDL_SCANCODE_LEFT, "Lt", 15.5f, 5.2f, 1 });
	keys.push_back(KeyDef{ SDL_SCANCODE_DOWN, "Dn", 16.5f, 5.2f, 1 });
	keys.push_back(KeyDef{ SDL_SCANCODE_RIGHT, "Rt", 17.5f, 5.2f, 1 });
	return keys;
}

}  // namespace

// ---- bindings -------------------------------------------------------------------------------------------------------

CHotkeys &CHotkeys::Get() {
	static CHotkeys instance;
	return instance;
}

CHotkeys::CHotkeys() : loaded(false), open(false), selected(HK_FIRE), listenAct(-1), listenSlot(0), hoverAct(-1) {
	SetDefaults(live);
	SetDefaults(edit);
	Load();
}

void CHotkeys::SetDefaults(int (*to)[2]) {
	for (int a = 0; a < HK_COUNT; a++) { to[a][0] = DEFS[a].keys[0]; to[a][1] = DEFS[a].keys[1]; }
}

void CHotkeys::Load() {
	loaded = true;
	std::istringstream in(BCLoadText("hotkeys"));
	std::string line;
	while (std::getline(in, line)) {
		size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string id = line.substr(0, eq);
		int a = -1;
		for (int i = 0; i < HK_COUNT; i++) if (id == DEFS[i].id) a = i;
		if (a < 0) continue;
		int k0 = 0, k1 = 0;
		if (sscanf(line.c_str() + eq + 1, "%d,%d", &k0, &k1) < 1) continue;
		auto valid = [](int k) { return k > 0 && k < SDL_NUM_SCANCODES && !Reserved(k) ? k : 0; };
		live[a][0] = valid(k0);
		live[a][1] = valid(k1);
	}
	memcpy(edit, live, sizeof(edit));
}

void CHotkeys::Save() {
	std::ostringstream out;
	for (int a = 0; a < HK_COUNT; a++) out << DEFS[a].id << "=" << live[a][0] << "," << live[a][1] << "\n";
	BCSaveText("hotkeys", out.str());
}

bool CHotkeys::Down(int action, const unsigned char *state) const {
	if (!state || action < 0 || action >= HK_COUNT) return false;
	for (int s = 0; s < 2; s++) {
		int sc = live[action][s];
		if (sc > 0 && sc < SDL_NUM_SCANCODES && state[sc]) return true;
	}
	return false;
}

int CHotkeys::ActionForKey(int scancode) const {
	if (scancode <= 0) return -1;
	for (int a = 0; a < HK_COUNT; a++) {
		if (DEFS[a].held) continue;
		if (live[a][0] == scancode || live[a][1] == scancode) return a;
	}
	return -1;
}

const char *CHotkeys::Name(int action) { return (action >= 0 && action < HK_COUNT) ? DEFS[action].name : ""; }

std::string CHotkeys::KeyName(int sc) {
	switch (sc) {
		case 0: return "";
		case SDL_SCANCODE_LSHIFT: return "L-Shift";
		case SDL_SCANCODE_RSHIFT: return "R-Shift";
		case SDL_SCANCODE_LCTRL: return "L-Ctrl";
		case SDL_SCANCODE_RCTRL: return "R-Ctrl";
		case SDL_SCANCODE_LALT: return "L-Alt";
		case SDL_SCANCODE_RALT: return "R-Alt";
		case SDL_SCANCODE_RETURN: return "Enter";
		case SDL_SCANCODE_KP_ENTER: return "Num Enter";
		case SDL_SCANCODE_GRAVE: return "` (left of 1)";
		case SDL_SCANCODE_BACKSLASH: return "\\";
		default: break;
	}
	const char *n = SDL_GetScancodeName((SDL_Scancode)sc);
	return (n && *n) ? n : "?";
}

std::string CHotkeys::KeyText(int action) const {
	if (action < 0 || action >= HK_COUNT) return "";
	std::string t = KeyName(live[action][0]);
	std::string u = KeyName(live[action][1]);
	if (!u.empty()) t += t.empty() ? u : " / " + u;
	return t.empty() ? "unbound" : t;
}

bool CHotkeys::Differs(int a) const {
	return edit[a][0] != DEFS[a].keys[0] || edit[a][1] != DEFS[a].keys[1];
}

void CHotkeys::SetPreset(int preset) {
	SetDefaults(edit);
	if (preset == 1) {   // WASD to drive, space to fire; the drop key moves to X because D now turns
		edit[HK_UP][0] = SDL_SCANCODE_W;    edit[HK_UP][1] = SDL_SCANCODE_UP;
		edit[HK_DOWN][0] = SDL_SCANCODE_S;  edit[HK_DOWN][1] = SDL_SCANCODE_DOWN;
		edit[HK_LEFT][0] = SDL_SCANCODE_A;  edit[HK_LEFT][1] = SDL_SCANCODE_LEFT;
		edit[HK_RIGHT][0] = SDL_SCANCODE_D; edit[HK_RIGHT][1] = SDL_SCANCODE_RIGHT;
		edit[HK_FIRE][0] = SDL_SCANCODE_SPACE; edit[HK_FIRE][1] = SDL_SCANCODE_LSHIFT;
		edit[HK_DROP][0] = SDL_SCANCODE_Q;
	}
}

void CHotkeys::Assign(int action, int slot, int sc) {
	note.clear();
	for (int a = 0; a < HK_COUNT; a++) for (int s = 0; s < 2; s++) {
		if (edit[a][s] == sc && !(a == action && s == slot)) {
			edit[a][s] = 0;
			if (a != action) note = KeyName(sc) + " was taken from \"" + DEFS[a].name + "\" and is now on \"" + DEFS[action].name + "\".";
		}
	}
	edit[action][slot] = sc;
}

// ---- the editor -----------------------------------------------------------------------------------------------------

void CHotkeys::OpenEditor(std::function<void()> closed) {
	memcpy(edit, live, sizeof(edit));
	onClose = closed;
	open = true;
	listenAct = -1;
	hoverAct = -1;
	note.clear();
	BCDialogsShowCustom([this](float s) { return Draw(s); }, 960.0f, 610.0f);
}

namespace {

// A key cap button. mode: 0 normal, 1 waiting for a key, 2 empty
bool Keycap(const char *id, ImVec2 p, ImVec2 size, const std::string &label, int mode, ImU32 accent, bool changed) {
	ImDrawList *dl = ImGui::GetWindowDrawList();
	ImGui::SetCursorScreenPos(p);
	bool clicked = ImGui::InvisibleButton(id, size);
	bool hov = ImGui::IsItemHovered();
	float r = 4.0f * g_s;
	ImU32 face = hov ? IM_COL32(86, 92, 108, 255) : IM_COL32(66, 71, 84, 255);
	dl->AddRectFilled(ImVec2(p.x, p.y + 2 * g_s), ImVec2(p.x + size.x, p.y + size.y + 1 * g_s), IM_COL32(20, 22, 28, 255), r);
	dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y - 1 * g_s), face, r);
	ImU32 border = IM_COL32(110, 116, 132, 255);
	std::string text = label;
	ImU32 tcol = changed ? IM_COL32(255, 214, 96, 255) : IM_COL32(236, 238, 244, 255);
	if (mode == 1) {
		float pulse = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 7.0f);
		border = Alpha(accent, (int)(140 + 115 * pulse));
		text = "Press a key...";
		tcol = IM_COL32(255, 255, 255, 255);
	} else if (mode == 2) {
		text = "unbound";
		tcol = IM_COL32(130, 136, 150, 255);
	}
	dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y - 1 * g_s), border, r, 0, mode == 1 ? 2.0f * g_s : 1.0f);
	ImVec2 ts = ImGui::CalcTextSize(text.c_str());
	dl->PushClipRect(p, ImVec2(p.x + size.x, p.y + size.y), true);
	dl->AddText(ImVec2(p.x + (size.x - ts.x) / 2, p.y + (size.y - ts.y) / 2 - 1 * g_s), tcol, text.c_str());
	dl->PopClipRect();
	return clicked;
}

}  // namespace

bool CHotkeys::Draw(float s) {
	g_s = s;
	bool keepOpen = true, save = false;

	// keys pressed since the last frame
	int key = BCTakeKeyPress();
	if (key) {
		if (listenAct >= 0) {
			if (key == SDL_SCANCODE_ESCAPE) { listenAct = -1; }
			else if (key == SDL_SCANCODE_BACKSPACE || key == SDL_SCANCODE_DELETE) { edit[listenAct][listenSlot] = 0; note.clear(); listenAct = -1; }
			else if (Reserved(key)) { note = "Keys 1 to 0 pick your tank and cannot be changed."; listenAct = -1; }
			else { Assign(listenAct, listenSlot, key); selected = listenAct; listenAct = -1; }
		} else if (key == SDL_SCANCODE_ESCAPE) {
			keepOpen = false;
		}
	}

	ImGuiIO &io = ImGui::GetIO();
	ImVec2 size(940.0f * s, 590.0f * s);
	ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - size.x) / 2, (io.DisplaySize.y - size.y) / 2), ImGuiCond_Always);
	ImGui::SetNextWindowSize(size, ImGuiCond_Always);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
	                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	// a dark page of its own: the key caps and icons are drawn for a dark background
	const struct { ImGuiCol id; ImU32 col; } theme[] = {
		{ ImGuiCol_WindowBg, IM_COL32(22, 25, 32, 255) }, { ImGuiCol_ChildBg, IM_COL32(30, 33, 42, 255) },
		{ ImGuiCol_Text, IM_COL32(228, 231, 238, 255) }, { ImGuiCol_TextDisabled, IM_COL32(140, 146, 162, 255) },
		{ ImGuiCol_Border, IM_COL32(62, 68, 86, 255) }, { ImGuiCol_TitleBg, IM_COL32(34, 38, 50, 255) },
		{ ImGuiCol_TitleBgActive, IM_COL32(40, 46, 62, 255) }, { ImGuiCol_Button, IM_COL32(52, 60, 82, 255) },
		{ ImGuiCol_ButtonHovered, IM_COL32(72, 86, 122, 255) }, { ImGuiCol_ButtonActive, IM_COL32(92, 110, 160, 255) },
		{ ImGuiCol_ScrollbarBg, IM_COL32(24, 27, 34, 255) }, { ImGuiCol_Separator, IM_COL32(62, 68, 86, 255) },
	};
	for (const auto &t : theme) ImGui::PushStyleColor(t.id, t.col);
	bool shown = ImGui::Begin("Shortcut Editor###hotkeys", nullptr, flags);
	if (shown) {
		float footerH = 40.0f * s;
		float bodyH = ImGui::GetContentRegionAvail().y - footerH;
		float leftW = 440.0f * s;
		hoverAct = -1;

		// ---- left: every command with its two keys ----
		ImGui::BeginChild("##list", ImVec2(leftW, bodyH), true);
		{
			ImDrawList *dl = ImGui::GetWindowDrawList();
			for (int cat = 0; cat < HC_COUNT; cat++) {
				ImVec2 hp = ImGui::GetCursorScreenPos();
				dl->AddRectFilled(ImVec2(hp.x, hp.y + 4 * s), ImVec2(hp.x + 4 * s, hp.y + 17 * s), CATEGORY_COLORS[cat], 2.0f);
				dl->AddText(ImVec2(hp.x + 10 * s, hp.y + 3 * s), Alpha(CATEGORY_COLORS[cat], 255), CATEGORY_NAMES[cat]);
				ImGui::Dummy(ImVec2(1, 22 * s));
				for (int a = 0; a < HK_COUNT; a++) {
					if (DEFS[a].category != cat) continue;
					ImGui::PushID(a);
					ImVec2 p = ImGui::GetCursorScreenPos();
					float w = ImGui::GetContentRegionAvail().x, h = 30.0f * s;
					float keyW = 92.0f * s, resetW = 44.0f * s;
					float keysW = keyW * 2 + 4 * s + 6 * s + resetW;
					bool sel = selected == a;
					ImGui::SetCursorScreenPos(p);
					if (ImGui::InvisibleButton("row", ImVec2(w - keysW - 4 * s, h))) { selected = a; listenAct = -1; }
					bool hov = ImGui::IsItemHovered();
					if (hov) hoverAct = a;
					if (sel || hov) dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), sel ? Alpha(CATEGORY_COLORS[cat], 46) : IM_COL32(255, 255, 255, 14), 4.0f * s);
					if (sel) dl->AddRect(p, ImVec2(p.x + w, p.y + h), Alpha(CATEGORY_COLORS[cat], 150), 4.0f * s);
					dl->AddRectFilled(ImVec2(p.x + 4 * s, p.y + 3 * s), ImVec2(p.x + 28 * s, p.y + h - 3 * s), IM_COL32(26, 29, 36, 255), 4.0f * s);
					DrawIcon(dl, a, ImVec2(p.x + 16 * s, p.y + h / 2), 9.0f * s, CATEGORY_COLORS[cat]);
					ImVec2 ts = ImGui::CalcTextSize(DEFS[a].name);
					dl->AddText(ImVec2(p.x + 36 * s, p.y + (h - ts.y) / 2), IM_COL32(232, 234, 240, 255), DEFS[a].name);

					float kx = p.x + w - keysW;
					for (int slot = 0; slot < 2; slot++) {
						bool listening = listenAct == a && listenSlot == slot;
						char id[8]; snprintf(id, sizeof(id), "k%d", slot);
						int mode = listening ? 1 : (edit[a][slot] ? 0 : 2);
						bool changed = edit[a][slot] != DEFS[a].keys[slot];
						if (Keycap(id, ImVec2(kx + slot * (keyW + 4 * s), p.y + 3 * s), ImVec2(keyW, h - 6 * s), KeyName(edit[a][slot]), mode, CATEGORY_COLORS[cat], changed)) {
							listenAct = a; listenSlot = slot; selected = a; note.clear();
						}
						if (ImGui::IsItemHovered() && !listening) ImGui::SetTooltip("Click, then press the new key.\nBackspace clears it, Esc cancels.");
					}
					if (Differs(a)) {
						ImGui::SetCursorScreenPos(ImVec2(kx + keyW * 2 + 10 * s, p.y + 4 * s));
						if (ImGui::Button("Reset", ImVec2(resetW, h - 8 * s))) {
							for (int slot = 0; slot < 2; slot++) {
								int d = DEFS[a].keys[slot];
								if (d) Assign(a, slot, d); else edit[a][slot] = 0;
							}
							selected = a;
						}
					}
					ImGui::SetCursorScreenPos(p);
					ImGui::Dummy(ImVec2(w, h + 2 * s));
					ImGui::PopID();
				}
				ImGui::Dummy(ImVec2(1, 6 * s));
			}
		}
		ImGui::EndChild();

		// ---- right: the keyboard, then details of the chosen command ----
		ImGui::SameLine();
		ImGui::BeginChild("##right", ImVec2(0, bodyH), true);
		{
			ImDrawList *dl = ImGui::GetWindowDrawList();
			ImVec2 origin = ImGui::GetCursorScreenPos();
			float u = 24.5f * s, gap = 2.0f * s;
			int owner[SDL_NUM_SCANCODES];
			for (int i = 0; i < SDL_NUM_SCANCODES; i++) owner[i] = -1;
			for (int a = 0; a < HK_COUNT; a++) for (int slot = 0; slot < 2; slot++) if (edit[a][slot] > 0 && edit[a][slot] < SDL_NUM_SCANCODES) owner[edit[a][slot]] = a;
			int focus = hoverAct >= 0 ? hoverAct : selected;
			int keyHover = -1;
			for (const KeyDef &k : Keyboard()) {
				ImVec2 p0(origin.x + k.x * u, origin.y + k.y * u), p1(origin.x + (k.x + k.w) * u - gap, origin.y + (k.y + 1) * u - gap);
				int a = owner[k.sc];
				bool locked = Reserved(k.sc);
				ImGui::SetCursorScreenPos(p0);
				char id[16]; snprintf(id, sizeof(id), "kb%d", k.sc);
				if (ImGui::InvisibleButton(id, ImVec2(p1.x - p0.x, p1.y - p0.y)) && !locked && selected >= 0) {
					int slot = listenAct == selected ? listenSlot : (edit[selected][0] == 0 ? 0 : (edit[selected][1] == 0 ? 1 : 0));
					Assign(selected, slot, k.sc);
					listenAct = -1;
				}
				bool hov = ImGui::IsItemHovered();
				if (hov) keyHover = k.sc;
				ImU32 fill = IM_COL32(52, 56, 68, 255);
				if (locked) fill = IM_COL32(38, 40, 48, 255);
				else if (a >= 0) fill = Alpha(CATEGORY_COLORS[DEFS[a].category], 70);
				if (hov && !locked) fill = IM_COL32(88, 94, 112, 255);
				dl->AddRectFilled(p0, p1, fill, 3.0f * s);
				bool lit = a >= 0 && a == focus;
				dl->AddRect(p0, p1, lit ? IM_COL32(255, 255, 255, 255) : (a >= 0 ? Alpha(CATEGORY_COLORS[DEFS[a].category], 200) : IM_COL32(86, 92, 108, 255)), 3.0f * s, 0, lit ? 2.0f * s : 1.0f);
				ImU32 lc = locked ? IM_COL32(96, 100, 112, 255) : (a >= 0 ? IM_COL32(246, 247, 250, 255) : IM_COL32(150, 156, 170, 255));
				dl->AddText(ImVec2(p0.x + 3 * s, p0.y + 1 * s), lc, k.label);
				if (a >= 0) DrawIcon(dl, a, ImVec2(p1.x - 8 * s, p1.y - 7 * s), 5.5f * s, CATEGORY_COLORS[DEFS[a].category]);
			}
			if (keyHover >= 0) {
				if (Reserved(keyHover)) ImGui::SetTooltip("Reserved: 1 to 0 pick your tank");
				else if (owner[keyHover] >= 0) ImGui::SetTooltip("%s", DEFS[owner[keyHover]].name);
				else ImGui::SetTooltip("Not used. Click to give it to \"%s\"", DEFS[selected].name);
			}

			ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + 6.6f * u));
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("Pick a command, then press a key or click one above. Keys with a white outline belong to the command you are pointing at.");
			ImGui::PopTextWrapPos();
			ImGui::Dummy(ImVec2(1, 6 * s));
			ImGui::Separator();

			// details
			if (selected >= 0) {
				ImU32 cc = CATEGORY_COLORS[DEFS[selected].category];
				ImVec2 p = ImGui::GetCursorScreenPos();
				float box = 64.0f * s;
				dl->AddRectFilled(p, ImVec2(p.x + box, p.y + box), IM_COL32(26, 29, 36, 255), 8.0f * s);
				dl->AddRect(p, ImVec2(p.x + box, p.y + box), Alpha(cc, 180), 8.0f * s, 0, 2.0f);
				DrawIcon(dl, selected, ImVec2(p.x + box / 2, p.y + box / 2), 24.0f * s, cc);
				ImGui::SetCursorScreenPos(ImVec2(p.x + box + 14 * s, p.y + 2 * s));
				ImGui::BeginGroup();
				ImGui::PushStyleColor(ImGuiCol_Text, Alpha(cc, 255));
				ImGui::TextUnformatted(DEFS[selected].name);
				ImGui::PopStyleColor();
				ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 4 * s);
				ImGui::TextUnformatted(DEFS[selected].desc);
				ImGui::PopTextWrapPos();
				ImGui::EndGroup();
				ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + box + 8 * s));
				std::string cur = KeyName(edit[selected][0]), alt = KeyName(edit[selected][1]);
				if (!alt.empty()) cur += (cur.empty() ? "" : "  and  ") + alt;
				ImGui::Text("Keys now: %s", cur.empty() ? "none" : cur.c_str());
				std::string def = KeyName(DEFS[selected].keys[0]), def2 = KeyName(DEFS[selected].keys[1]);
				if (!def2.empty()) def += (def.empty() ? "" : "  and  ") + def2;
				ImGui::TextDisabled("Default: %s", def.empty() ? "none" : def.c_str());
				if (DEFS[selected].held) ImGui::TextDisabled("Works while the key is held down.");
			}
			if (!note.empty()) {
				ImGui::Dummy(ImVec2(1, 4 * s));
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 214, 96, 255));
				ImGui::PushTextWrapPos(0.0f);
				ImGui::TextUnformatted(note.c_str());
				ImGui::PopTextWrapPos();
				ImGui::PopStyleColor();
			}
		}
		ImGui::EndChild();

		// ---- footer ----
		ImGui::Dummy(ImVec2(1, 4 * s));
		float bw = 120.0f * s, bh = 28.0f * s;
		if (ImGui::Button("Default layout", ImVec2(bw, bh))) { SetPreset(0); listenAct = -1; note = "Every key is back to its default."; }
		ImGui::SameLine();
		if (ImGui::Button("WASD layout", ImVec2(bw, bh))) { SetPreset(1); listenAct = -1; note = "W A S D drive, Space fires, Q drops items. The arrow keys still work."; }
		float right = ImGui::GetWindowContentRegionMax().x;
		ImGui::SameLine(right - bw * 2 - 8 * s);
		if (ImGui::Button("Save", ImVec2(bw, bh))) save = true;
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(bw, bh))) keepOpen = false;
	}
	ImGui::End();
	ImGui::PopStyleColor((int)(sizeof(theme) / sizeof(theme[0])));

	if (save) {
		if (!edit[HK_CHAT][0] && !edit[HK_CHAT][1]) {
			note = "Chat needs a key, or you could not talk to anyone.";
			selected = HK_CHAT;
			save = false;
		} else {
			memcpy(live, edit, sizeof(live));
			Save();
			keepOpen = false;
		}
	}
	if (!keepOpen) {
		open = false;
		if (onClose) onClose();
		return false;
	}
	return true;
}
