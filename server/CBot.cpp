/*
===============================================================================

    Battle City - CBot
    Server-side bots and the events built around them. See docs/solo-play.md.

    This file is part of Battle City.

    Battle City is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
===============================================================================
*/
#include "CBot.h"

#include <algorithm>
#include <queue>
#include <ctime>

static const float STEP_MS = 30.0f;            // movement and AI step
static const float UPDATE_MS = 120.0f;         // how often each bot's position goes to nearby players
static const float TURN_MS = 50.0f;            // a tank turns one step (of 32) per 50 ms, same as the client
static const float PI_GAME = 3.14f;            // the client uses 3.14; staying equal keeps dead reckoning in step
static const int MAX_FIGHTERS_HARD = 12;
static const int DAILY_BOT_POINT_CAP = 60;

// ---- small helpers -------------------------------------------------------------------------------------------

float CBotManager::Now() const { return p->Tick; }

float CBotManager::Rand01() {
	return (float)(rand() % 10000) / 10000.0f;
}

/// Direction index (0..31) that moves from (0,0) towards (dx,dy): the client moves by (sin a, cos a) with a = (32 - dir) / 16 * 3.14
int CBotManager::DirTo(float dx, float dy) {
	float a = atan2f(dx, dy);
	float d = 32.0f - a * 16.0f / PI_GAME;
	int di = (int)floorf(d + 0.5f);
	di %= 32;
	if (di < 0) di += 32;
	return di;
}

/// Compass words as the player sees them: the view is mirrored, so a larger world X is to the west and a larger Y is to the north
std::string CBotManager::Compass(float dx, float dy) {
	std::string ns, ew;
	float ax = fabsf(dx), ay = fabsf(dy);
	if (ay > ax * 0.4f) ns = dy > 0 ? "north" : "south";
	if (ax > ay * 0.4f) ew = dx > 0 ? "west" : "east";
	if (ns.empty() && ew.empty()) return "nearby";
	if (ns.empty()) return ew;
	if (ew.empty()) return ns;
	return ns + "-" + ew;
}

static std::string Num(long v) {
	std::ostringstream o;
	o << v;
	return o.str();
}

static std::string Lower(const std::string &s) {
	std::string r = s;
	for (size_t i = 0; i < r.size(); i++) r[i] = (char)tolower((unsigned char)r[i]);
	return r;
}

/// Name as stored in the state file: lower case and no spaces (the file is whitespace separated)
static std::string Key(const std::string &s) {
	std::string r = Lower(s);
	for (size_t i = 0; i < r.size(); i++) if (r[i] == ' ') r[i] = '_';
	return r;
}

CBotManager::CBotManager(CServer *Server) {
	p = Server;
	enabled = true;
	tempo = 1.0f;
	safeRadius = 1500.0f;
	maxBots = 3;
	botCity = 63;
	overseer = -1;
	nextStep = nextManage = nextSave = lastStepTick = nextTurret = 0;
	pathBudget = 0;
	lastHumanSeen = 0;
	lastHumanCount = 0;
	lastWebhook = -1e9f;
	dirty = false;
	lastOnlineTime = 0;
	ghostPoints = 0;
	boardWeek = 0;
	nextBoard = 0;
	lastWinnerWeek = lastWinnerScore = 0;
	for (int i = 0; i < 24; i++) { bots[i].used = false; }
	raid = Raid();         // value-initialised (these hold std::set: never memset them)
	mission = Mission();
	memset(drops, 0, sizeof(drops));
	memset(trial, 0, sizeof(trial));
	memset(range, 0, sizeof(range));
	raid.active = false; mission.active = false;
	nextRaid = nextMission = nextBounty = nextDrop = 0;
}

CBotManager::~CBotManager() {
}

void CBotManager::Init() {
	if (const char *n = getenv("BC_BOTS")) maxBots = atoi(n);
	if (maxBots < 0) maxBots = 0;
	if (maxBots > MAX_FIGHTERS_HARD) maxBots = MAX_FIGHTERS_HARD;
	if (const char *c = getenv("BC_BOT_CITY")) botCity = atoi(c);
	if (const char *z = getenv("BC_BOTS_SAFE_RADIUS")) { safeRadius = (float)atof(z); if (safeRadius < 0) safeRadius = 0; }
	if (!CCity::isValidCityIndex(botCity)) botCity = 63;
	if (const char *w = getenv("BC_DISCORD_WEBHOOK")) webhook = w;
	if (const char *t = getenv("BC_BOTS_TEMPO")) { tempo = (float)atof(t); if (tempo < 0.01f) tempo = 0.01f; if (tempo > 10.0f) tempo = 10.0f; }   // test hook: scales the gaps between events
	enabled = (maxBots > 0);
	srand((unsigned int)time(NULL) ^ 0x5bd1e995u);
	cout << "CBotManager::Init() bots=" << maxBots << " city=" << botCity << (enabled ? "" : " (disabled)") << (webhook.empty() ? "" : " discord=on") << endl;
	if (enabled) {
		Load();
		LoadGhost();
	}
}

void CBotManager::ResetState() {
	RemoveAllBots();
	raid.active = false;
	mission.active = false;
	for (int i = 0; i < 4; i++) drops[i].active = false;
	memset(trial, 0, sizeof(trial));
	memset(range, 0, sizeof(range));
	recording.clear();
	recordNext.clear();
	greeted.clear();
	lastHumanCount = 0;
	nextRaid = nextMission = nextBounty = nextDrop = 0;
}

void CBotManager::Shutdown() {
	RemoveAllBots();
	if (enabled) Save();
}

bool CBotManager::IsBotSlot(int slot) const {
	return slot >= 0 && slot < MAX_PLAYERS && p->Player[slot]->IsBot;
}

bool CBotManager::IsHuman(int i) const {
	return i >= 0 && i < MAX_PLAYERS && p->Player[i]->isInGame() && !p->Player[i]->IsBot;
}

int CBotManager::CountHumans() const {
	int n = 0;
	for (int i = 0; i < MAX_PLAYERS; i++) if (IsHuman(i)) n++;
	return n;
}

int CBotManager::NearestHuman(float x, float y, float *distOut, bool aliveOnly) const {
	int best = -1;
	float bestD = 1e12f;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (!IsHuman(i)) continue;
		if (aliveOnly && (p->Player[i]->isDead || p->Player[i]->isCloaked)) continue;
		float dx = p->Player[i]->X - x, dy = p->Player[i]->Y - y;
		float d = sqrtf(dx * dx + dy * dy);
		if (d < bestD) { bestD = d; best = i; }
	}
	if (distOut) *distOut = bestD;
	return best;
}

// ---- terrain -------------------------------------------------------------------------------------------------

bool CBotManager::Blocked(float x, float y) const {
	if (x < 96 || y < 96 || x > 511 * 48 - 96 || y > 511 * 48 - 96) return true;
	const int gap = 8;
	long rx = (long)x + gap, ry = (long)y + gap, rw = 48 - gap - gap, rh = 48 - gap - gap;
	if (p->Map->map[(rx - 1) / 48][(ry - 1) / 48] != 0) return true;
	if (p->Map->map[(rx + rw) / 48][(ry - 1) / 48] != 0) return true;
	if (p->Map->map[(rx + rw) / 48][(ry + rh) / 48] != 0) return true;
	if (p->Map->map[(rx - 1) / 48][(ry + rh) / 48] != 0) return true;
	for (CBuilding *bld = p->Build->buildingListHead; bld; bld = bld->next) {
		long bx = (bld->x - 2) * 48, by = (bld->y - 2) * 48, bw = 144, bh = 144;
		if (bld->type / 100 <= 2) { by = (bld->y - 1) * 48; bh = 96; }
		if (rx < bx + bw && rx + rw > bx && ry < by + bh && ry + rh > by) return true;
	}
	return false;
}

bool CBotManager::FreeSpot(float x, float y) const {
	// a spot with room around it, so a bot never starts boxed in
	return !Blocked(x, y) && !Blocked(x + 40, y) && !Blocked(x - 40, y) && !Blocked(x, y + 40) && !Blocked(x, y - 40);
}

/// Bots keep well clear of cities that have a mayor, so nobody is hunted (or blocked by a tank parked on the plot) while building at home
bool CBotManager::InSafeZone(float x, float y, float *awayX, float *awayY) const {
	const float SAFE = safeRadius;
	for (int ci = 0; ci < MAX_CITIES; ci++) {
		if (ci == botCity || p->City[ci]->Mayor == -1) continue;
		float dx = x - (float)p->City[ci]->x, dy = y - (float)p->City[ci]->y;
		float d2 = dx * dx + dy * dy;
		if (d2 < SAFE * SAFE) {
			if (awayX) {
				float d = sqrtf(d2) + 1.0f;
				*awayX = x + dx / d * 900.0f;
				*awayY = y + dy / d * 900.0f;
			}
			return true;
		}
	}
	return false;
}

bool CBotManager::FindSpawn(float nearX, float nearY, float minDist, float maxDist, float *ox, float *oy) {
	for (int tries = 0; tries < 40; tries++) {
		float a = Rand01() * 6.2832f;
		float d = minDist + Rand01() * (maxDist - minDist);
		float x = nearX + cosf(a) * d, y = nearY + sinf(a) * d;
		if (!FreeSpot(x, y) || InSafeZone(x, y)) continue;
		// never pop in on top of someone
		float dist;
		NearestHuman(x, y, &dist, false);
		if (dist < 700) continue;
		*ox = x; *oy = y;
		return true;
	}
	return false;
}

// ---- slots, announcements, events ----------------------------------------------------------------------------

int CBotManager::AllocSlot() {
	for (int i = MAX_PLAYERS - 1; i >= 0; i--) {
		if (p->Player[i]->State == State_Disconnected) return i;
	}
	return -1;
}

void CBotManager::ReleaseSlot(int slot) {
	char pk[1] = { (char)slot };
	p->Send->SendAllBut(-1, smClearPlayer, pk, 1);
	CPlayer *pl = p->Player[slot];
	pl->State = State_Disconnected;
	pl->IsBot = false;
	pl->City = -1;
	pl->Name.clear();
	pl->Town.clear();
	pl->X = pl->Y = 0;
	pl->Points = pl->Orbs = pl->Assists = pl->Deaths = pl->MonthlyPoints = 0;
	pl->Tank = pl->displayTank = 0;
	pl->isDead = false;
	pl->isCloaked = false;
	pl->Mayor = 0;
}

void CBotManager::EnsureOverseer() {
	if (overseer >= 0 && p->Player[overseer]->IsBot) return;
	int slot = AllocSlot();
	if (slot < 0) return;
	CPlayer *pl = p->Player[slot];
	pl->IsBot = true;
	pl->Name = "Overseer";
	pl->Town = "Command";
	pl->City = botCity;
	pl->State = State_Game;
	pl->playerType = 1;
	pl->Red = 255; pl->Green = 215; pl->Blue = 0;
	pl->Member = 0;
	pl->isDead = true;            // invisible: it only exists to speak for the server
	pl->timeDeath = p->Tick;
	pl->X = pl->Y = 0;
	pl->lastMove = p->Tick;
	overseer = slot;
	// The bot city must look taken, so no human can start or be sent there
	p->City[botCity]->Mayor = slot;
	p->City[botCity]->notHiring = 1;
	// tell everyone
	sSMPlayer pk;
	memset(&pk, 0, sizeof(pk));
	pk.Index = slot;
	strncpy(pk.Name, pl->Name.c_str(), 15);
	strncpy(pk.Town, pl->Town.c_str(), 15);
	pk.playerType = 1; pk.Red = 255; pk.Green = 215; pk.Blue = 0;
	p->Send->SendAllBut(-1, smPlayerData, (char *)&pk, sizeof(pk));
	sSMJoinData jd;
	jd.id = slot; jd.Mayor = 0; jd.City = botCity;
	p->Send->SendGameAllBut(-1, smJoinData, (char *)&jd, sizeof(jd));
}

void CBotManager::Tell(int idx, const std::string &text) {
	if (!IsHuman(idx)) return;
	EnsureOverseer();
	if (overseer < 0) return;
	std::string t = text.substr(0, 200);
	char buf[255];
	memset(buf, 0, sizeof(buf));
	buf[0] = (char)overseer;
	memcpy(&buf[1], t.c_str(), t.size());
	p->Winsock->SendData(idx, smGlobal, buf, (int)t.size() + 2);
}

