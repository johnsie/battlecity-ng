/*
===============================================================================
    Battle City - CWaypoints source file. See CWaypoints.h and docs/waypoints.md.
===============================================================================
*/
#include "CWaypoints.h"
#include <cmath>
#include <vector>

namespace {
const float SEND_MS = 1000.0f;
const float FIGHT_MS = 45000.0f;       // a fight is "the latest" for this long
const float FIGHT_NEAR = 700.0f;       // no pointer once you are in it
const size_t MAX_POINTS = 8;
}

CWaypoints::CWaypoints(CServer *server) {
	p = server;
	nextSend = 0;
	haveFight = false;
	fightX = fightY = fightAt = 0;
}

void CWaypoints::Reset() {
	haveFight = false;
	nextSend = 0;
}

void CWaypoints::NoteFight(float x, float y) {
	haveFight = true;
	fightX = x; fightY = y;
	fightAt = p->Tick;
}

void CWaypoints::Cycle() {
	if (p->Tick < nextSend) return;
	nextSend = p->Tick + SEND_MS;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		CPlayer *pl = p->Player[i];
		if (pl->IsBot || pl->Socket < 0 || pl->lingering || !pl->isInGame()) continue;
		std::vector<WaypointInfo> list;
		CCity *city = CCity::isValidCityIndex(pl->City) ? p->City[pl->City] : NULL;
		if (city) list.push_back(WaypointInfo{ WP_HOME, (float)city->x, (float)city->y });
		p->Bots->Objectives(i, list);
		if (haveFight && p->Tick - fightAt < FIGHT_MS) {
			float dx = fightX - pl->X, dy = fightY - pl->Y;
			if (dx * dx + dy * dy > FIGHT_NEAR * FIGHT_NEAR) list.push_back(WaypointInfo{ WP_FIGHT, fightX, fightY });
		}
		if (list.size() > MAX_POINTS) list.resize(MAX_POINTS);
		char packet[1 + MAX_POINTS * 5];
		packet[0] = (char)list.size();
		for (size_t k = 0; k < list.size(); k++) {
			unsigned short x = (unsigned short)std::max(0.0f, std::min(65000.0f, list[k].x));
			unsigned short y = (unsigned short)std::max(0.0f, std::min(65000.0f, list[k].y));
			packet[1 + k * 5] = (char)list[k].type;
			packet[2 + k * 5] = (char)(x & 255); packet[3 + k * 5] = (char)(x >> 8);
			packet[4 + k * 5] = (char)(y & 255); packet[5 + k * 5] = (char)(y >> 8);
		}
		p->Winsock->SendData(i, smWaypoints, packet, (int)(1 + list.size() * 5));
	}
}
