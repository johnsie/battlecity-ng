/*
    Battle City - keeping the game going across a server restart

    The world (cities, buildings, items, who is where) lives in memory and used to be lost whenever the server was restarted, which is
    what every update does: players were thrown out and the cities of mayors were gone. Now the world is written to dat/world.state when
    the server is stopped and read back at the next start. A player who was in the game gets their seat back when their client logs in
    again (the web client does so by itself): same city, same position, same things in the tank, mayors are mayors again.

    A city whose mayor does not come back is destroyed after TIMER_GRACE, as if the mayor had left. Nothing is restored from a file older
    than FILE_MAX_AGE (a crash, a long outage): the world then starts fresh as before.
*/
#include "CServer.h"

namespace {

const char *FILE_NAME = "dat/world.state";
const long FILE_MAX_AGE = 15 * 60;          // seconds
const long SEAT_SECONDS = 120;              // a player has this long after the start to log back in
const float TIMER_GRACE = 150000.0f;        // ms before the city of a mayor who did not come back is destroyed

// names may hold anything: keep the lines parseable
std::string Esc(const std::string &s) {
	std::string out;
	char buf[4];
	for (size_t i = 0; i < s.size(); i++) {
		unsigned char c = (unsigned char)s[i];
		if (isalnum(c) || c == '-' || c == '_' || c == '.') out += (char)c;
		else { snprintf(buf, sizeof(buf), "%%%02X", c); out += buf; }
	}
	return out.empty() ? "%" : out;
}

std::string Unesc(const std::string &s) {
	if (s == "%") return "";
	std::string out;
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '%' && i + 2 < s.size() + 0 && isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) {
			out += (char)strtol(s.substr(i + 1, 2).c_str(), NULL, 16);
			i += 2;
		} else out += s[i];
	}
	return out;
}

std::string Lower(std::string s) {
	for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
	return s;
}

}  // namespace

void CServer::SaveWorld() {
	// only worth keeping when somebody is playing
	bool anyone = false;
	for (int i = 0; i < MAX_PLAYERS; i++) if (Player[i]->isInGame() && !Player[i]->IsBot) anyone = true;
	if (!anyone) {
		remove(FILE_NAME);
		return;
	}
	std::string tmp = std::string(FILE_NAME) + ".tmp";
	std::ofstream out(tmp.c_str());
	if (!out) { cout << "World::Save failed: cannot write " << tmp << endl; return; }
	out << "BCWORLD 1 " << (long)time(NULL) << "\n";
	out << "IDS " << Build->bldID << " " << Item->itmID << "\n";

	int cities = 0, buildings = 0, items = 0, seats = 0;
	for (int c = 0; c < MAX_CITIES; c++) {
		CCity *city = City[c];
		bool mayorHere = city->Mayor >= 0 && city->Mayor < MAX_PLAYERS && Player[city->Mayor]->isInGame();
		if (!mayorHere && city->DestructTimer <= 0) continue;
		std::string mayor = mayorHere ? Player[city->Mayor]->Name : "";
		out << "C " << c << " " << Esc(mayor) << " " << city->Orbs << " " << city->notHiring << " " << city->currentBuildingCount << " "
		    << city->maxBuildingCount << " " << (long)(Tick - city->startTime) << " " << (int)city->hadBombFactory << " " << (int)city->hadOrbFactory << " "
		    << city->cash << " " << city->income << " " << city->itemproduction << " " << city->cashresearch << " " << city->hospital << " "
		    << (long)(city->DestructTimer > 0 ? std::max(1.0f, city->DestructTimer - Tick) : 0) << "\n";
		out << "CB";
		for (int i = 0; i < 30; i++) out << " " << city->canBuild[i];
		out << "\nCR";
		for (int i = 0; i < 20; i++) out << " " << (long)(city->research[i] > 0 ? std::max(1.0f, city->research[i] - Tick) : city->research[i]);
		out << "\nCI";
		for (int i = 0; i < 12; i++) out << " " << city->itemC[i];
		out << "\n";
		cities++;
	}
	for (CBuilding *b = Build->buildingListHead; b; b = b->next) {
		if (!CCity::isValidCityIndex(b->City) || b->isCC()) continue;
		out << "B " << b->x << " " << b->y << " " << b->type << " " << b->City << " " << b->id << " " << b->AttachedID << " " << b->pop << " "
		    << b->AttachedID2 << " " << b->AttachedPop << " " << b->AttachedPop2 << " "
		    << (long)(b->ProduceTick - Tick) << " " << (long)(b->PopulationTick - Tick) << " " << (long)(b->CashTick - Tick) << " "
		    << (long)(b->MoneyTick - Tick) << " " << (long)(b->ResearchTick - Tick) << "\n";
		buildings++;
	}
	for (CItem *it = Item->itemListHead; it; it = it->next) {
		if (it->holder != -1 || !CCity::isValidCityIndex(it->City)) continue;
		out << "I " << it->x << " " << it->y << " " << it->type << " " << it->City << " " << it->id << " " << it->active << " " << it->life << "\n";
		items++;
	}
	for (int i = 0; i < MAX_PLAYERS; i++) {
		CPlayer *pl = Player[i];
		if (pl->IsBot || pl->guest || !pl->isInGame() || pl->Name.empty()) continue;
		std::vector<CItem *> held;
		for (CItem *it = Item->itemListHead; it; it = it->next) if (it->holder == i) held.push_back(it);
		out << "S " << Esc(pl->Name) << " " << pl->City << " " << (pl->Mayor ? 1 : 0) << " " << (int)pl->X << " " << (int)pl->Y << " " << held.size();
		for (size_t k = 0; k < held.size(); k++) out << " " << held[k]->type << " " << held[k]->active << " " << held[k]->life << " " << held[k]->City;
		out << "\n";
		seats++;
	}
	out.close();
	if (rename(tmp.c_str(), FILE_NAME) != 0) { cout << "World::Save failed: cannot rename " << tmp << endl; return; }
	cout << "World::Saved " << cities << " cities, " << buildings << " buildings, " << items << " items, " << seats << " players" << endl;
}