void CBotManager::TellAll(const std::string &text) {
	for (int i = 0; i < MAX_PLAYERS; i++) if (IsHuman(i)) Tell(i, text);
}

/// "25 tiles north-west of you": where a spot is relative to this player (map coordinates mean nothing to someone who cannot see their own)
std::string CBotManager::Rel(int idx, float x, float y) const {
	float dx = x - p->Player[idx]->X, dy = y - p->Player[idx]->Y;
	long tiles = (long)(sqrtf(dx * dx + dy * dy) / 48.0f);
	if (tiles < 5) return "right here";
	return Num(tiles) + " tiles " + Compass(dx, dy) + " of you";
}

/// Tells every human `before` + where (x, y) is relative to them + `after`
void CBotManager::TellEachRel(const std::string &before, float x, float y, const std::string &after) {
	for (int i = 0; i < MAX_PLAYERS; i++) if (IsHuman(i)) Tell(i, before + Rel(i, x, y) + after);
}

void CBotManager::TellCity(int city, const std::string &text) {
	for (int i = 0; i < MAX_PLAYERS; i++) if (IsHuman(i) && p->Player[i]->City == city) Tell(i, text);
}

void CBotManager::Event(const std::string &text) {
	time_t t = time(NULL);
	struct tm *tmv = gmtime(&t);
	char stamp[32];
	strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", tmv);
	events.push_back(std::string(stamp) + "  " + text);
	while (events.size() > 8) events.erase(events.begin());
	dirty = true;
}

std::string CBotManager::RecentEvents() {
	if (!enabled) return "";
	std::string s = "\r\n\r\nRecent events\r\n";
	if (events.empty()) s += "  (none yet: come and make some)\r\n";
	for (int i = (int)events.size() - 1; i >= 0; i--) s += "  " + events[i] + "\r\n";
	std::string board = BoardText(3, "\r\n");
	if (!board.empty()) s += "\r\nWeekly leaderboard (solo score)\r\n" + board;
	if (!lastWinnerName.empty()) s += "  Last week's winner: " + lastWinnerName + " (" + Num(lastWinnerScore) + ")\r\n";
	if (!lastOnlineName.empty() && lastOnlineTime > 0) {
		time_t t = (time_t)lastOnlineTime;
		struct tm *tmv = gmtime(&t);
		char stamp[32];
		strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", tmv);
		s += "  Last player online: " + lastOnlineName + " (" + stamp + " UTC)\r\n";
	}
	return s;
}

// ---- persistence ---------------------------------------------------------------------------------------------

void CBotManager::Load() {
	std::ifstream in("bots_state.txt");
	std::string line;
	while (std::getline(in, line)) {
		std::istringstream ls(line);
		std::string kind;
		ls >> kind;
		if (kind == "chal") {
			std::string name; Progress g; int dd, wd;
			ls >> name >> g.day >> g.dailyKills >> dd >> g.week >> g.weeklyKills >> wd >> g.pointsDay >> g.botPoints;
			g.dailyDone = dd != 0; g.weeklyDone = wd != 0;
			if (!name.empty()) progress[name] = g;
		}
		else if (kind == "trial") { std::string n; int ms; ls >> n >> ms; if (!n.empty()) trialBest[n] = ms; }
		else if (kind == "range") { std::string n; int ms; ls >> n >> ms; if (!n.empty()) rangeBest[n] = ms; }
		else if (kind == "streak") { std::string n; Streak s; ls >> n >> s.lastDay >> s.count >> s.best; if (!n.empty()) streaks[n] = s; }
		else if (kind == "score") { std::string n; int v; ls >> n >> v; if (!n.empty() && v > 0) weekScore[n] = v; }
		else if (kind == "board") { ls >> boardWeek; }
		else if (kind == "winner") { ls >> lastWinnerWeek >> lastWinnerScore >> lastWinnerName; }
		else if (kind == "online") { ls >> lastOnlineTime >> lastOnlineName; }
		else if (kind == "event") { std::string rest; std::getline(ls, rest); if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1); events.push_back(rest); }
	}
	while (events.size() > 8) events.erase(events.begin());
}

void CBotManager::Save() {
	std::ofstream out("bots_state.txt", std::ios::trunc);
	if (!out) return;
	for (std::map<std::string, Progress>::iterator it = progress.begin(); it != progress.end(); ++it) {
		const Progress &g = it->second;
		out << "chal " << it->first << " " << g.day << " " << g.dailyKills << " " << (g.dailyDone ? 1 : 0) << " " << g.week << " "
			<< g.weeklyKills << " " << (g.weeklyDone ? 1 : 0) << " " << g.pointsDay << " " << g.botPoints << "\n";
	}
	for (std::map<std::string, int>::iterator it = trialBest.begin(); it != trialBest.end(); ++it) out << "trial " << it->first << " " << it->second << "\n";
	for (std::map<std::string, int>::iterator it = rangeBest.begin(); it != rangeBest.end(); ++it) out << "range " << it->first << " " << it->second << "\n";
	for (std::map<std::string, Streak>::iterator it = streaks.begin(); it != streaks.end(); ++it) out << "streak " << it->first << " " << it->second.lastDay << " " << it->second.count << " " << it->second.best << "\n";
	for (std::map<std::string, int>::iterator it = weekScore.begin(); it != weekScore.end(); ++it) out << "score " << it->first << " " << it->second << "\n";
	out << "board " << boardWeek << "\n";
	if (!lastWinnerName.empty()) out << "winner " << lastWinnerWeek << " " << lastWinnerScore << " " << lastWinnerName << "\n";
	if (!lastOnlineName.empty()) out << "online " << lastOnlineTime << " " << lastOnlineName << "\n";
	for (size_t i = 0; i < events.size(); i++) out << "event " << events[i] << "\n";
	dirty = false;
}

void CBotManager::SaveGhost() {
	std::ofstream out("ghost.dat", std::ios::trunc);
	if (!out) return;
	out << ghostName << " " << ghostPoints << " " << ghostPath.size() << "\n";
	for (size_t i = 0; i < ghostPath.size(); i++) out << (int)ghostPath[i].x << " " << (int)ghostPath[i].y << " " << ghostPath[i].dir << "\n";
}

void CBotManager::LoadGhost() {
	std::ifstream in("ghost.dat");
	if (!in) return;
	size_t n = 0;
	if (!(in >> ghostName >> ghostPoints >> n)) return;
	ghostPath.clear();
	for (size_t i = 0; i < n && i < 400; i++) {
		Sample s; int x, y;
		if (!(in >> x >> y >> s.dir)) break;
		s.x = (float)x; s.y = (float)y;
		ghostPath.push_back(s);
	}
	if (ghostPath.size() < 20) { ghostPath.clear(); ghostName.clear(); }
}

// ---- progress and points -------------------------------------------------------------------------------------

Progress &CBotManager::ProgressFor(int idx) {
	std::string name = Lower(p->Player[idx]->Name);
	long now = (long)time(NULL);
	int day = (int)(now / 86400), week = day / 7;
	std::map<std::string, Progress>::iterator it = progress.find(name);
	if (it == progress.end()) {
		Progress g;
		memset(&g, 0, sizeof(g));
		g.day = day; g.week = week; g.pointsDay = day;
		it = progress.insert(std::make_pair(name, g)).first;
	}
	Progress &g = it->second;
	if (g.day != day) { g.day = day; g.dailyKills = 0; g.dailyDone = false; }
	if (g.week != week) { g.week = week; g.weeklyKills = 0; g.weeklyDone = false; }
	if (g.pointsDay != day) { g.pointsDay = day; g.botPoints = 0; }
	return g;
}

std::string CBotManager::DailyText(int idx) {
	Progress &g = ProgressFor(idx);
	int day = g.day;
	int dailyGoal = 8 + 4 * (day % 3);
	std::string s = "Daily: destroy " + Num(dailyGoal) + " raiders: " + Num(g.dailyKills) + "/" + Num(dailyGoal) + (g.dailyDone ? " DONE" : "");
	s += "   Weekly: destroy 60: " + Num(g.weeklyKills) + "/60" + (g.weeklyDone ? " DONE" : "");
	return s;
}

// ---- login streaks and the weekly leaderboard -----------------------------------------------------------------

/// Leaderboard weeks start on Monday 00:00 UTC (the Unix epoch was a Thursday, hence the + 3)
int CBotManager::BoardWeekOf(long t) {
	return (int)((t / 86400 + 3) / 7);
}

void CBotManager::AddScore(const std::string &name, int n) {
	if (n <= 0) return;
	const std::string k = Key(name);
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (!IsHuman(i) || Key(p->Player[i]->Name) != k) continue;
		if (p->Player[i]->guest) return;   // guests leave nothing behind, so they are not ranked
		if (boardWeek == 0) boardWeek = BoardWeekOf((long)time(NULL));
		weekScore[k] += n;
		dirty = true;
		return;
	}
}

std::string CBotManager::BoardText(int count, const char *eol) {
	std::vector<std::pair<int, std::string> > rows;
	for (std::map<std::string, int>::iterator it = weekScore.begin(); it != weekScore.end(); ++it) rows.push_back(std::make_pair(-it->second, it->first));
	std::sort(rows.begin(), rows.end());
	std::string s;
	for (int i = 0; i < (int)rows.size() && i < count; i++) s += "  " + Num(i + 1) + ". " + rows[i].second + "  " + Num(-rows[i].first) + eol;
	return s;
}

void CBotManager::TellBoard(int idx) {
	std::string top = BoardText(5, "\n");
	Tell(idx, "Weekly leaderboard (solo score, resets Monday 00:00 UTC):");
	if (top.empty()) Tell(idx, "  Nobody has scored yet this week: kill raiders, finish missions and trials to get on it.");
	std::istringstream ls(top);
	std::string line;
	while (std::getline(ls, line)) Tell(idx, line);
	const std::string k = Key(p->Player[idx]->Name);
	if (p->Player[idx]->guest) Tell(idx, "Guests are not ranked: log in with an account to take part.");
	else if (weekScore.count(k)) {
		int rank = 1;
		for (std::map<std::string, int>::iterator it = weekScore.begin(); it != weekScore.end(); ++it) if (it->second > weekScore[k]) rank++;
		Tell(idx, "You: #" + Num(rank) + " with " + Num(weekScore[k]) + " points.");
	}
	else Tell(idx, "You have no score yet this week.");
	if (!lastWinnerName.empty()) Tell(idx, "Last week's winner: " + lastWinnerName + " with " + Num(lastWinnerScore) + ".");
}

/// Called every manage tick: when the week changes, the leader of the finished week is announced and the board starts over
void CBotManager::RollBoard() {
	int week = BoardWeekOf((long)time(NULL));
	if (boardWeek == week) return;
	if (boardWeek != 0 && !weekScore.empty()) {
		std::string best; int bestScore = 0;
		for (std::map<std::string, int>::iterator it = weekScore.begin(); it != weekScore.end(); ++it) {
			if (it->second > bestScore) { best = it->first; bestScore = it->second; }
		}
		if (!best.empty()) {
			lastWinnerName = best; lastWinnerScore = bestScore; lastWinnerWeek = boardWeek;
			Event("Weekly leaderboard won by " + best + " with " + Num(bestScore));
			for (int i = 0; i < MAX_PLAYERS; i++) {
				if (IsHuman(i) && Key(p->Player[i]->Name) == best) {
					p->Account->AddPoints(i, 25);
					Tell(i, "You won last week's leaderboard with " + Num(bestScore) + "! +25 points.");
				}
			}
			TellAll("Last week's leaderboard was won by " + best + " with " + Num(bestScore) + " points. A new week begins: /top");
			// the winner is a registered player name (letters, digits, underscore), safe to put in the JSON body
			std::string safe;
			for (size_t i = 0; i < best.size(); i++) if (isalnum((unsigned char)best[i]) || best[i] == '_' || best[i] == '-') safe += best[i];
			Discord("Battle City weekly leaderboard won by " + safe + " with " + Num(bestScore) + " points. A new week starts now: bc.outiva.com", true);
		}
	}
	weekScore.clear();
	boardWeek = week;
	dirty = true;
}

