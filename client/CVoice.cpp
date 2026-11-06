/*
 * Battle City - voice chat (client). See CVoice.h and docs/voice-chat.md.
 */
#include "CGame.h"
#include "CVoice.h"
#include "adpcm.h"
#include "platform/dialogs.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>

std::string BCLoadText(const char *key);
void BCSaveText(const char *key, const std::string &text);

CVoice *g_voice = NULL;
static void *CVoicePointer = NULL;     // the game, for the dialog procedure

namespace {
const int RATE = 16000;                 // the voice sample rate
const int FRAME = 320;                  // 20 ms
const int RING = 9600;                  // 0.6 s kept per speaker
const int PREBUFFER = 1600;             // 100 ms before playing starts: smooths the network's jitter
const float VOLUME[5] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f };
const float GATE[3] = { 0.030f, 0.012f, 0.005f };     // open mic: low sensitivity needs louder speech
const char *SETTINGS = "voice.ini";
}

const char *CVoice::ScopeName(int scope) {
	switch (scope) {
		case 1: return "My city";
		case 2: return "Nearby";
		default: return "Everyone";
	}
}

std::string CVoice::Lower(const std::string &name) {
	std::string s = name;
	for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
	return s;
}

static void CaptureThunk(void *userdata, Uint8 *stream, int len) {
	((CVoice *)userdata)->CaptureCallback((const int16_t *)stream, len / 2);
}

CVoice::CVoice(CGame *game) {
	p = game;
	on = false; mode = 1; scope = 0; sensitivity = 1; volume = 3; muteAll = false;
	serverEnabled = false;
	capture = 0; openedAt = 0; lastFrameAt = 0; capturedAny = false; wasInGame = false;
	sentScope = -1; sentOn = false;
	ptt = false; transmitting = false; wantOpen = false; level = 0; sensitivityA = 1;
	filled = 0; encPred = 0; encIdx = 0; seq = 0; hangover = 0;
	outLock = SDL_CreateMutex();
	mixLock = SDL_CreateMutex();
	for (int i = 0; i < MAX_PLAYERS; i++) {
		spk[i].ring.assign(RING, 0.0f);
		spk[i].head = spk[i].count = 0;
		spk[i].playing = false; spk[i].frac = 0; spk[i].last = 0; spk[i].lastHeard = 0;
	}
	g_voice = this;
	CVoicePointer = game;
}

CVoice::~CVoice() {
	g_voice = NULL;
	CloseCapture();
	SDL_DestroyMutex(outLock);
	SDL_DestroyMutex(mixLock);
}

void CVoice::Init() {
	std::istringstream in(BCLoadText(SETTINGS));
	std::string line;
	while (std::getline(in, line)) {
		size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string k = line.substr(0, eq), v = line.substr(eq + 1);
		int n = atoi(v.c_str());
		if (k == "on") on = n != 0;
		else if (k == "mode") mode = std::max(0, std::min(1, n));
		else if (k == "scope") scope = std::max(0, std::min(2, n));
		else if (k == "sens") sensitivity = std::max(0, std::min(2, n));
		else if (k == "vol") volume = std::max(0, std::min(4, n));
		else if (k == "muteall") muteAll = n != 0;
		else if (k == "block" && !v.empty() && blocked.size() < 100) blocked.insert(Lower(v));
	}
	sensitivityA = sensitivity;
}

void CVoice::Save() {
	std::ostringstream out;
	out << "on=" << (on ? 1 : 0) << "\nmode=" << mode << "\nscope=" << scope << "\nsens=" << sensitivity << "\nvol=" << volume
	    << "\nmuteall=" << (muteAll ? 1 : 0) << "\n";
	for (std::set<std::string>::const_iterator it = blocked.begin(); it != blocked.end(); ++it) out << "block=" << *it << "\n";
	BCSaveText(SETTINGS, out.str());
	sensitivityA = sensitivity;
}

void CVoice::Reset() {
	CloseCapture();
	serverEnabled = false;
	sentScope = -1; sentOn = false;
	wasInGame = false;
	status.clear();
	SDL_LockMutex(mixLock);
	for (int i = 0; i < MAX_PLAYERS; i++) { spk[i].head = spk[i].count = 0; spk[i].playing = false; }
	SDL_UnlockMutex(mixLock);
}

