/*
===============================================================================
    Battle City - CVoice source file. See CVoice.h and docs/voice-chat.md.
===============================================================================
*/
#include "CVoice.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

using namespace std;

namespace {
const int MIN_AUDIO = 6;                // seq 2 + predictor 2 + step index 1 + at least one byte
const int MAX_AUDIO = 200;              // a real packet is 165 bytes (20 ms of 16 kHz ADPCM)
const double MAX_PER_SECOND = 70.0;     // 50 are sent while talking
const double BURST = 25.0;
const size_t MAX_LISTENER_BACKLOG = 16 * 1024;   // a listener this far behind misses voice instead of delaying the game
const int REPORTS_PER_HOUR = 5;
}

double CVoice::Now() {
	return chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count();
}

CVoice::CVoice(CServer *server) {
	p = server;
	const char *env = getenv("BC_VOICE");
	enabled = !(env && strcmp(env, "0") == 0);
	memset(st, 0, sizeof(st));
}

void CVoice::Reset(int slot) {
	if (slot < 0 || slot >= MAX_PLAYERS) return;
	memset(&st[slot], 0, sizeof(State));
}

bool CVoice::Includes(int a, int scope, int b) {
	CPlayer *pa = p->Player[a], *pb = p->Player[b];
	switch (scope) {
		case VOICE_EVERYONE: return true;
		case VOICE_CITY: return pa->City >= 0 && pa->City == pb->City;
		case VOICE_NEARBY: {
			float dx = pa->X - pb->X, dy = pa->Y - pb->Y;
			return sqrtf(dx * dx + dy * dy) <= VOICE_NEARBY_RANGE;
		}
	}
	return false;
}

/// Two players hear each other only if each one's channel includes the other (so someone on "my city" is never heard
/// by another city, and a player on "everyone" is heard only by those whose own channel includes them too).
bool CVoice::Hears(int speaker, int listener) {
	if (speaker == listener) return false;
	CPlayer *s = p->Player[speaker], *l = p->Player[listener];
	if (!s->isInGame() || !l->isInGame() || l->IsBot || l->Socket < 0 || l->lingering) return false;
	const State &a = st[speaker], &b = st[listener];
	return a.on && b.on && Includes(speaker, a.scope, listener) && Includes(listener, b.scope, speaker);
}

void CVoice::SendState(int slot) {
	if (slot < 0 || slot >= MAX_PLAYERS || p->Player[slot]->IsBot) return;
	char packet[3];
	packet[0] = enabled ? 1 : 0;
	packet[1] = st[slot].on ? 1 : 0;
	packet[2] = (char)st[slot].scope;
	p->Winsock->SendData(slot, smVoiceState, packet, 3);
}

void CVoice::HandleSet(const char *data, int length, int slot) {
	if (!enabled || length < 2 || slot < 0 || slot >= MAX_PLAYERS) return;
	CPlayer *player = p->Player[slot];
	if (!player->isInGame() || player->IsBot) return;
	unsigned char on = (unsigned char)data[0], scope = (unsigned char)data[1];
	if (scope > VOICE_NEARBY) scope = VOICE_EVERYONE;
	st[slot].on = on != 0;
	st[slot].scope = scope;
	SendState(slot);
}

void CVoice::HandleAudio(const char *data, int length, int slot) {
	if (!enabled || slot < 0 || slot >= MAX_PLAYERS) return;
	if (length < MIN_AUDIO || length > MAX_AUDIO) return;
	State &s = st[slot];
	if (!s.on) return;
	CPlayer *speaker = p->Player[slot];
	if (!speaker->isInGame() || speaker->IsBot) return;

	// Flood control: a client that sends more than the allowed rate is ignored until it calms down
	double now = Now();
	if (s.lastFill == 0) { s.lastFill = now; s.tokens = BURST; }
	s.tokens += (now - s.lastFill) * MAX_PER_SECOND;
	if (s.tokens > BURST) s.tokens = BURST;
	s.lastFill = now;
	if (s.tokens < 1.0) return;
	s.tokens -= 1.0;

	char out[256];
	out[0] = (char)slot;
	memcpy(out + 1, data, (size_t)length);
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (!Hears(slot, i)) continue;
		if (p->Player[i]->Out.Pending() > MAX_LISTENER_BACKLOG) continue;   // never let voice add to a backed-up connection
		p->Winsock->SendData(i, smVoice, out, length + 1);
	}
}

void CVoice::HandleReport(const char *data, int length, int slot) {
	if (length < 1 || slot < 0 || slot >= MAX_PLAYERS) return;
	int target = (unsigned char)data[0];
	if (target >= MAX_PLAYERS || target == slot || !p->Player[target]->isConnected() || p->Player[target]->IsBot) return;
	State &s = st[slot];
	double now = Now();
	int kept = 0;
	for (int i = 0; i < s.reports; i++) if (now - s.reportTimes[i] < 3600.0) s.reportTimes[kept++] = s.reportTimes[i];
	s.reports = kept;
	if (s.reports >= REPORTS_PER_HOUR) return;
	s.reportTimes[s.reports++] = now;
	ofstream f("voice-reports.log", ios::app);
	if (f) {
		time_t t = time(NULL);
		char when[32];
		strftime(when, sizeof(when), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
		f << when << " reporter=" << p->Player[slot]->Name << " reported=" << p->Player[target]->Name << "\n";
	}
	cout << "VoiceReport::" << p->Player[slot]->Name << " reported " << p->Player[target]->Name << endl;
}