/// The first time a registered player is seen this session: count the day in their streak and pay the reward
void CBotManager::LoginStreak(int idx) {
	if (p->Player[idx]->guest) return;
	const std::string k = Key(p->Player[idx]->Name);
	int day = (int)((long)time(NULL) / 86400);
	std::map<std::string, Streak>::iterator it = streaks.find(k);
	if (it == streaks.end()) {
		Streak s; s.lastDay = 0; s.count = 0; s.best = 0;
		it = streaks.insert(std::make_pair(k, s)).first;
	}
	Streak &s = it->second;
	if (s.lastDay == day) {
		Tell(idx, "Login streak: " + Num(s.count) + " days (best " + Num(s.best) + "). Come back tomorrow to keep it going.");
		return;
	}
	s.count = (s.lastDay == day - 1) ? s.count + 1 : 1;
	s.lastDay = day;
	if (s.count > s.best) s.best = s.count;
	int pts = std::min(s.count, 7);
	if (s.count % 7 == 0) pts += 10;
	p->Account->AddPoints(idx, pts);
	AddScore(p->Player[idx]->Name, 2);
	dirty = true;
	if (s.count == 1) Tell(idx, "Daily login: +" + Num(pts) + " point. Come back tomorrow to start a streak (more points each day, up to 7, and a bonus every 7th day).");
	else Tell(idx, "Login streak: day " + Num(s.count) + "! +" + Num(pts) + " points" + (s.count % 7 == 0 ? " (weekly streak bonus)" : "") + ". Keep it going tomorrow.");
	if (s.count == 7 || s.count == 30 || s.count == 100) Event(p->Player[idx]->Name + " has logged in " + Num(s.count) + " days in a row");
}

/// The panel at the bottom right of the client shows these lines: the week's top five, then this player's own standing
void CBotManager::SendBoards(float now) {
	if (now < nextBoard) return;
	nextBoard = now + 5000.0f;
	std::vector<std::pair<int, std::string> > rows;
	for (std::map<std::string, int>::iterator it = weekScore.begin(); it != weekScore.end(); ++it) rows.push_back(std::make_pair(-it->second, it->first));
	std::sort(rows.begin(), rows.end());
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (!IsHuman(i)) continue;
		std::string text = "WEEKLY TOP";
		for (int r = 0; r < (int)rows.size() && r < 5; r++) text += "\n" + Num(r + 1) + ". " + rows[r].second.substr(0, 14) + " " + Num(-rows[r].first);
		if (rows.empty()) text += "\nNo scores yet";
		const std::string k = Key(p->Player[i]->Name);
		if (p->Player[i]->guest) text += "\nGuests are not ranked";
		else {
			int mine = weekScore.count(k) ? weekScore[k] : 0, rank = 1;
			for (size_t r = 0; r < rows.size(); r++) if (-rows[r].first > mine) rank++;
			text += mine > 0 ? "\nYou: #" + Num(rank) + " " + Num(mine) : std::string("\nYou: no score yet");
			std::map<std::string, Streak>::iterator st = streaks.find(k);
			if (st != streaks.end() && st->second.count > 1 && st->second.lastDay >= (int)((long)time(NULL) / 86400) - 1) text += "\nStreak: " + Num(st->second.count) + " days";
		}
		char buf[255];
		int len = (int)std::min<size_t>(text.size(), 250);
		memcpy(buf, text.data(), len);
		p->Winsock->SendData(i, smLeaderboard, buf, len);
	}
}

void CBotManager::GiveBotPoints(int idx, int pts) {
	if (pts <= 0 || !IsHuman(idx)) return;
	Progress &g = ProgressFor(idx);
	int room = DAILY_BOT_POINT_CAP - g.botPoints;
	if (room <= 0) return;
	if (pts > room) pts = room;
	g.botPoints += pts;
	p->Account->AddPoints(idx, pts);
	dirty = true;
}

void CBotManager::GivePoints(const std::string &name, int pts) {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (IsHuman(i) && Lower(p->Player[i]->Name) == Lower(name)) {
			p->Account->AddPoints(i, pts);
			return;
		}
	}
}

int CBotManager::Points(const BotInfo &b) const {
	switch (b.tier) {
		case BOT_SCOUT: return 1;
		case BOT_HEAVY: return 3;
		case BOT_BOSS: return 10;
		case BOT_DRONE: return 1;
		default: return 0;
	}
}

float CBotManager::Speed(const BotInfo &b) const {
	switch (b.tier) {
		case BOT_SCOUT: return 0.30f;
		case BOT_HEAVY: return 0.20f;
		case BOT_BOSS: return 0.17f;
		case BOT_DRONE: return 0.14f;
		case BOT_GHOST: return 0.0f;
		default: return 0.0f;
	}
}

BotInfo *CBotManager::BotBySlot(int slot) {
	for (int i = 0; i < 24; i++) if (bots[i].used && bots[i].slot == slot) return &bots[i];
	return NULL;
}

int CBotManager::CountTier(int tier) const {
	int n = 0;
	for (int i = 0; i < 24; i++) if (bots[i].used && bots[i].tier == tier && bots[i].raidCity < 0) n++;
	return n;
}

int CBotManager::CountRaiders() const {
	int n = 0;
	for (int i = 0; i < 24; i++) if (bots[i].used && bots[i].raidCity >= 0 && !bots[i].dead) n++;
	return n;
}

// ---- spawning and removing bots ------------------------------------------------------------------------------

int CBotManager::SpawnBot(int tier, float x, float y, int raidCity) {
	int idx = -1;
	for (int i = 0; i < 24; i++) if (!bots[i].used) { idx = i; break; }
	if (idx < 0) return -1;
	int slot = AllocSlot();
	if (slot < 0) return -1;
	EnsureOverseer();
	BotInfo &b = bots[idx];
	b.used = true;
	b.slot = slot;
	b.tier = tier;
	b.dead = false;
	b.permanent = (tier == BOT_BOSS || tier == BOT_DUMMY || tier == BOT_GHOST);
	b.raidCity = raidCity;
	b.x = x; b.y = y;
	b.dir = rand() % 32;
	b.goalX = x; b.goalY = y;
	b.nextThink = b.nextShot = b.nextUpdate = b.nextTurn = b.nextGoal = 0;
	b.respawnAt = 0;
	b.fleeUntil = 0;
	b.lastX = x; b.lastY = y; b.stuckSince = 0;
	b.target = -1;
	b.lastHitBy = -1;
	b.ghostIndex = 0;
	b.ghostNext = 0;
	b.aimX = b.aimY = 0;
	b.offX = b.offY = 0;
	b.avoidUntil = 0;
	b.avoidDir = 8;
	b.path.clear(); b.pathIdx = 0; b.nextPath = 0; b.pathGX = b.pathGY = 0;
	b.steerX = x; b.steerY = y; b.nextSteer = 0;
	b.expiresAt = 0;
	b.owner = -1;
	b.damageBy.clear();
	switch (tier) {
		case BOT_SCOUT: b.maxHp = 15; break;
		case BOT_HEAVY: b.maxHp = 40; break;
		case BOT_BOSS: b.maxHp = 150; break;
		case BOT_DRONE: b.maxHp = 10; break;
		case BOT_DUMMY: b.maxHp = 10; break;
		default: b.maxHp = 1; break;
	}
	b.hp = b.maxHp;

	static const char *scoutNames[] = { "Raider Rex", "Raider Vex", "Raider Zed", "Raider Kip", "Raider Nox", "Raider Jax", "Raider Bo", "Raider Max" };
	static const char *heavyNames[] = { "Brute Gor", "Brute Mak", "Brute Tusk", "Brute Rok" };
	CPlayer *pl = p->Player[slot];
	pl->IsBot = true;
	pl->State = State_Game;
	pl->City = botCity;
	pl->playerType = 1;
	pl->Member = 0;
	pl->Mayor = 0;
	pl->isDead = false;
	pl->isCloaked = false;
	pl->X = x; pl->Y = y;
	pl->Points = pl->Orbs = pl->Assists = pl->Deaths = pl->MonthlyPoints = 0;
	pl->lastMove = p->Tick;
	switch (tier) {
		case BOT_SCOUT: pl->Name = scoutNames[rand() % 8]; pl->Town = "Raiders"; pl->displayTank = pl->Tank = 4; pl->Red = 255; pl->Green = 90; pl->Blue = 180; break;
		case BOT_HEAVY: pl->Name = heavyNames[rand() % 4]; pl->Town = "Raiders"; pl->displayTank = pl->Tank = 12; pl->Red = 120; pl->Green = 200; pl->Blue = 90; break;
		case BOT_BOSS: pl->Name = "Warlord"; pl->Town = "Bounty"; pl->displayTank = pl->Tank = 3; pl->Red = 255; pl->Green = 60; pl->Blue = 60; break;
		case BOT_DRONE: pl->Name = "Snowman"; pl->Town = "Wildlife"; pl->displayTank = pl->Tank = 9; pl->Red = 235; pl->Green = 245; pl->Blue = 255; break;
		case BOT_DUMMY: pl->Name = "Target"; pl->Town = "Range"; pl->displayTank = pl->Tank = 7; pl->Red = 200; pl->Green = 200; pl->Blue = 200; break;
		case BOT_GHOST: pl->Name = ("Ghost:" + ghostName).substr(0, 15); pl->Town = "Echoes"; pl->displayTank = pl->Tank = 10; pl->Red = 200; pl->Green = 200; pl->Blue = 255; break;
	}
	SendBotToAll(b);
	return idx;
}

void CBotManager::SendBotToAll(BotInfo &b) {
	CPlayer *pl = p->Player[b.slot];
	sSMPlayer pk;
	memset(&pk, 0, sizeof(pk));
	pk.Index = b.slot;
	strncpy(pk.Name, pl->Name.c_str(), 15);
	strncpy(pk.Town, pl->Town.c_str(), 15);
	pk.playerType = pl->playerType; pk.Red = pl->Red; pk.Green = pl->Green; pk.Blue = pl->Blue; pk.Member = 0; pk.Tank = pl->displayTank;
	p->Send->SendAllBut(-1, smPlayerData, (char *)&pk, sizeof(pk));
	sSMPoints pts;
	memset(&pts, 0, sizeof(pts));
	pts.Index = b.slot;
	p->Send->SendAllBut(-1, smPointsUpdate, (char *)&pts, sizeof(pts));
	sSMJoinData jd;
	jd.id = b.slot; jd.Mayor = 0; jd.City = botCity;
	p->Send->SendGameAllBut(-1, smJoinData, (char *)&jd, sizeof(jd));
	SendUpdate(b);
}

void CBotManager::SendUpdate(BotInfo &b) {
	sSMUpdate u;
	memset(&u, 0, sizeof(u));
	u.id = (unsigned char)b.slot;
	u.x = (unsigned short)std::max(0.0f, std::min(65000.0f, b.x));
	u.y = (unsigned short)std::max(0.0f, std::min(65000.0f, b.y));
	u.turn = 1;
	u.move = (Speed(b) > 0 && !b.dead && (fabsf(b.goalX - b.x) + fabsf(b.goalY - b.y)) > 24) ? 2 : 1;
	u.dir = (unsigned char)b.dir;
	p->Send->SendRadarNotIndex(b.slot, smUpdate, (char *)&u, sizeof(u));
}

void CBotManager::RemoveBot(BotInfo &b) {
	if (!b.used) return;
	ReleaseSlot(b.slot);
	b.used = false;
}

void CBotManager::RemoveAllBots() {
	for (int i = 0; i < 24; i++) if (bots[i].used) RemoveBot(bots[i]);
	if (overseer >= 0) {
		if (CCity::isValidCityIndex(botCity) && p->City[botCity]->Mayor == overseer) {
			p->City[botCity]->Mayor = -1;
			p->City[botCity]->notHiring = 0;
		}
		if (p->Player[overseer]->IsBot) ReleaseSlot(overseer);
		overseer = -1;
	}
}