void CServer::LoadWorld() {
	std::ifstream in(FILE_NAME);
	if (!in) return;
	std::string line;
	std::getline(in, line);
	std::istringstream head(line);
	std::string magic; int version = 0; long saved = 0;
	head >> magic >> version >> saved;
	if (magic != "BCWORLD" || version != 1 || (long)time(NULL) - saved > FILE_MAX_AGE || (long)time(NULL) < saved) {
		cout << "World::Load ignored (old or unreadable file)" << endl;
		in.close();
		remove(FILE_NAME);
		return;
	}
	float now = Timer->GetTime();
	Tick = now;
	int cities = 0, buildings = 0, items = 0, seats = 0;
	int maxBld = Build->bldID, maxItm = Item->itmID;
	CCity *cur = NULL;
	// Buildings and items first: creating them counts them in their city, and the city's own saved counters must win
	std::vector<std::string> lines;
	while (std::getline(in, line)) lines.push_back(line);
	for (int pass = 0; pass < 2; pass++) for (size_t li = 0; li < lines.size(); li++) {
		std::istringstream ls(lines[li]);
		std::string tag;
		ls >> tag;
		bool objects = (tag == "B" || tag == "I" || tag == "IDS");
		if (objects != (pass == 0)) continue;
		if (tag == "IDS") {
			int b = 0, i = 0; ls >> b >> i;
			maxBld = std::max(maxBld, b); maxItm = std::max(maxItm, i);
		} else if (tag == "C") {
			int id; std::string mayor; long uptime, destruct; int hadBomb, hadOrb;
			ls >> id >> mayor;
			if (!CCity::isValidCityIndex(id)) { cur = NULL; continue; }
			cur = City[id];
			ls >> cur->Orbs >> cur->notHiring >> cur->currentBuildingCount >> cur->maxBuildingCount >> uptime >> hadBomb >> hadOrb
			   >> cur->cash >> cur->income >> cur->itemproduction >> cur->cashresearch >> cur->hospital >> destruct;
			cur->startTime = now - uptime;
			cur->hadBombFactory = hadBomb != 0;
			cur->hadOrbFactory = hadOrb != 0;
			cur->Mayor = -1;
			cur->Successor = -1;
			cur->hiring = -1;
			cur->moneyTick = 0;
			cur->cityVerificationTick = 0;
			// the mayor has TIMER_GRACE to log back in; a city that was already counting down goes on counting
			cur->DestructTimer = now + (destruct > 0 ? (float)destruct : TIMER_GRACE);
			cities++;
		} else if (tag == "CB" && cur) {
			for (int i = 0; i < 30; i++) ls >> cur->canBuild[i];
		} else if (tag == "CR" && cur) {
			for (int i = 0; i < 20; i++) { long v = 0; ls >> v; cur->research[i] = v > 0 ? now + (float)v : (float)v; }
		} else if (tag == "CI" && cur) {
			for (int i = 0; i < 12; i++) ls >> cur->itemC[i];
		} else if (tag == "B") {
			int x, y, type, city, id, a1, pop, a2, ap1, ap2; long t[5];
			ls >> x >> y >> type >> city >> id >> a1 >> pop >> a2 >> ap1 >> ap2 >> t[0] >> t[1] >> t[2] >> t[3] >> t[4];
			if (!CCity::isValidCityIndex(city)) continue;
			CBuilding *b = Build->newBuilding(x, y, type, city, (unsigned short)id);
			b->AttachedID = a1; b->pop = pop; b->AttachedID2 = a2; b->AttachedPop = ap1; b->AttachedPop2 = ap2;
			b->ProduceTick = now + t[0]; b->PopulationTick = now + t[1]; b->CashTick = now + t[2]; b->MoneyTick = now + t[3]; b->ResearchTick = now + t[4];
			buildings++;
		} else if (tag == "I") {
			int x, y, type, city, id, active, life;
			ls >> x >> y >> type >> city >> id >> active >> life;
			if (!CCity::isValidCityIndex(city) || type < 0 || type > 11) continue;
			CItem *it = Item->newItem(x, y, type, city, (unsigned short)id);
			it->active = active; it->life = life;
			items++;
		} else if (tag == "S") {
			std::string name; WorldSeat seat; size_t n = 0; int mayor;
			int sx, sy;
			ls >> name >> seat.city >> mayor >> sx >> sy >> n;
			seat.mayor = mayor != 0; seat.x = (float)sx; seat.y = (float)sy;
			seat.expires = (long)time(NULL) + SEAT_SECONDS;
			for (size_t k = 0; k < n && k < 40; k++) { WorldSeat::Held h; ls >> h.type >> h.active >> h.life >> h.city; seat.held.push_back(h); }
			if (CCity::isValidCityIndex(seat.city)) { Seats[Lower(Unesc(name))] = seat; seats++; }
		}
	}
	in.close();
	remove(FILE_NAME);      // used once: a later crash must not bring back an even older world
	Build->bldID = maxBld;
	Item->itmID = maxItm;
	if (cities > 0 || seats > 0) keepWorldUntil = (long)time(NULL) + SEAT_SECONDS + 30;
	cout << "World::Loaded " << cities << " cities, " << buildings << " buildings, " << items << " items, " << seats << " players waiting to come back" << endl;
}