void CVoice::OnState(const char *data, int length) {
	if (length < 3) return;
	serverEnabled = data[0] != 0;
	bool serverOn = data[1] != 0;
	int serverScope = (unsigned char)data[2];
	sentOn = serverOn; sentScope = serverScope;
	if (serverEnabled && (serverOn != on || serverScope != scope)) SendSettings();
}

void CVoice::SendSettings() {
	if (!serverEnabled) return;
	char packet[2];
	packet[0] = on ? 1 : 0;
	packet[1] = (char)scope;
	p->Winsock->SendData(cmVoiceSet, packet, 2);
	sentOn = on; sentScope = scope;
}

void CVoice::SetOn(bool value) {
	on = value;
	status.clear();
	Save();
	SendSettings();
}

void CVoice::Toggle() {
	if (!serverEnabled) {
		p->InGame->AppendChat("Voice chat is not available on this server.", COLOR_SYSTEM);
		return;
	}
	SetOn(!on);
	if (on) {
		p->InGame->AppendChat(mode == 0 ? "Voice chat on. Hold ` (the key left of 1) to talk. Press V to turn it off." : "Voice chat on: you are heard when you speak. Click the Voice button for settings, V turns it off.", COLOR_SYSTEM);
	} else {
		p->InGame->AppendChat("Voice chat off.", COLOR_SYSTEM);
	}
}

bool CVoice::Speaking(int slot) const {
	if (slot < 0 || slot >= MAX_PLAYERS) return false;
	return spk[slot].lastHeard != 0 && SDL_GetTicks() - spk[slot].lastHeard < 400;
}

bool CVoice::Hidden(const std::string &name) const {
	std::string l = Lower(name);
	return muted.count(l) > 0 || blocked.count(l) > 0;
}

// ---- microphone ---------------------------------------------------------------------------------------------------------

void CVoice::OpenCapture() {
	if (capture) return;
	SDL_AudioSpec want, have;
	SDL_zero(want);
	want.freq = RATE;
	want.format = AUDIO_S16SYS;
	want.channels = 1;
	want.samples = 512;
	want.callback = CaptureThunk;
	want.userdata = this;
	capturedAny = false;
	openedAt = SDL_GetTicks();
	capture = SDL_OpenAudioDevice(NULL, 1, &want, &have, 0);
	if (!capture) {
		status = "No microphone could be opened. You can still listen.";
		openedAt = SDL_GetTicks() + 5000;      // try again in a few seconds
		return;
	}
	filled = 0; hangover = 0;
	SDL_PauseAudioDevice(capture, 0);
	status = "Microphone starting...";
}

void CVoice::CloseCapture() {
	if (capture) {
		SDL_CloseAudioDevice(capture);
		capture = 0;
	}
	transmitting = false;
	level = 0;
	SDL_LockMutex(outLock);
	outbound.clear();
	SDL_UnlockMutex(outLock);
}

/// Audio thread: microphone sound in; a squeezed 20 ms packet out whenever the player is talking
void CVoice::CaptureCallback(const int16_t *samples, int count) {
	capturedAny = true;
	for (int i = 0; i < count; i++) {
		frame[filled++] = samples[i];
		if (filled < FRAME) continue;
		filled = 0;
		double energy = 0;
		for (int k = 0; k < FRAME; k++) { double v = frame[k] / 32768.0; energy += v * v; }
		float rms = (float)sqrt(energy / FRAME);
		level = level.load() * 0.7f + rms * 0.3f;
		bool talk;
		if (mode == 1) {
			if (rms > GATE[std::max(0, std::min(2, sensitivityA.load()))]) hangover = 25;      // 0.5 s after the last loud frame
			else if (hangover > 0) hangover--;
			talk = hangover > 0;
		} else {
			talk = ptt.load();
			hangover = 0;
		}
		transmitting = talk;
		if (!talk || !on) continue;
		std::vector<unsigned char> pk(5 + FRAME / 2);
		seq++;
		pk[0] = (unsigned char)(seq & 255); pk[1] = (unsigned char)(seq >> 8);
		pk[2] = (unsigned char)(encPred & 255); pk[3] = (unsigned char)((encPred >> 8) & 255);
		pk[4] = (unsigned char)encIdx;
		adpcm::Encode(frame, FRAME, encPred, encIdx, &pk[5]);
		SDL_LockMutex(outLock);
		if (outbound.size() >= 12) outbound.pop_front();       // the game thread is behind: drop the oldest rather than build a backlog
		outbound.push_back(pk);
		SDL_UnlockMutex(outLock);
	}
}