void CBotManager::KillBot(BotInfo &b, int killer) {
	if (b.dead) return;
	b.dead = true;
	b.hp = 0;
	CPlayer *pl = p->Player[b.slot];
	pl->isDead = true;
	pl->timeDeath = p->Tick;
	char pk[3];
	pk[0] = (char)b.slot;
	pk[1] = 0;
	pk[2] = (killer >= 0 && killer < MAX_PLAYERS) ? (char)p->Player[killer]->City : (char)botCity;
	p->Send->SendGameAllBut(-1, smDeath, pk, 3);
	if (killer >= 0) AwardKill(b, killer);
	if (b.permanent) {
		// gone for good: remove it a moment later so its explosion plays first
		b.respawnAt = Now() + 1500.0f;
	}
	else {
		b.respawnAt = Now() + (b.tier == BOT_HEAVY ? 14000.0f : (b.tier == BOT_DRONE ? 20000.0f : 8000.0f));
	}
}

void CBotManager::RespawnBot(BotInfo &b) {
	float nx, ny;
	float anchorX = b.x, anchorY = b.y;
	float dist;
	int h = NearestHuman(b.x, b.y, &dist, false);
	if (h >= 0) { anchorX = p->Player[h]->X; anchorY = p->Player[h]->Y; }
	if (!FindSpawn(anchorX, anchorY, 1100, 2400, &nx, &ny)) { b.respawnAt = Now() + 3000.0f; return; }
	b.x = nx; b.y = ny; b.goalX = nx; b.goalY = ny;
	b.path.clear(); b.steerX = nx; b.steerY = ny; b.nextSteer = 0;
	b.hp = b.maxHp;
	b.dead = false;
	b.target = -1;
	b.lastHitBy = -1;
	b.fleeUntil = 0;
	b.damageBy.clear();
	CPlayer *pl = p->Player[b.slot];
	pl->isDead = false;
	pl->X = nx; pl->Y = ny;
	char pk[2] = { (char)b.slot, 0 };
	p->Send->SendAllBut(-1, smRespawn, pk, 1);
	SendUpdate(b);
}

// ---- damage and rewards --------------------------------------------------------------------------------------

void CBotManager::BulletHitBot(int botSlot, int damage, int shooter) {
	BotInfo *b = BotBySlot(botSlot);
	if (!b || b->dead || b->tier == BOT_GHOST) return;
	if (b->tier != BOT_DUMMY && b->tier != BOT_DRONE) p->Waypoints->NoteFight(b->x, b->y);
	if (shooter >= 0 && !IsHuman(shooter)) return;
	b->hp -= damage;
	b->lastHitBy = shooter;
	if (shooter >= 0) b->damageBy[Lower(p->Player[shooter]->Name)] += damage;
	if (b->tier == BOT_DRONE) b->fleeUntil = Now() + 3000.0f;
	if (b->hp <= 0) KillBot(*b, shooter);
}

void CBotManager::AwardKill(BotInfo &b, int killer) {
	if (!IsHuman(killer)) return;
	const std::string name = Lower(p->Player[killer]->Name);

	if (b.tier == BOT_DUMMY) {
		// range: count down, report the time when the last target falls
		if (range[killer].active) {
			range[killer].left--;
			if (range[killer].left <= 0) CheckRange(killer, Now());
		}
		return;
	}

	if (b.tier == BOT_SCOUT || b.tier == BOT_HEAVY || b.tier == BOT_BOSS) AddScore(name, Points(b));
	if (b.tier != BOT_BOSS) GiveBotPoints(killer, Points(b));   // the boss pays through the shared bounty below

	if (b.tier == BOT_SCOUT || b.tier == BOT_HEAVY || b.tier == BOT_BOSS) {
		Progress &g = ProgressFor(killer);
		int dailyGoal = 8 + 4 * (g.day % 3);
		g.dailyKills++;
		g.weeklyKills++;
		dirty = true;
		if (!g.dailyDone && g.dailyKills >= dailyGoal) {
			g.dailyDone = true;
			p->Account->AddPoints(killer, 8);
			AddScore(name, 5);
			Tell(killer, "Daily challenge complete! +8 points. Come back tomorrow for a new one.");
			Event(p->Player[killer]->Name + " completed the daily challenge");
		}
		if (!g.weeklyDone && g.weeklyKills >= 60) {
			g.weeklyDone = true;
			p->Account->AddPoints(killer, 40);
			AddScore(name, 20);
			Tell(killer, "WEEKLY challenge complete! +40 points.");
			Event(p->Player[killer]->Name + " completed the weekly challenge");
		}
		if (raid.active && b.raidCity == raid.city) raid.killers.insert(name);
		if (mission.active) {
			mission.participants.insert(name);
			if (mission.type == 0) mission.progress++;
		}
	}

	if (b.tier == BOT_BOSS) {
		// shared bounty: everyone who hurt it gets paid, and a supply drop lands where it fell
		for (std::map<std::string, int>::iterator it = b.damageBy.begin(); it != b.damageBy.end(); ++it) {
			if (it->first != name && it->second >= 10) { GivePoints(it->first, 6); AddScore(it->first, 5); }
		}
		p->Account->AddPoints(killer, 10);
		TellAll("The Warlord has been destroyed by " + p->Player[killer]->Name + "! Everyone who hit it earns the bounty.");
		Event("Warlord destroyed by " + p->Player[killer]->Name);
		int type = (rand() % 2) ? ITEM_TYPE_MEDKIT : ITEM_TYPE_ROCKET;
		DropCrate(killer, type, b.x, b.y, true);
		DropCrate(killer, ITEM_TYPE_CLOAK, b.x + 96, b.y, false);
		if (mission.active && mission.type == 1) { mission.progress = mission.goal; mission.participants.insert(name); }
	}
}

// ---- AI --------------------------------------------------------------------------------------------------------

void CBotManager::Fire(BotInfo &b) {
	int type = 0;
	if (b.tier == BOT_HEAVY) type = 1;
	if (b.tier == BOT_BOSS) type = 2;
	float fDir = (float)-b.dir + 32;
	int fy = (int)b.y - 24 + 10 + (int)(cosf(fDir / 16 * PI_GAME) * 20);
	int fx = (int)b.x - 24 + 6 + (int)(sinf(fDir / 16 * PI_GAME) * 20);
	if (fx < 0 || fy < 0 || fx > 24000 || fy > 24000) return;
	p->Bullet->newBullet(fx, fy, type, b.dir, b.slot);
	sSMShot shot;
	shot.id = (unsigned short)b.slot;
	shot.x = (unsigned short)fx;
	shot.y = (unsigned short)fy;
	shot.dir = (unsigned char)b.dir;
	shot.type = (unsigned char)type;
	p->Send->SendRadarNotIndex(b.slot, smShoot, (char *)&shot, sizeof(shot));
}

void CBotManager::Think(BotInfo &b, float now) {
	if (b.tier == BOT_DUMMY || b.tier == BOT_GHOST) return;

	// A short list of what the bot wants: flee if hurt, else fight a human it can see, else raid or wander
	float dist = 1e9f;
	int h = NearestHuman(b.x, b.y, &dist, true);
	float detect = (b.tier == BOT_BOSS) ? 1500.0f : (b.tier == BOT_HEAVY ? 900.0f : 1000.0f);
	b.target = -1;

	// Inside a human city's safe zone (and not raiding): head back out and never fight there
	if (b.raidCity < 0) {
		float ax, ay;
		if (InSafeZone(b.x, b.y, &ax, &ay)) { b.goalX = ax; b.goalY = ay; b.nextGoal = now + 3000.0f; return; }
		if (h >= 0 && InSafeZone(p->Player[h]->X, p->Player[h]->Y)) h = -1;   // a player at home is left alone
	}

	if (b.tier == BOT_DRONE) {
		// harmless: wander, and run from whoever hurt it
		if (now < b.fleeUntil && b.lastHitBy >= 0 && IsHuman(b.lastHitBy)) {
			float dx = b.x - p->Player[b.lastHitBy]->X, dy = b.y - p->Player[b.lastHitBy]->Y;
			b.goalX = b.x + dx * 4; b.goalY = b.y + dy * 4;
			return;
		}
	}
	else if (h >= 0 && dist < detect) {
		bool hurt = b.hp * 4 < b.maxHp && b.tier != BOT_BOSS;
		if (hurt && now < b.fleeUntil + 4000.0f) {
			if (b.fleeUntil < now) b.fleeUntil = now + 3500.0f;
			float dx = b.x - p->Player[h]->X, dy = b.y - p->Player[h]->Y;
			b.goalX = b.x + dx * 3; b.goalY = b.y + dy * 3;
			return;
		}
		b.target = h;
		float tx = p->Player[h]->X, ty = p->Player[h]->Y;
		// keep a fighting distance rather than ramming: stop at a short range, back-circle when too close
		float keep = (b.tier == BOT_HEAVY) ? 420.0f : (b.tier == BOT_BOSS ? 520.0f : 300.0f);
		if (dist > keep) { b.goalX = tx; b.goalY = ty; }
		else { b.goalX = b.x; b.goalY = b.y; }
		return;
	}

	// raiders go for their city's buildings
	if (b.raidCity >= 0 && b.tier != BOT_DRONE) {
		CBuilding *best = NULL;
		float bestD = 1e12f;
		for (CBuilding *bld = p->Build->buildingListHead; bld; bld = bld->next) {
			if (bld->City != b.raidCity || bld->isCC()) continue;
			float dx = bld->x * 48 - b.x, dy = bld->y * 48 - b.y;
			float d = dx * dx + dy * dy;
			if (d < bestD) { bestD = d; best = bld; }
		}
		if (best) {
			float dx = best->x * 48 - b.x, dy = best->y * 48 - b.y;
			float d = sqrtf(dx * dx + dy * dy);
			if (d > 460.0f) { b.goalX = best->x * 48 - dx / d * 380.0f; b.goalY = best->y * 48 - dy / d * 380.0f; }
			else { b.goalX = b.x; b.goalY = b.y; b.target = -2; b.aimX = (float)best->x * 48; b.aimY = (float)best->y * 48; }
			return;
		}
	}

	// wander
	if (now > b.nextGoal || (fabsf(b.goalX - b.x) + fabsf(b.goalY - b.y)) < 40.0f) {
		for (int tries = 0; tries < 12; tries++) {
			float a = Rand01() * 6.2832f, d = 500.0f + Rand01() * 1400.0f;
			float gx = b.x + cosf(a) * d, gy = b.y + sinf(a) * d;
			if (FreeSpot(gx, gy) && (b.raidCity >= 0 || !InSafeZone(gx, gy))) { b.goalX = gx; b.goalY = gy; break; }
		}
		b.nextGoal = now + 6000.0f + Rand01() * 6000.0f;
	}
}

// ---- finding a way round obstacles ------------------------------------------------------------------------------
// Bots used to head straight for their goal and sidestep at random when they bumped into something, so one that met a lake
// or a rock wall between itself and a player could stay in front of it for ever. When the straight line is blocked a route
// over the tile grid (A*, 8 directions, no corner cutting) is planned and followed, planned again now and then.

bool CBotManager::LineClear(float x0, float y0, float x1, float y1) const {
	float dx = x1 - x0, dy = y1 - y0;
	int n = (int)(sqrtf(dx * dx + dy * dy) / 24.0f) + 1;
	for (int i = 1; i <= n; i++) {
		if (Blocked(x0 + dx * i / n, y0 + dy * i / n)) return false;
	}
	return true;
}