/// The server was restarted while this account was in the game (see SaveWorld): put the player back where they were, in the same
/// city, as mayor again when they were, carrying what they carried. Returns false when there is no seat to give back.
bool CProcess::RestoreSeat(int Index, const char *user) {
	std::string key = Lower(user);
	std::map<std::string, WorldSeat>::iterator it = p->Seats.find(key);
	if (it == p->Seats.end()) return false;
	WorldSeat seat = it->second;
	p->Seats.erase(it);
	if ((long)time(NULL) > seat.expires || !CCity::isValidCityIndex(seat.city)) return false;
	CPlayer *player = p->Player[Index];
	CCity *city = p->City[seat.city];
	if (seat.mayor) {
		// the city must still be waiting for its mayor (it is not if somebody else founded it meanwhile or it was destroyed)
		if (city->Mayor != -1 || city->DestructTimer <= 0) return false;
	} else if (city->PlayerCount() >= MAX_PLAYERS_PER_CITY) {
		return false;
	}

	player->City = seat.city;
	player->resumePending = true;
	player->resumeX = seat.x;
	player->resumeY = seat.y;
	player->lastMove = p->Tick;
	player->seatGuardUntil = p->Tick + 8000.0f;
	player->StartJoin();                    // State_Game, and the client is told to start joining
	if (seat.mayor) {
		city->DestructTimer = 0;
		player->setMayor(true);
	} else {
		player->setMayor(false);
	}

	// what the tank carried
	for (size_t k = 0; k < seat.held.size(); k++) {
		const WorldSeat::Held &h = seat.held[k];
		if (h.type < 0 || h.type > 11) continue;
		unsigned short id = (unsigned short)(p->Item->itmID++);
		if (p->Item->itmID > 30000) p->Item->itmID = 1;
		CItem *item = p->Item->newItem(0, 0, h.type, CCity::isValidCityIndex(h.city) ? h.city : seat.city, id);
		if (CCity::isValidCityIndex(item->City)) p->City[item->City]->itemC[item->type] -= 1;   // it was counted when it was saved
		item->holder = Index;
		item->active = h.active;
		item->life = h.life;
		sCMItem packet;
		packet.id = item->id;
		packet.active = item->active;
		packet.type = item->type;
		p->Winsock->SendData(Index, smPickedUp, (char *)&packet, sizeof(packet));
	}
	cout << "Restored::" << user << " in city " << seat.city << (seat.mayor ? " as mayor" : "") << endl;
	return true;
}