/// Game thread, once a frame
void CVoice::Update() {
	bool inGame = p->State == STATE_GAME;
	if (!inGame) {
		if (wasInGame) Reset();
		return;
	}
	wasInGame = true;
	// push to talk: the key left of 1 (` / ~)
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	bool keyDown = keys && keys[SDL_SCANCODE_GRAVE];
	ptt = keyDown && !p->InGame->IsChatting && !BCDialogsActive();

	bool want = serverEnabled && on;
	Uint32 now = SDL_GetTicks();
	if (want && !capture && (int)(now - openedAt) >= 0) OpenCapture();
	if (!want && capture) { CloseCapture(); status.clear(); }
	if (capture && !capturedAny && now - openedAt > 8000 && status != "No microphone sound. Allow the microphone for this site, then turn voice off and on.") {
		status = "No microphone sound. Allow the microphone for this site, then turn voice off and on.";
	}
	if (capture && capturedAny && status.compare(0, 10, "Microphone") == 0) status.clear();

	// send what the audio thread has prepared (a few per frame at most, so a slow frame never bursts)
	for (int sent = 0; sent < 6; sent++) {
		std::vector<unsigned char> pk;
		SDL_LockMutex(outLock);
		if (!outbound.empty()) { pk = outbound.front(); outbound.pop_front(); }
		SDL_UnlockMutex(outLock);
		if (pk.empty()) break;
		p->Winsock->SendData(cmVoice, (char *)&pk[0], (int)pk.size());
	}
}

// ---- other players' voices ----------------------------------------------------------------------------------------------

void CVoice::OnAudio(const char *data, int length) {
	if (!serverEnabled || muteAll || length < 7) return;
	int slot = (unsigned char)data[0];
	if (slot >= MAX_PLAYERS || slot == p->Winsock->MyIndex) return;
	if (Hidden(p->Player[slot]->Name)) return;
	const unsigned char *d = (const unsigned char *)data;
	int pred = (int16_t)(d[3] | (d[4] << 8));
	int idx = d[5];
	int bytes = length - 6;
	if (bytes > 200) bytes = 200;
	int n = bytes * 2;
	float samples[400];
	adpcm::Decode(d + 6, n, pred, idx, samples);

	SDL_LockMutex(mixLock);
	Speaker &s = spk[slot];
	for (int i = 0; i < n; i++) {
		if (s.count >= (size_t)RING) { s.head = (s.head + 1) % RING; s.count--; }    // too far behind: drop the oldest sound
		s.ring[(s.head + s.count) % RING] = samples[i];
		s.count++;
	}
	s.lastHeard = SDL_GetTicks();
	SDL_UnlockMutex(mixLock);
}

/// Audio thread: adds everyone who is talking to the game's own sound (resampled from 16 kHz mono)
void CVoice::Mix(float *mix, int frames, int rate) {
	if (muteAll || rate <= 0) return;
	float gain = VOLUME[std::max(0, std::min(4, volume))] * 0.8f;
	double step = (double)RATE / rate;
	SDL_LockMutex(mixLock);
	for (int sp = 0; sp < MAX_PLAYERS; sp++) {
		Speaker &s = spk[sp];
		if (!s.playing) {
			if (s.count >= (size_t)PREBUFFER) { s.playing = true; s.frac = 0; }
			else continue;
		}
		for (int i = 0; i < frames; i++) {
			if (s.count < 2) { s.playing = false; break; }       // ran dry: wait for the next burst
			float a = s.ring[s.head], b = s.ring[(s.head + 1) % RING];
			float v = (a + (b - a) * (float)s.frac) * gain;
			mix[(size_t)i * 2] += v;
			mix[(size_t)i * 2 + 1] += v;
			s.frac += step;
			while (s.frac >= 1.0) { s.frac -= 1.0; s.head = (s.head + 1) % RING; s.count--; if (s.count == 0) break; }
		}
	}
	SDL_UnlockMutex(mixLock);
}

void CVoice::Report(int slot) {
	char packet[1];
	packet[0] = (char)slot;
	p->Winsock->SendData(cmVoiceReport, packet, 1);
}