bool CBotManager::FindPath(BotInfo &b) {
	const int N = 512;
	static std::vector<unsigned char> blocked;
	static std::vector<float> cost;
	static std::vector<int> parent;
	static std::vector<unsigned> seen;
	static unsigned stamp = 0;
	if (blocked.empty()) { blocked.resize(N * N); cost.resize(N * N); parent.resize(N * N); seen.assign(N * N, 0); }

	// what a tank cannot stand on: rock, water, lava, the rim of the map and buildings
	for (int x = 0; x < N; x++) {
		for (int y = 0; y < N; y++) {
			blocked[x * N + y] = (p->Map->map[x][y] != 0 || x < 2 || y < 2 || x > 509 || y > 509) ? 1 : 0;
		}
	}
	for (CBuilding *bld = p->Build->buildingListHead; bld; bld = bld->next) {
		int x0 = bld->x - 2, y0 = bld->y - 2, w = 3, h = 3;
		if (bld->type / 100 <= 2) { y0 = bld->y - 1; h = 2; }
		for (int x = std::max(0, x0); x < std::min(N, x0 + w); x++) for (int y = std::max(0, y0); y < std::min(N, y0 + h); y++) blocked[x * N + y] = 1;
	}

	auto tileOf = [](float v) { return (int)floorf(v / 48.0f + 0.5f); };
	int sx = std::max(0, std::min(N - 1, tileOf(b.x))), sy = std::max(0, std::min(N - 1, tileOf(b.y)));
	int gx = std::max(0, std::min(N - 1, tileOf(b.pathGX))), gy = std::max(0, std::min(N - 1, tileOf(b.pathGY)));
	auto h = [&](int x, int y) {
		float dx = (float)abs(x - gx), dy = (float)abs(y - gy);
		return 1.2f * ((dx + dy) + (1.4142f - 2.0f) * std::min(dx, dy));
	};

	stamp++;
	typedef std::pair<float, int> Node;
	std::priority_queue<Node, std::vector<Node>, std::greater<Node> > open;
	int start = sx * N + sy;
	cost[start] = 0; parent[start] = -1; seen[start] = stamp;
	open.push(Node(h(sx, sy), start));
	static const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	static const int DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
	int found = -1, expanded = 0;
	while (!open.empty() && expanded < 60000) {
		Node top = open.top(); open.pop();
		int cur = top.second, cx = cur / N, cy = cur % N;
		if (top.first > cost[cur] + h(cx, cy) + 0.001f) continue;   // an out-of-date entry
		expanded++;
		if (abs(cx - gx) <= 1 && abs(cy - gy) <= 1) { found = cur; break; }
		for (int k = 0; k < 8; k++) {
			int nx = cx + DX[k], ny = cy + DY[k];
			if (nx < 0 || ny < 0 || nx >= N || ny >= N || blocked[nx * N + ny]) continue;
			if (k >= 4 && (blocked[(cx + DX[k]) * N + cy] || blocked[cx * N + cy + DY[k]])) continue;   // no cutting corners
			float nc = cost[cur] + (k >= 4 ? 1.4142f : 1.0f);
			int ni = nx * N + ny;
			if (seen[ni] == stamp && cost[ni] <= nc) continue;
			seen[ni] = stamp; cost[ni] = nc; parent[ni] = cur;
			open.push(Node(nc + h(nx, ny), ni));
		}
	}
	b.path.clear();
	b.pathIdx = 0;
	if (found < 0) return false;
	for (int cur = found; cur != -1 && cur != start; cur = parent[cur]) b.path.push_back(cur);
	std::reverse(b.path.begin(), b.path.end());
	return !b.path.empty();
}

void CBotManager::UpdateSteer(BotInfo &b, float now) {
	if (now < b.nextSteer) return;
	b.nextSteer = now + 200.0f;
	b.steerX = b.goalX; b.steerY = b.goalY;
	float gdx = b.goalX - b.x, gdy = b.goalY - b.y;
	if (gdx * gdx + gdy * gdy < 100.0f * 100.0f || LineClear(b.x, b.y, b.goalX, b.goalY)) { b.path.clear(); return; }

	// the straight line is blocked: follow a route, planning a new one when there is none, the goal moved or it got old
	float mx = b.goalX - b.pathGX, my = b.goalY - b.pathGY;
	bool stale = b.path.empty() || b.pathIdx >= (int)b.path.size() || now >= b.nextPath || mx * mx + my * my > 300.0f * 300.0f;
	if (stale && pathBudget > 0) {
		pathBudget--;
		b.pathGX = b.goalX; b.pathGY = b.goalY;
		b.nextPath = now + 4000.0f;
		if (!FindPath(b)) b.nextPath = now + 1500.0f;     // no way (or too far to search): the sidestep logic keeps trying meanwhile
	}
	if (b.path.empty()) return;
	auto wx = [&](int i) { return (float)(b.path[i] / 512) * 48.0f; };
	auto wy = [&](int i) { return (float)(b.path[i] % 512) * 48.0f; };
	while (b.pathIdx + 1 < (int)b.path.size() && fabsf(wx(b.pathIdx) - b.x) + fabsf(wy(b.pathIdx) - b.y) < 50.0f) b.pathIdx++;
	int best = b.pathIdx;
	for (int k = b.pathIdx + 1; k < (int)b.path.size() && k <= b.pathIdx + 8; k++) {
		if (LineClear(b.x, b.y, wx(k), wy(k))) best = k; else break;
	}
	b.steerX = wx(best); b.steerY = wy(best);
}

void CBotManager::StepBot(BotInfo &b, float dt, float now) {
	if (b.dead) return;

	if (b.tier == BOT_GHOST) {
		// replay: step through the recorded samples once a second, gliding between them
		if (ghostPath.size() < 2) return;
		if (now >= b.ghostNext) {
			b.ghostIndex = (b.ghostIndex + 1) % (int)ghostPath.size();
			b.ghostNext = now + 1000.0f;
		}
		const Sample &s = ghostPath[b.ghostIndex];
		// the path is translated so it starts near whoever is online (goal holds the offset)
		float tx = s.x + b.offX, ty = s.y + b.offY;
		float dx = tx - b.x, dy = ty - b.y;
		float d = sqrtf(dx * dx + dy * dy);
		if (d > 600.0f) { b.x = tx; b.y = ty; }
		else if (d > 2.0f) { float st = std::min(d, 0.38f * dt * 1.2f); b.x += dx / d * st; b.y += dy / d * st; b.dir = DirTo(dx, dy); }
		p->Player[b.slot]->X = b.x; p->Player[b.slot]->Y = b.y;
		return;
	}

	float speed = Speed(b);
	// turn towards the goal (or the target while shooting)
	UpdateSteer(b, now);
	float dgoal = fabsf(b.goalX - b.x) + fabsf(b.goalY - b.y);
	float dx = b.steerX - b.x, dy = b.steerY - b.y;      // head for the goal, or the next bend of the route round an obstacle
	int want = b.dir;
	bool aim = false;
	// a detour round an obstacle is under way: drive the route (a tank only moves the way it faces) and do not shoot at the wall
	bool detour = b.steerX != b.goalX || b.steerY != b.goalY;
	if (!detour && b.target >= 0 && IsHuman(b.target)) {
		want = DirTo(p->Player[b.target]->X - b.x, p->Player[b.target]->Y - b.y);
		aim = true;
	}
	else if (!detour && b.target == -2) {
		want = DirTo(b.aimX - b.x, b.aimY - b.y);
		aim = true;
	}
	else if (dgoal > 24.0f) want = DirTo(dx, dy);
	// just ran into rock, water or a building: head off to one side for a moment, then try the direct line again
	if (now < b.avoidUntil && !aim) want = (want + b.avoidDir + 32) % 32;

	if (now >= b.nextTurn && want != b.dir) {
		int diff = ((want - b.dir + 16) % 32 + 32) % 32 - 16;
		b.dir = (b.dir + (diff > 0 ? 1 : -1) + 32) % 32;
		b.nextTurn = now + TURN_MS;
	}
	int diff = ((want - b.dir + 16) % 32 + 32) % 32 - 16;
	bool facing = abs(diff) <= 1;

	// move when heading roughly the right way
	if (speed > 0 && dgoal > 24.0f && (facing || !aim) && abs(diff) <= 4) {
		float fDir = (float)-b.dir + 32;
		float nx = b.x + sinf(fDir / 16 * PI_GAME) * speed * dt;
		float ny = b.y + cosf(fDir / 16 * PI_GAME) * speed * dt;
		if (!Blocked(nx, ny)) { b.x = nx; b.y = ny; }
		else if (!Blocked(nx, b.y)) { b.x = nx; }
		else if (!Blocked(b.x, ny)) { b.y = ny; }
		else {
			// boxed in: sidestep (alternating sides) and keep the goal; a wanderer also picks somewhere new
			b.avoidUntil = now + 1000.0f + (float)(rand() % 900);
			b.avoidDir = -b.avoidDir;
			if (b.raidCity < 0) { b.nextGoal = 0; b.goalX = b.x; b.goalY = b.y; }
		}
	}
	p->Player[b.slot]->X = b.x;
	p->Player[b.slot]->Y = b.y;

	// stuck detector: wanted to move but barely did for two seconds
	if (dgoal > 60.0f) {
		float mx = b.x - b.lastX, my = b.y - b.lastY;
		if (b.stuckSince == 0) { b.stuckSince = now; b.lastX = b.x; b.lastY = b.y; }
		else if (now - b.stuckSince > 2000.0f) {
			if (mx * mx + my * my < 30.0f * 30.0f) {
				b.avoidUntil = now + 1500.0f + (float)(rand() % 1000);
				b.avoidDir = (rand() % 2) ? 8 : -8;
				b.nextPath = 0; b.nextSteer = 0;     // plan the route again
				if (b.raidCity < 0) { b.nextGoal = 0; b.goalX = b.x; b.goalY = b.y; }
			}
			b.stuckSince = 0;
		}
	}
	else b.stuckSince = 0;

	// shooting
	bool wantsFire = false;
	if (b.tier == BOT_SCOUT || b.tier == BOT_HEAVY || b.tier == BOT_BOSS) {
		if (b.target >= 0 && IsHuman(b.target)) {
			float ddx = p->Player[b.target]->X - b.x, ddy = p->Player[b.target]->Y - b.y;
			float dd = sqrtf(ddx * ddx + ddy * ddy);
			float reach = (b.tier == BOT_SCOUT) ? 520.0f : (b.tier == BOT_HEAVY ? 640.0f : 720.0f);
			wantsFire = facing && dd < reach && !detour;
		}
		else if (b.target == -2) wantsFire = facing;   // raider in range of a building
	}
	if (wantsFire && now >= b.nextShot) {
		Fire(b);
		float base = (b.tier == BOT_SCOUT) ? 1500.0f : (b.tier == BOT_HEAVY ? 2000.0f : 1600.0f);
		b.nextShot = now + base + Rand01() * 500.0f;
	}
}

// ---- main loop ------------------------------------------------------------------------------------------------

void CBotManager::Cycle() {
	if (!enabled) return;
	float now = Now();
	if (now < nextStep) return;
	float dt = lastStepTick > 0 ? now - lastStepTick : STEP_MS;
	if (dt > 200.0f) dt = 200.0f;
	lastStepTick = now;
	nextStep = now + STEP_MS;
	pathBudget = 2;

	bool any = false;
	for (int i = 0; i < 24; i++) {
		BotInfo &b = bots[i];
		if (!b.used) continue;
		any = true;
		// the player may have left the slot (server shut it down): drop our record
		if (!p->Player[b.slot]->IsBot) { b.used = false; continue; }
		if (b.dead) {
			if (now >= b.respawnAt) {
				if (b.permanent) RemoveBot(b);
				else RespawnBot(b);
			}
			continue;
		}
		if (b.expiresAt > 0 && now > b.expiresAt) { RemoveBot(b); continue; }
		if (now >= b.nextThink) { Think(b, now); b.nextThink = now + 120.0f + Rand01() * 80.0f; }
		StepBot(b, dt, now);
		if (now >= b.nextUpdate) { SendUpdate(b); b.nextUpdate = now + UPDATE_MS; }
	}
	(void)any;

	TurretsFire(now);

	if (now >= nextManage) {
		Manage(now);
		nextManage = now + 1000.0f;
	}
	if (dirty && now >= nextSave) {
		Save();
		nextSave = now + 30000.0f;
	}
}

