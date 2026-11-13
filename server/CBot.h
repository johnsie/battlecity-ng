/*
===============================================================================

    Battle City - CBot header file
    Server-side bots and the events built around them (raids, missions, drops,
    challenges, trials). See docs/solo-play.md.

    This file is part of Battle City.

    Battle City is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
===============================================================================
*/
#ifndef _CBOT
#define _CBOT

#include <map>
#include <set>
#include <string>
#include <vector>

#include "CServer.h"

class CServer;

enum BotTier {
	BOT_DUMMY,      // stationary practice target
	BOT_DRONE,      // neutral wandering "snowman", harmless
	BOT_SCOUT,      // fast, laser
	BOT_HEAVY,      // slow, tough, rockets
	BOT_BOSS,       // bounty target, plasma
	BOT_GHOST       // replays a recorded run, harmless and untouchable
};

struct BotInfo {
	bool used;
	int slot;
	int tier;
	int hp, maxHp;
	int dir;                         // 0..31, same as a player's Direction
	float x, y;
	float goalX, goalY;
	float nextThink, nextShot, nextUpdate, nextTurn, respawnAt, nextGoal;
	float fleeUntil;
	float lastX, lastY, stuckSince;
	int target;                      // human slot being chased, or -1
	int lastHitBy;                   // human slot that last damaged it, or -1
	bool dead;
	bool permanent;                  // not respawned when killed (boss, dummies)
	int raidCity;                    // >= 0 for raiders: the human city they are attacking
	int ghostIndex;                  // ghost: position in the recorded path
	float ghostNext;
	float aimX, aimY;                // raider in range of a building: where to point
	float offX, offY;                // ghost: translation applied to the recorded path
	float avoidUntil;                // after bumping into terrain: steer sideways until then
	int avoidDir;                    // +8 or -8 direction steps
	float expiresAt;                 // 0 = never
	int owner;                       // dummy: the human who asked for the range
	std::map<std::string, int> damageBy;   // boss: damage per human name (for shared rewards)
};

struct Sample { float x, y; int dir; };

/// An important place for one player (see CWaypoints): type is a WP_ value
struct WaypointInfo { int type; float x, y; };

struct Progress {
	int day;           // challenge day number the daily counters belong to
	int dailyKills;
	bool dailyDone;
	int week;
	int weeklyKills;
	bool weeklyDone;
	int pointsDay;     // day number the bot-point counter belongs to
	int botPoints;     // points earned from bots today (capped)
};

struct Streak {
	int lastDay;       // day number of the last day the player logged in
	int count;         // consecutive days including lastDay
	int best;          // longest streak so far
};

class CBotManager {

	public:
		CBotManager(CServer *Server);
		~CBotManager();

		void Init();
		void Cycle();
		void Shutdown();
		void ResetState();   // the server is resetting (nobody is connected): drop every bot and event in flight

		bool Enabled() const { return enabled; }
		bool IsBotSlot(int slot) const;

		// Hooks called by the rest of the server
		void BulletHitBot(int botSlot, int damage, int shooter);
		bool HandleChat(int idx, const char *text);       // true if the line was a bot command and is consumed
		void HumanJoined(int idx);
		void HumanLeft(int idx);
		void HumanUpdated(int idx);
		void Objectives(int slot, std::vector<WaypointInfo> &out);   // this player's crate, raid, boss, zone and trial gate
		std::string RecentEvents();                       // text appended to the login-screen news

	private:
		CServer *p;
		bool enabled;
		int maxBots;
		float safeRadius;     // BC_BOTS_SAFE_RADIUS: how far bots keep from a human city (tests set 0 so bots engage at once)
		float tempo;          // BC_BOTS_TEMPO: scales the gaps between events (tests only)
		int botCity;
		std::string webhook;

		BotInfo bots[24];
		int overseer;                    // slot used to speak for the server, -1 when absent

		float nextStep, nextManage, nextSave, lastStepTick, nextTurret;
		float lastHumanSeen;
		int lastHumanCount;
		float lastWebhook;
		bool dirty;

		// Events and persistence
		std::vector<std::string> events;
		std::map<std::string, Progress> progress;
		std::map<std::string, int> trialBest;     // milliseconds
		std::map<std::string, int> rangeBest;     // milliseconds
		std::map<std::string, Streak> streaks;    // daily login streaks (registered players only)
		std::map<std::string, int> weekScore;     // this week's solo score per player
		int boardWeek;                            // the leaderboard week weekScore belongs to (weeks start on Monday, UTC)
		int lastWinnerWeek, lastWinnerScore;      // the previous week's winner
		std::string lastWinnerName;
		std::string lastOnlineName;
		long lastOnlineTime;
		std::vector<Sample> ghostPath;
		std::string ghostName;
		int ghostPoints;
		std::map<int, std::vector<Sample> > recording;
		std::map<int, float> recordNext;
		std::set<int> greeted;