// ---- the settings dialog (Options > Voice chat, or a click on the voice line in the game) -------------------------------------

static std::vector<int> g_dlgSlots;      // list row -> player slot

static void FillPlayers(HWND hwnd, CGame *p, int keep) {
	SendDlgItemMessage(hwnd, IDV_PLAYERS, LB_RESETCONTENT, 0, 0);
	g_dlgSlots.clear();
	for (int i = 0; i < MAX_PLAYERS; i++) {
		CPlayer *pl = p->Player[i];
		if (i == p->Winsock->MyIndex || !pl->isInGame || pl->Name.empty()) continue;
		std::string label = pl->Name;
		if (p->Voice->blocked.count(CVoice::Lower(pl->Name))) label += "  [blocked]";
		else if (p->Voice->muted.count(CVoice::Lower(pl->Name))) label += "  [muted]";
		SendDlgItemMessage(hwnd, IDV_PLAYERS, LB_ADDSTRING, 0, (LPARAM)label.c_str());
		g_dlgSlots.push_back(i);
	}
	if (keep >= 0 && keep < (int)g_dlgSlots.size()) SendDlgItemMessage(hwnd, IDV_PLAYERS, LB_SETCURSEL, keep, 0);
}

static int CALLBACK VoiceDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	CGame *p = (CGame *)CVoicePointer;
	if (!p) return 0;
	CVoice *v = p->Voice;
	switch (Message) {
		case WM_INITDIALOG: {
			CheckDlgButton(hwnd, IDV_ON, v->on);
			CheckDlgButton(hwnd, IDV_MUTEALL, v->muteAll);
			const char *modes[] = { "Push to talk (hold the ` key)", "Open mic (heard when you speak)" };
			for (int i = 0; i < 2; i++) SendDlgItemMessage(hwnd, IDV_MODE, CB_ADDSTRING, 0, (LPARAM)modes[i]);
			SendDlgItemMessage(hwnd, IDV_MODE, CB_SETCURSEL, v->mode, 0);
			const char *scopes[] = { "Everyone", "My city only", "Players nearby only" };
			for (int i = 0; i < 3; i++) SendDlgItemMessage(hwnd, IDV_SCOPE, CB_ADDSTRING, 0, (LPARAM)scopes[i]);
			SendDlgItemMessage(hwnd, IDV_SCOPE, CB_SETCURSEL, v->scope, 0);
			const char *sens[] = { "Low (needs a louder voice)", "Medium", "High (picks up quiet speech)" };
			for (int i = 0; i < 3; i++) SendDlgItemMessage(hwnd, IDV_SENS, CB_ADDSTRING, 0, (LPARAM)sens[i]);
			SendDlgItemMessage(hwnd, IDV_SENS, CB_SETCURSEL, v->sensitivity, 0);
			const char *vols[] = { "25%", "50%", "75%", "100%", "150%" };
			for (int i = 0; i < 5; i++) SendDlgItemMessage(hwnd, IDV_VOLUME, CB_ADDSTRING, 0, (LPARAM)vols[i]);
			SendDlgItemMessage(hwnd, IDV_VOLUME, CB_SETCURSEL, v->volume, 0);
			FillPlayers(hwnd, p, -1);
			std::string status;
			if (!v->serverEnabled) status = "This server does not offer voice chat (or you are not in a game yet).";
			else if (!v->status.empty()) status = v->status;
			else if (v->on) status = "Voice chat is on.";
			else status = "Voice chat is off. Nobody hears you until you turn it on.";
			SetDlgItemText(hwnd, IDV_STATUS, status.c_str());
			return 1;
		}
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case 1: {
					bool wasOn = v->on;
					v->on = IsDlgButtonChecked(hwnd, IDV_ON) != 0;
					v->muteAll = IsDlgButtonChecked(hwnd, IDV_MUTEALL) != 0;
					int m = (int)SendDlgItemMessage(hwnd, IDV_MODE, CB_GETCURSEL, 0, 0); if (m >= 0) v->mode = m;
					int s = (int)SendDlgItemMessage(hwnd, IDV_SCOPE, CB_GETCURSEL, 0, 0); if (s >= 0) v->scope = s;
					int n = (int)SendDlgItemMessage(hwnd, IDV_SENS, CB_GETCURSEL, 0, 0); if (n >= 0) v->sensitivity = n;
					int o = (int)SendDlgItemMessage(hwnd, IDV_VOLUME, CB_GETCURSEL, 0, 0); if (o >= 0) v->volume = o;
					if (v->on != wasOn) v->status.clear();
					v->Save();
					v->SendSettings();
					p->Dialog->StartDialog = 0;
					EndDialog(hwnd, 1);
					break;
				}
				case 2:
					p->Dialog->StartDialog = 0;
					EndDialog(hwnd, 2);
					break;
				case IDV_MUTE:
				case IDV_BLOCK:
				case IDV_REPORT: {
					int row = (int)SendDlgItemMessage(hwnd, IDV_PLAYERS, LB_GETCURSEL, 0, 0);
					if (row < 0 || row >= (int)g_dlgSlots.size()) break;
					int slot = g_dlgSlots[row];
					std::string name = CVoice::Lower(p->Player[slot]->Name);
					if (LOWORD(wParam) == IDV_MUTE) {
						if (!v->muted.erase(name)) v->muted.insert(name);
					} else if (LOWORD(wParam) == IDV_BLOCK) {
						if (!v->blocked.erase(name)) v->blocked.insert(name);
						v->Save();
					} else {
						v->Report(slot);
						SetDlgItemText(hwnd, IDV_STATUS, "Reported. The server's owner can read it. You can also mute or block them.");
					}
					FillPlayers(hwnd, p, row);
					break;
				}
			}
			return 1;
		default:
			return 0;
	}
	return 1;
}