// Turrets are simulated by the clients, whose bullets never reach the server, so bots (which live on the server)
// were never hurt by them. Fire the city's guns at bots here: same range and damage as the client turret.
void CBotManager::TurretsFire(float now) {
	if (now < nextTurret) return;
	nextTurret = now + 500.0f;
	for (CItem *itm = p->Item->itemListHead; itm; itm = itm->next) {
		if (itm->holder != -1 || !itm->active) continue;
		int dmg;
		if (itm->type == ITEM_TYPE_TURRET || itm->type == ITEM_TYPE_SLEEPER) dmg = 4;
		else if (itm->type == ITEM_TYPE_PLASMA) dmg = 5;
		else continue;
		float ix = (float)itm->x * 48.0f, iy = (float)itm->y * 48.0f;
		BotInfo *best = NULL;
		float bestD = 360.0f;
		for (int i = 0; i < 24; i++) {
			BotInfo &b = bots[i];
			if (!b.used || b.dead || b.tier == BOT_GHOST || itm->City == botCity) continue;
			float d = sqrtf((b.x - ix) * (b.x - ix) + (b.y - iy) * (b.y - iy));
			if (d < bestD) { bestD = d; best = &b; }
		}
		if (!best) continue;
		// credit the city's mayor when they are online
		int shooter = -1;
		if (CCity::isValidCityIndex(itm->City)) {
			int m = p->City[itm->City]->Mayor;
			if (m >= 0 && IsHuman(m)) shooter = m;
		}
		BulletHitBot(best->slot, dmg, shooter);
	}
}

void CBotManager::Manage(float now) {
	int humans = CountHumans();
	RollBoard();

	// Nobody here: nothing to entertain. Take everything down so an empty server stays idle.
	if (humans == 0) {
		if (lastHumanCount > 0) {
			RemoveAllBots();
			raid.active = false;
			if (mission.active) EndMission(false);
			for (int i = 0; i < 4; i++) drops[i].active = false;
			if (dirty) Save();
		}
		lastHumanCount = 0;
		nextRaid = nextMission = nextBounty = nextDrop = 0;
		return;
	}

	if (lastHumanCount == 0) {
		// first player has just arrived: schedule the first events a little way off
		nextRaid = now + 150000.0f * tempo;
		nextMission = now + 90000.0f * tempo;
		nextBounty = now + 240000.0f * tempo;
		nextDrop = now + 60000.0f * tempo;
	}
	lastHumanCount = humans;

	ManageFighters(now, humans);
	ManageRaid(now, humans);
	ManageMission(now, humans);
	ManageBounty(now, humans);
	ManageDrops(now, humans);
	ManageGhost(now, humans);
	SendBoards(now);

	// range and trial timeouts
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (range[i].active) CheckRange(i, now);
		if (trial[i].active) {
			if (!IsHuman(i) || now - trial[i].startedAt > 180000.0f) {
				if (IsHuman(i)) Tell(i, "Trial timed out.");
				trial[i].active = false;
			}
			else if (now >= trial[i].nextHint) {
				trial[i].nextHint = now + 5000.0f;
				float dx = trial[i].cx[trial[i].step] - p->Player[i]->X, dy = trial[i].cy[trial[i].step] - p->Player[i]->Y;
				Tell(i, "Trial checkpoint " + Num(trial[i].step + 1) + "/4: " + Num((long)(sqrtf(dx * dx + dy * dy) / 48)) + " tiles " + Compass(dx, dy));
			}
		}
	}
}

void CBotManager::ManageFighters(float now, int humans) {
	// how many fighting bots we want for this many humans: a lone player gets the full set, a crowd needs none
	int wanted = 0;
	if (humans == 1) wanted = maxBots;
	else if (humans == 2) wanted = (maxBots * 3 + 3) / 4;
	else if (humans == 3) wanted = maxBots / 2;
	else if (humans <= 5) wanted = maxBots / 4;
	if (mission.active && mission.type == 0) wanted = std::min(MAX_FIGHTERS_HARD, wanted + 2);
	int heavies = wanted >= 3 ? std::max(1, wanted / 4) : 0;
	int scouts = wanted - heavies;

	// anchor spawns on a random human so there is always something to meet
	int anchor = -1;
	int pick = rand() % humans, seen = 0;
	for (int i = 0; i < MAX_PLAYERS; i++) if (IsHuman(i)) { if (seen++ == pick) { anchor = i; break; } }
	if (anchor < 0) return;
	float ax = p->Player[anchor]->X, ay = p->Player[anchor]->Y, sx, sy;

	if (CountTier(BOT_SCOUT) < scouts && FindSpawn(ax, ay, 1100, 2300, &sx, &sy)) SpawnBot(BOT_SCOUT, sx, sy);
	else if (CountTier(BOT_HEAVY) < heavies && FindSpawn(ax, ay, 1300, 2400, &sx, &sy)) SpawnBot(BOT_HEAVY, sx, sy);
	else if (CountTier(BOT_DRONE) < std::min(3, (maxBots + 1) / 2) && FindSpawn(ax, ay, 700, 1800, &sx, &sy)) SpawnBot(BOT_DRONE, sx, sy);

	// Trim extras when a crowd arrives, and bots that wandered far away from every human
	for (int i = 0; i < 24; i++) {
		BotInfo &b = bots[i];
		if (!b.used || b.dead || b.raidCity >= 0 || b.tier == BOT_BOSS || b.tier == BOT_DUMMY || b.tier == BOT_GHOST) continue;
		float dist;
		NearestHuman(b.x, b.y, &dist, false);
		bool excess = (b.tier == BOT_SCOUT && CountTier(BOT_SCOUT) > scouts) || (b.tier == BOT_HEAVY && CountTier(BOT_HEAVY) > heavies);
		if (excess || dist > 6500.0f) { RemoveBot(b); break; }
	}
}

// ---- raids ----------------------------------------------------------------------------------------------------

void CBotManager::ManageRaid(float now, int humans) {
	if (raid.active) {
		bool alive = CountRaiders() > 0;
		if (!alive && raid.spawned > 0) EndRaid(true);
		else if (now > raid.endsAt) EndRaid(false);
		return;
	}
	if (now < nextRaid) return;

	// candidates: a human city that has buildings other than its command centre
	std::vector<int> cities;
	for (int c = 0; c < MAX_CITIES; c++) {
		if (c == botCity || p->City[c]->Mayor == -1) continue;
		bool humanThere = false;
		for (int i = 0; i < MAX_PLAYERS; i++) if (IsHuman(i) && p->Player[i]->City == c) humanThere = true;
		if (!humanThere) continue;
		int n = 0;
		for (CBuilding *bld = p->Build->buildingListHead; bld; bld = bld->next) if (bld->City == c && !bld->isCC()) n++;
		if (n >= 1) cities.push_back(c);
	}
	nextRaid = now + (1200000.0f + Rand01() * 600000.0f) * tempo;
	if (cities.empty()) { nextRaid = now + 120000.0f * tempo; return; }

	int city = cities[rand() % cities.size()];
	int inCity = 0;
	for (int i = 0; i < MAX_PLAYERS; i++) if (IsHuman(i) && p->Player[i]->City == city) inCity++;
	int count = 2 + (inCity >= 2 ? 1 : 0) + (inCity >= 3 ? 1 : 0);
	float cx = (float)p->City[city]->x, cy = (float)p->City[city]->y;
	float sx, sy;
	if (!FindSpawn(cx, cy, 1500, 2200, &sx, &sy)) { nextRaid = now + 60000.0f * tempo; return; }

	raid.active = true;
	raid.city = city;
	raid.endsAt = now + 360000.0f;
	raid.spawned = 0;
	raid.killers.clear();
	for (int k = 0; k < count; k++) {
		float rx = sx + (float)(rand() % 200 - 100), ry = sy + (float)(rand() % 200 - 100);
		if (!FreeSpot(rx, ry)) { rx = sx; ry = sy; }
		int tier = (k == count - 1 && inCity >= 2) ? BOT_HEAVY : BOT_SCOUT;
		if (SpawnBot(tier, rx, ry, city) >= 0) raid.spawned++;
	}
	if (raid.spawned == 0) { raid.active = false; return; }
	std::string town = p->Player[p->City[city]->Mayor]->Town;
	if (town.empty()) town = "your city";
	TellEachRel("RAID! " + Num(raid.spawned) + " raiders are attacking " + town + " from the " + Compass(sx - cx, sy - cy) + ". The city is ", cx, cy, ". Defend it!");
	Event("Raiders attacked " + town);
}

void CBotManager::EndRaid(bool success) {
	if (!raid.active) return;
	raid.active = false;
	int city = raid.city;
	std::string town = (p->City[city]->Mayor >= 0 && p->City[city]->Mayor < MAX_PLAYERS) ? p->Player[p->City[city]->Mayor]->Town : "the city";
	if (success) {
		for (std::set<std::string>::iterator it = raid.killers.begin(); it != raid.killers.end(); ++it) { GivePoints(*it, 3); AddScore(*it, 5); }
		TellAll("The raid on " + town + " was repelled. Defenders earn 3 points.");
		Event("Raid on " + town + " repelled");
	}
	else {
		TellAll("The raiders have withdrawn from " + town + ".");
		Event("Raiders withdrew from " + town);
	}
	for (int i = 0; i < 24; i++) if (bots[i].used && bots[i].raidCity >= 0) { bots[i].raidCity = -1; bots[i].expiresAt = Now() + 4000.0f; }
}

// ---- co-op missions --------------------------------------------------------------------------------------------

void CBotManager::StartMission(int type, int humans) {
	mission.active = true;
	mission.type = type;
	mission.progress = 0;
	mission.heldMs = 0;
	mission.participants.clear();
	mission.nextWave = Now() + 20000.0f;
	mission.nextHint = Now() + 45000.0f;
	if (type == 0) {
		mission.goal = 6 + 2 * humans;
		mission.endsAt = Now() + 300000.0f;
		TellAll("MISSION: Raider Sweep. Destroy " + Num(mission.goal) + " raiders within 5 minutes. Everyone who takes part earns 5 points.");
	}
	else if (type == 1) {
		mission.goal = 1;
		mission.endsAt = Now() + 480000.0f;
		bool have = false;
		for (int i = 0; i < 24; i++) if (bots[i].used && bots[i].tier == BOT_BOSS) have = true;
		if (!have) {
			int h = NearestHuman(0, 0, NULL, false);
			float sx, sy;
			if (h >= 0 && FindSpawn(p->Player[h]->X, p->Player[h]->Y, 1500, 2600, &sx, &sy)) {
				int bi = SpawnBot(BOT_BOSS, sx, sy);
				if (bi >= 0) bots[bi].expiresAt = mission.endsAt;
			}
		}
		TellAll("MISSION: Boss Hunt. A Warlord is on the map. Destroy it within 8 minutes; everyone who takes part earns 5 points.");
	}
	else {
		mission.goal = 75;
		mission.endsAt = Now() + 360000.0f;
		int h = NearestHuman(0, 0, NULL, false);
		float zx = 0, zy = 0;
		if (h < 0 || !FindSpawn(p->Player[h]->X, p->Player[h]->Y, 800, 1500, &zx, &zy)) { mission.active = false; return; }
		mission.zoneX = zx; mission.zoneY = zy;
		TellEachRel("MISSION: Hold the Zone, ", zx, zy, ". Stay close to it for " + Num(mission.goal) + " seconds while raiders try to break you. Everyone who takes part earns 5 points. /where shows the way.");
	}
	Event("Mission started");
}

void CBotManager::EndMission(bool success) {
	if (!mission.active) return;
	mission.active = false;
	if (success) {
		for (std::set<std::string>::iterator it = mission.participants.begin(); it != mission.participants.end(); ++it) { GivePoints(*it, 5); AddScore(*it, 10); }
		TellAll("MISSION COMPLETE! " + Num((long)mission.participants.size()) + " player(s) earned 5 points.");
		Event("Mission completed with " + Num((long)mission.participants.size()) + " player(s)");
	}
	else {
		TellAll("Mission failed. Another will come.");
		Event("Mission failed");
	}
	nextMission = Now() + (480000.0f + Rand01() * 300000.0f) * tempo;
}