		// Raid, mission, bounty, drops
		struct Raid { bool active; int city; float endsAt; int spawned; std::set<std::string> killers; } raid;
		float nextRaid;
		struct Mission {
			bool active; int type; float endsAt; int goal; int progress; float heldMs;
			float zoneX, zoneY; float nextWave; float nextHint; std::set<std::string> participants;
		} mission;
		float nextMission;
		float nextBounty;
		struct Drop { bool active; int id; int x, y; int city; float expiresAt; } drops[4];
		float nextDrop;

		// Per-player activities
		struct Trial { bool active; int step; float startedAt; float nextHint; float cx[4], cy[4]; } trial[MAX_PLAYERS];
		struct Range { bool active; float startedAt; float endsAt; int total; int left; } range[MAX_PLAYERS];

		// Helpers
		float Now() const;
		bool IsHuman(int i) const;
		int CountHumans() const;
		int NearestHuman(float x, float y, float *distOut, bool aliveOnly) const;
		bool Blocked(float x, float y) const;
		bool FreeSpot(float x, float y) const;
		bool InSafeZone(float x, float y, float *awayX = NULL, float *awayY = NULL) const;   // near a human city: bots keep out (raiders on a raid excepted)
		bool FindSpawn(float nearX, float nearY, float minDist, float maxDist, float *ox, float *oy);
		static int DirTo(float dx, float dy);
		static std::string Compass(float dx, float dy);
		static float Rand01();

		// Slots and announcements
		int AllocSlot();
		void ReleaseSlot(int slot);
		void EnsureOverseer();
		void Tell(int idx, const std::string &text);
		void TellAll(const std::string &text);
		void TellCity(int city, const std::string &text);
		std::string Rel(int idx, float x, float y) const;
		void TellEachRel(const std::string &before, float x, float y, const std::string &after);
		void Event(const std::string &text);

		// Bots
		int SpawnBot(int tier, float x, float y, int raidCity = -1);
		void SendBotToAll(BotInfo &b);
		void SendUpdate(BotInfo &b);
		void RemoveBot(BotInfo &b);
		void RemoveAllBots();
		void KillBot(BotInfo &b, int killer);
		void RespawnBot(BotInfo &b);
		void Think(BotInfo &b, float now);
		void StepBot(BotInfo &b, float dt, float now);
		void Fire(BotInfo &b);
		void TurretsFire(float now);
		void Manage(float now);
		int CountTier(int tier) const;
		int CountRaiders() const;
		BotInfo *BotBySlot(int slot);
		float Speed(const BotInfo &b) const;
		int Points(const BotInfo &b) const;

		// Features
		void ManageFighters(float now, int humans);
		void ManageRaid(float now, int humans);
		void ManageMission(float now, int humans);
		void ManageBounty(float now, int humans);
		void ManageDrops(float now, int humans);
		void ManageGhost(float now, int humans);
		void StartMission(int type, int humans);
		void EndMission(bool success);
		void EndRaid(bool success);
		void DropCrate(int humanSlot, int itemType, float x, float y, bool announce);
		void ExpireDrops(float now);
		void StartRange(int idx);
		void StartTrial(int idx);
		void TrialProgress(int idx);
		void CheckRange(int idx, float now);
		void AwardKill(BotInfo &b, int killer);
		void GiveBotPoints(int idx, int pts);
		void GivePoints(const std::string &name, int pts);
		Progress &ProgressFor(int idx);
		void Discord(const std::string &text, bool force = false);
		void AddScore(const std::string &name, int n);          // weekly leaderboard score (registered players who are online)
		void LoginStreak(int idx);                              // first sighting of a player this session: streak and its reward
		void RollBoard();                                       // a new week has begun: announce the winner and start over
		void TellBoard(int idx);
		void SendBoards(float now);                              // smLeaderboard to every human, every few seconds
		float nextBoard;
		std::string BoardText(int count, const char *eol);
		static int BoardWeekOf(long t);
		std::string DailyText(int idx);

		// Files
		void Load();
		void Save();
		void SaveGhost();
		void LoadGhost();
};

#endif