void CVoice::OpenSettings() {
	p->Dialog->StartDialog = ID_VOICE;
	p->Dialog->DialogProcessor = &VoiceDlgProc;
}


/// Test hook (BC_TEST_VOICE=1 in the offline test scene): one second of a 440 Hz tone goes through the capture path, the
/// codec and the playback mixer, and what comes out must still be that tone.
bool CVoice::SelfTest() {
	bool oldOn = on; int oldMode = mode; bool oldEnabled = serverEnabled;
	on = true; mode = 1; sensitivity = 1; sensitivityA = 1; serverEnabled = true;
	std::vector<int16_t> tone(RATE);
	for (int i = 0; i < RATE; i++) tone[i] = (int16_t)(12000.0 * sin(2.0 * 3.14159265 * 440.0 * i / RATE));
	std::vector<std::vector<unsigned char> > sent;
	for (int at = 0; at + FRAME <= RATE; at += FRAME) {       // (drained as it goes, as the game thread does)
		CaptureCallback(&tone[at], FRAME);
		SDL_LockMutex(outLock);
		while (!outbound.empty()) { sent.push_back(outbound.front()); outbound.pop_front(); }
		SDL_UnlockMutex(outLock);
	}
	int slot = (p->Winsock->MyIndex + 1) % MAX_PLAYERS;
	for (size_t i = 0; i < sent.size(); i++) {
		std::vector<char> pk(1 + sent[i].size());
		pk[0] = (char)slot;
		memcpy(&pk[1], &sent[i][0], sent[i].size());
		OnAudio(&pk[0], (int)pk.size());
	}
	std::vector<float> out;
	for (int blocks = 0; blocks < 200; blocks++) {
		std::vector<float> mixbuf(2 * 441, 0.0f);
		Mix(&mixbuf[0], 441, 44100);
		for (int i = 0; i < 441; i++) out.push_back(mixbuf[2 * i]);
	}
	double energy = 0; int crossings = 0; size_t n = 0;
	for (size_t i = 1; i < out.size(); i++) {
		if (out[i] == 0 && out[i - 1] == 0) continue;
		n++; energy += out[i] * out[i];
		if ((out[i - 1] < 0) != (out[i] < 0)) crossings++;
	}
	double rms = n ? sqrt(energy / n) : 0;
	double seconds = (double)n / 44100.0;
	double freq = seconds > 0 ? crossings / 2.0 / seconds : 0;
	printf("voice self test: %d packets sent, %d bytes each, played %.2f s, rms %.3f, %.0f Hz\n", (int)sent.size(), sent.empty() ? 0 : (int)sent[0].size(), seconds, rms, freq);
	on = oldOn; mode = oldMode; serverEnabled = oldEnabled;
	return sent.size() >= 40 && seconds > 0.5 && rms > 0.05 && freq > 400 && freq < 480;
}