void CBotManager::ManageMission(float now, int humans) {
	if (!mission.active) {
		if (now >= nextMission) {
			static int rotate = 0;
			StartMission(rotate++ % 3, humans);
		}
		return;
	}

	if (mission.type == 2) {
		// the zone: count the seconds that somebody is inside; spawn defenders' attackers on a timer
		bool inside = false;
		for (int i = 0; i < MAX_PLAYERS; i++) {
			if (!IsHuman(i) || p->Player[i]->isDead) continue;
			float dx = p->Player[i]->X - mission.zoneX, dy = p->Player[i]->Y - mission.zoneY;
			if (dx * dx + dy * dy < 280.0f * 280.0f) { inside = true; mission.participants.insert(Lower(p->Player[i]->Name)); }
		}
		if (inside) mission.heldMs += 1000.0f;
		mission.progress = (int)(mission.heldMs / 1000.0f);
		if (now >= mission.nextWave && CountTier(BOT_SCOUT) + CountRaiders() < MAX_FIGHTERS_HARD) {
			mission.nextWave = now + 35000.0f;
			float sx, sy;
			for (int k = 0; k < 2; k++) {
				if (FindSpawn(mission.zoneX, mission.zoneY, 1000, 1500, &sx, &sy)) {
					int bi = SpawnBot(BOT_SCOUT, sx, sy);
					if (bi >= 0) { bots[bi].expiresAt = now + 120000.0f; bots[bi].goalX = mission.zoneX; bots[bi].goalY = mission.zoneY; }
				}
			}
		}
		if (now >= mission.nextHint) {
			mission.nextHint = now + 45000.0f;
			TellEachRel("Zone: " + Num(mission.progress) + "/" + Num(mission.goal) + " s held. It is ", mission.zoneX, mission.zoneY, ".");
		}
	}
	else if (mission.type == 1 && now >= mission.nextHint) {
		mission.nextHint = now + 60000.0f;
		for (int i = 0; i < 24; i++) {
			if (bots[i].used && bots[i].tier == BOT_BOSS && !bots[i].dead) {
				TellEachRel("Warlord last seen ", bots[i].x, bots[i].y, ".");
			}
		}
	}

	if (mission.progress >= mission.goal) EndMission(true);
	else if (now > mission.endsAt) EndMission(false);
}

// ---- bounty boss -----------------------------------------------------------------------------------------------

void CBotManager::ManageBounty(float now, int humans) {
	for (int i = 0; i < 24; i++) if (bots[i].used && bots[i].tier == BOT_BOSS) {
		if (bots[i].dead) return;
		// expired bosses are removed in Cycle (expiresAt); tell the players once
		if (bots[i].expiresAt > 0 && now > bots[i].expiresAt - 1500.0f && now <= bots[i].expiresAt) { TellAll("The Warlord escaped."); Event("The Warlord escaped"); bots[i].expiresAt = now + 100.0f; }
		return;
	}
	if (now < nextBounty || (mission.active && mission.type == 1)) return;
	int h = NearestHuman(0, 0, NULL, false);
	float sx, sy;
	nextBounty = now + (900000.0f + Rand01() * 600000.0f) * tempo;
	if (h < 0 || !FindSpawn(p->Player[h]->X, p->Player[h]->Y, 1800, 3000, &sx, &sy)) return;
	int bi = SpawnBot(BOT_BOSS, sx, sy);
	if (bi < 0) return;
	bots[bi].expiresAt = now + 600000.0f;
	TellEachRel("BOUNTY: a Warlord (150 HP) roams the map, ", sx, sy, ". Destroy it for 10 points and a supply drop. /where shows the way.");
	Event("A Warlord is on the loose");
}

// ---- supply crates -----------------------------------------------------------------------------------------------

void CBotManager::DropCrate(int humanSlot, int itemType, float x, float y, bool announce) {
	if (!IsHuman(humanSlot)) return;
	int slotIdx = -1;
	for (int i = 0; i < 4; i++) if (!drops[i].active) { slotIdx = i; break; }
	if (slotIdx < 0) return;
	int tx = (int)(x / 48), ty = (int)(y / 48);
	if (tx < 3 || ty < 3 || tx > 508 || ty > 508) return;
	// an unblocked tile, nudging until one is found
	bool ok = false;
	for (int tries = 0; tries < 20 && !ok; tries++) {
		int ox = tx + (tries == 0 ? 0 : rand() % 5 - 2), oy = ty + (tries == 0 ? 0 : rand() % 5 - 2);
		if (ox < 3 || oy < 3 || ox > 508 || oy > 508) continue;
		if (!Blocked((float)(ox * 48), (float)(oy * 48))) { tx = ox; ty = oy; ok = true; }
	}
	if (!ok) return;
	int city = p->Player[humanSlot]->City;
	sSMItem item;
	memset(&item, 0, sizeof(item));
	item.x = (unsigned short)tx; item.y = (unsigned short)ty;
	item.City = (unsigned char)city; item.type = (unsigned char)itemType; item.active = 0;
	item.id = (unsigned short)(p->Item->itmID++);
	if (p->Item->itmID > 30000) p->Item->itmID = 1;
	p->Item->newItem(tx, ty, itemType, city, item.id);
	p->Send->SendSectorArea(tx * 48, ty * 48, smAddItem, (char *)&item, sizeof(item));
	drops[slotIdx].active = true;
	drops[slotIdx].id = item.id;
	drops[slotIdx].x = tx; drops[slotIdx].y = ty;
	drops[slotIdx].city = city;
	drops[slotIdx].expiresAt = Now() + 360000.0f;
	if (announce) {
		static const char *names[] = { "cloak", "rocket", "medkit" };
		std::string what = (itemType >= 0 && itemType <= 2) ? names[itemType] : "supplies";
		for (int i = 0; i < MAX_PLAYERS; i++)
			if (IsHuman(i) && p->Player[i]->City == city)
				Tell(i, "SUPPLY CRATE (" + what + "): " + Rel(i, (float)(tx * 48), (float)(ty * 48)) + " (tile " + Num(tx) + "," + Num(ty) + "). Your city can pick it up for 6 minutes.");
	}
}

void CBotManager::ExpireDrops(float now) {
	for (int i = 0; i < 4; i++) {
		if (!drops[i].active) continue;
		CItem *itm = p->Item->findItem((unsigned short)drops[i].id);
		if (!itm || itm->holder != -1) { drops[i].active = false; continue; }   // picked up or gone
		if (now > drops[i].expiresAt) {
			p->Item->delItem(itm);
			drops[i].active = false;
		}
	}
}

void CBotManager::ManageDrops(float now, int humans) {
	ExpireDrops(now);
	if (now < nextDrop) return;
	nextDrop = now + (420000.0f + Rand01() * 240000.0f) * tempo;
	int pick = rand() % humans, seen = 0, h = -1;
	for (int i = 0; i < MAX_PLAYERS; i++) if (IsHuman(i)) { if (seen++ == pick) { h = i; break; } }
	if (h < 0) return;
	float a = Rand01() * 6.2832f, d = 900.0f + Rand01() * 1500.0f;
	int types[3] = { ITEM_TYPE_MEDKIT, ITEM_TYPE_CLOAK, ITEM_TYPE_ROCKET };
	DropCrate(h, types[rand() % 3], p->Player[h]->X + cosf(a) * d, p->Player[h]->Y + sinf(a) * d, true);
}

// ---- ghost -------------------------------------------------------------------------------------------------------

void CBotManager::ManageGhost(float now, int humans) {
	bool have = false;
	for (int i = 0; i < 24; i++) if (bots[i].used && bots[i].tier == BOT_GHOST) have = true;
	if (have || ghostPath.size() < 20) return;
	int h = NearestHuman(0, 0, NULL, false);
	if (h < 0) return;
	// translate the recorded path so that it begins about a thousand pixels from the player
	float px = p->Player[h]->X + 900.0f, py = p->Player[h]->Y + 300.0f;
	float offX = px - ghostPath[0].x, offY = py - ghostPath[0].y;
	for (size_t i = 0; i < ghostPath.size(); i++) {
		float x = ghostPath[i].x + offX, y = ghostPath[i].y + offY;
		if (x < 200 || y < 200 || x > 511 * 48 - 200 || y > 511 * 48 - 200) { offX = 0; offY = 0; break; }
	}
	int bi = SpawnBot(BOT_GHOST, ghostPath[0].x + offX, ghostPath[0].y + offY);
	if (bi < 0) return;
	bots[bi].offX = offX;
	bots[bi].offY = offY;
}

// ---- practice: range and trial -------------------------------------------------------------------------------------

void CBotManager::StartRange(int idx) {
	if (range[idx].active) { Tell(idx, "Your range is already running: clear all the targets."); return; }
	int placed = 0;
	float cx = p->Player[idx]->X, cy = p->Player[idx]->Y;
	for (int k = 0; k < 8 && placed < 6; k++) {
		float a = (float)k / 8.0f * 6.2832f + Rand01() * 0.3f, d = 380.0f + Rand01() * 160.0f;
		float x = cx + cosf(a) * d, y = cy + sinf(a) * d;
		if (!FreeSpot(x, y)) continue;
		int bi = SpawnBot(BOT_DUMMY, x, y);
		if (bi < 0) continue;
		bots[bi].owner = idx;
		bots[bi].expiresAt = Now() + 180000.0f;
		placed++;
	}
	if (placed < 3) { Tell(idx, "No room for a range here: try open ground."); for (int i = 0; i < 24; i++) if (bots[i].used && bots[i].owner == idx && bots[i].tier == BOT_DUMMY) RemoveBot(bots[i]); return; }
	range[idx].active = true;
	range[idx].startedAt = Now();
	range[idx].endsAt = Now() + 180000.0f;
	range[idx].total = placed;
	range[idx].left = placed;
	Tell(idx, "Range open: destroy all " + Num(placed) + " targets around you. The clock is running.");
}

void CBotManager::CheckRange(int idx, float now) {
	if (!range[idx].active) return;
	if (range[idx].left <= 0) {
		int ms = (int)(now - range[idx].startedAt);
		range[idx].active = false;
		std::string n = Lower(p->Player[idx]->Name);
		bool record = !rangeBest.count(n) || ms < rangeBest[n];
		if (record) { rangeBest[n] = ms; dirty = true; }
		Tell(idx, "Range cleared in " + Num(ms / 1000) + "." + Num((ms % 1000) / 100) + " s" + (record ? " (personal best!)" : "") + ".");
		return;
	}
	if (now > range[idx].endsAt || !IsHuman(idx)) {
		range[idx].active = false;
		for (int i = 0; i < 24; i++) if (bots[i].used && bots[i].tier == BOT_DUMMY && bots[i].owner == idx) RemoveBot(bots[i]);
		if (IsHuman(idx)) Tell(idx, "Range closed: time is up.");
	}
}

void CBotManager::StartTrial(int idx) {
	if (trial[idx].active) { Tell(idx, "Trial already running. Use /trial stop to cancel."); return; }
	float x = p->Player[idx]->X, y = p->Player[idx]->Y;
	for (int k = 0; k < 4; k++) {
		bool ok = false;
		for (int tries = 0; tries < 40 && !ok; tries++) {
			float a = Rand01() * 6.2832f, d = 500.0f + Rand01() * 800.0f;
			float cx = x + cosf(a) * d, cy = y + sinf(a) * d;
			if (FreeSpot(cx, cy)) { trial[idx].cx[k] = cx; trial[idx].cy[k] = cy; x = cx; y = cy; ok = true; }
		}
		if (!ok) { Tell(idx, "No good course around here: move to open ground and try again."); return; }
	}
	trial[idx].active = true;
	trial[idx].step = 0;
	trial[idx].startedAt = Now();
	trial[idx].nextHint = Now() + 1500.0f;
	Tell(idx, "Trial started: reach 4 checkpoints in order, as fast as you can. Directions follow.");
}

void CBotManager::TrialProgress(int idx) {
	Trial &t = trial[idx];
	float dx = p->Player[idx]->X - t.cx[t.step], dy = p->Player[idx]->Y - t.cy[t.step];
	if (dx * dx + dy * dy > 90.0f * 90.0f) return;
	t.step++;
	if (t.step < 4) {
		Tell(idx, "Checkpoint " + Num(t.step) + " reached.");
		t.nextHint = Now() + 500.0f;
		return;
	}
	int ms = (int)(Now() - t.startedAt);
	t.active = false;
	std::string n = Lower(p->Player[idx]->Name);
	bool record = !trialBest.count(n) || ms < trialBest[n];
	std::string line = p->Player[idx]->Name + " finished the trial in " + Num(ms / 1000) + "." + Num((ms % 1000) / 100) + " s";
	if (record) { trialBest[n] = ms; dirty = true; line += " (personal best)"; AddScore(n, 5); }
	TellAll(line);
	Event(line);
}

// ---- chat commands -----------------------------------------------------------------------------------------------

bool CBotManager::HandleChat(int idx, const char *text) {
	if (!enabled || !text || text[0] != '/' || !IsHuman(idx)) return false;
	std::string line(text);
	std::istringstream ls(line.substr(1));
	std::string cmd, arg;
	ls >> cmd >> arg;
	cmd = Lower(cmd);
	arg = Lower(arg);

	if (cmd == "help" || cmd == "solo") {
		Tell(idx, "Solo play commands: /where  /bots  /challenge  /mission  /range  /trial  /ghost  /events  /top  /streak");
		Tell(idx, "Raiders, a bounty Warlord, supply crates and missions appear while you play. Kill raiders for points (capped daily).");
		return true;
	}
	// Test hook (BC_TEST_TURRET=1): put a turret of city 0 next to the nearest target so the tests can check turret damage
	if (cmd == "testturret" && getenv("BC_TEST_TURRET")) {
		BotInfo *best = NULL; float bestD = 1e9f;
		for (int i = 0; i < 24; i++) {
			if (!bots[i].used || bots[i].dead || bots[i].tier != BOT_DUMMY) continue;
			float d = hypotf(bots[i].x - p->Player[idx]->X, bots[i].y - p->Player[idx]->Y);
			if (d < bestD) { bestD = d; best = &bots[i]; }
		}
		if (!best) { Tell(idx, "No target."); return true; }
		CItem *t = p->Item->newItem((int)(best->x / 48) + 1, (int)(best->y / 48), ITEM_TYPE_TURRET, 0, (unsigned short)(p->Item->itmID++));
		t->active = 1;
		Tell(idx, "Turret placed.");
		return true;
	}
	// Test hook (BC_TEST_TURRET=1): "/testbot dx dy" puts a scout dx,dy tiles from the player (the path test)
	if (cmd == "testbot" && getenv("BC_TEST_TURRET")) {
		int dx = 0, dy = 0;
		std::istringstream as(line.substr(1)); std::string skip; as >> skip >> dx >> dy;
		int bi = SpawnBot(BOT_SCOUT, p->Player[idx]->X + dx * 48.0f, p->Player[idx]->Y + dy * 48.0f);
		Tell(idx, bi < 0 ? std::string("No bot.") : "Test bot " + Num(bots[bi].slot));
		return true;
	}
	if (cmd == "bots") {
		int s = CountTier(BOT_SCOUT), hv = CountTier(BOT_HEAVY), d = CountTier(BOT_DRONE) , r = CountRaiders();
		bool boss = false;
		for (int i = 0; i < 24; i++) if (bots[i].used && bots[i].tier == BOT_BOSS && !bots[i].dead) boss = true;
		Tell(idx, "Bots: " + Num(s) + " scouts, " + Num(hv) + " heavies, " + Num(d) + " snowmen, " + Num(r) + " raiders" + (boss ? ", a Warlord" : "") + ".");
		return true;
	}
	if (cmd == "challenge" || cmd == "challenges") {
		Tell(idx, DailyText(idx));
		return true;
	}
	if (cmd == "top" || cmd == "leaderboard" || cmd == "board") { TellBoard(idx); return true; }
	if (cmd == "streak") {
		LoginStreak(idx);
		return true;
	}
	if (cmd == "mission") {
		if (!mission.active) { Tell(idx, "No mission right now. The next one starts soon."); return true; }
		static const char *names[] = { "Raider Sweep", "Boss Hunt", "Hold the Zone" };
		Tell(idx, std::string("Mission: ") + names[mission.type] + ", " + Num(mission.progress) + "/" + Num(mission.goal) + ", " + Num((long)std::max(0.0f, (mission.endsAt - Now()) / 1000)) + " s left." + (mission.type == 2 ? " The zone is " + Rel(idx, mission.zoneX, mission.zoneY) + "." : ""));
		return true;
	}
	if (cmd == "where") {
		Tell(idx, "You are at tile " + Num((long)(p->Player[idx]->X / 48)) + "," + Num((long)(p->Player[idx]->Y / 48)) + " (also shown under the radar).");
		bool any = false;
		if (mission.active && mission.type == 2) { Tell(idx, "Zone: " + Rel(idx, mission.zoneX, mission.zoneY) + "."); any = true; }
		for (int i = 0; i < 24; i++) if (bots[i].used && bots[i].tier == BOT_BOSS && !bots[i].dead) { Tell(idx, "Warlord: " + Rel(idx, bots[i].x, bots[i].y) + "."); any = true; }
		for (int i = 0; i < 4; i++) if (drops[i].active && drops[i].city == p->Player[idx]->City) { Tell(idx, "Supply crate: " + Rel(idx, (float)(drops[i].x * 48), (float)(drops[i].y * 48)) + " (tile " + Num(drops[i].x) + "," + Num(drops[i].y) + ")."); any = true; }
		if (raid.active) { Tell(idx, "Raid on city " + Num(raid.city) + ": " + Rel(idx, (float)p->City[raid.city]->x, (float)p->City[raid.city]->y) + "."); any = true; }
		if (!any) Tell(idx, "No objective is active right now.");
		return true;
	}
	if (cmd == "range") { StartRange(idx); return true; }
	if (cmd == "trial") {
		if (arg == "stop") { trial[idx].active = false; Tell(idx, "Trial cancelled."); }
		else if (arg == "best") {
			std::string n = Lower(p->Player[idx]->Name);
			if (trialBest.count(n)) Tell(idx, "Your best trial: " + Num(trialBest[n] / 1000) + "." + Num((trialBest[n] % 1000) / 100) + " s");
			else Tell(idx, "No trial time yet: type /trial to start one.");
			if (rangeBest.count(n)) Tell(idx, "Your best range: " + Num(rangeBest[n] / 1000) + "." + Num((rangeBest[n] % 1000) / 100) + " s");
		}
		else StartTrial(idx);
		return true;
	}
	if (cmd == "ghost") {
		if (ghostPath.empty()) Tell(idx, "No ghost yet: the best recent run of a player becomes the ghost.");
		else Tell(idx, "The ghost replays the run of " + ghostName + " (" + Num(ghostPoints) + " points).");
		return true;
	}
	if (cmd == "events") {
		for (int i = (int)events.size() - 1; i >= 0 && i >= (int)events.size() - 4; i--) Tell(idx, events[i]);
		if (events.empty()) Tell(idx, "No events recorded yet.");
		return true;
	}
	return false;
}

// ---- player hooks ----------------------------------------------------------------------------------------------------

void CBotManager::Discord(const std::string &text, bool force) {
#ifndef WIN32
	if (webhook.empty()) return;
	if (webhook.compare(0, 8, "https://") != 0) return;
	if (!force && Now() - lastWebhook < 1800000.0f) return;
	lastWebhook = Now();
	// fork/exec (never a shell) so nothing in the text or URL can run as a command; the name was already reduced to safe characters
	std::string body = "{\"content\":\"" + text + "\"}";
	pid_t pid = fork();
	if (pid == 0) {
		execlp("curl", "curl", "-s", "-m", "10", "-H", "Content-Type: application/json", "-d", body.c_str(), webhook.c_str(), (char *)NULL);
		_exit(0);
	}
	else if (pid > 0) {
		signal(SIGCHLD, SIG_IGN);   // no zombies
	}
#else
	(void)text; (void)force;
#endif
}

void CBotManager::HumanJoined(int idx) {
	if (!enabled) return;
	std::string safe;
	for (size_t i = 0; i < p->Player[idx]->Name.size(); i++) {
		char c = p->Player[idx]->Name[i];
		if (isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-') safe += c;
	}
	if (CountHumans() == 1) Discord(safe + " is online in Battle City. Join in at bc.outiva.com");
	recording[idx].clear();
	recordNext[idx] = 0;
	greeted.erase(idx);
}

void CBotManager::HumanLeft(int idx) {
	if (!enabled) return;
	lastOnlineName = p->Player[idx]->Name;
	for (size_t i = 0; i < lastOnlineName.size(); i++) if (lastOnlineName[i] == ' ') lastOnlineName[i] = '_';
	lastOnlineTime = (long)time(NULL);
	dirty = true;
	trial[idx].active = false;
	if (range[idx].active) { range[idx].active = false; for (int i = 0; i < 24; i++) if (bots[i].used && bots[i].tier == BOT_DUMMY && bots[i].owner == idx) RemoveBot(bots[i]); }
	// keep the run if it is long enough and from a player at least as good as the current ghost's owner
	std::vector<Sample> &rec = recording[idx];
	if (rec.size() >= (tempo < 1.0f ? 20u : 45u) && p->Player[idx]->Points >= ghostPoints) {
		ghostPath = rec;
		ghostName = p->Player[idx]->Name;
		for (size_t i = 0; i < ghostName.size(); i++) if (ghostName[i] == ' ') ghostName[i] = '_';
		ghostPoints = p->Player[idx]->Points;
		SaveGhost();
		Event("New ghost recorded from " + p->Player[idx]->Name);
	}
	recording.erase(idx);
	recordNext.erase(idx);
	greeted.erase(idx);
}

void CBotManager::HumanUpdated(int idx) {
	if (!enabled) return;
	float now = Now();
	if (!greeted.count(idx)) {
		greeted.insert(idx);
		Tell(idx, "Quiet server? Type /help for solo play: raiders, bounties, missions, a range and a time trial. /top shows the weekly leaderboard.");
		LoginStreak(idx);
	}
	if (trial[idx].active) TrialProgress(idx);
	if (now >= recordNext[idx]) {
		recordNext[idx] = now + std::max(100.0f, 1000.0f * tempo);   // (shortened with BC_BOTS_TEMPO so tests need seconds, not a minute)
		std::vector<Sample> &rec = recording[idx];
		Sample s;
		s.x = p->Player[idx]->X; s.y = p->Player[idx]->Y; s.dir = 0;
		if (!rec.empty()) s.dir = DirTo(s.x - rec.back().x, s.y - rec.back().y);
		rec.push_back(s);
		if (rec.size() > 300) rec.erase(rec.begin());
	}
}


/// The places that matter to one player right now: a supply crate for their city (or the latest one for somebody else's),
/// a raid on their city (the nearest raider), the bounty boss, the mission zone and the next time-trial gate.
void CBotManager::Objectives(int slot, std::vector<WaypointInfo> &out) {
	if (!enabled || slot < 0 || slot >= MAX_PLAYERS) return;
	int city = p->Player[slot]->City;
	int other = -1;
	for (int i = 0; i < 4; i++) {
		if (!drops[i].active) continue;
		if (drops[i].city == city) out.push_back(WaypointInfo{ WP_CRATE, (float)(drops[i].x * 48), (float)(drops[i].y * 48) });
		else if (other < 0 || drops[i].expiresAt > drops[other].expiresAt) other = i;
	}
	if (other >= 0) out.push_back(WaypointInfo{ WP_DROP, (float)(drops[other].x * 48), (float)(drops[other].y * 48) });
	if (raid.active && raid.city == city) {
		float px = p->Player[slot]->X, py = p->Player[slot]->Y, best = 1e30f;
		const BotInfo *nearest = NULL;
		for (int i = 0; i < 24; i++) {
			const BotInfo &b = bots[i];
			if (!b.used || b.dead || b.raidCity != city) continue;
			float d = (b.x - px) * (b.x - px) + (b.y - py) * (b.y - py);
			if (d < best) { best = d; nearest = &b; }
		}
		if (nearest) out.push_back(WaypointInfo{ WP_RAID, nearest->x, nearest->y });
	}
	for (int i = 0; i < 24; i++) {
		const BotInfo &b = bots[i];
		if (b.used && !b.dead && b.tier == BOT_BOSS) { out.push_back(WaypointInfo{ WP_BOSS, b.x, b.y }); break; }
	}
	if (mission.active && mission.type == 2) out.push_back(WaypointInfo{ WP_ZONE, mission.zoneX, mission.zoneY });
	if (trial[slot].active && trial[slot].step >= 0 && trial[slot].step < 4) out.push_back(WaypointInfo{ WP_GATE, trial[slot].cx[trial[slot].step], trial[slot].cy[trial[slot].step] });
}
