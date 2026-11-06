/*
 * Battle City - voice chat (client). See docs/voice-chat.md.
 *
 * The microphone is cut into 20 ms frames of 16 kHz mono sound, squeezed with ADPCM (client/adpcm.h) and sent to the
 * server over the game connection (cmVoice) while the player is talking; the server forwards it to the players who may
 * hear it (smVoice). Capture and playback run on SDL's audio threads and never wait for the game; the game thread only
 * sends and receives packets. Nothing here is ever recorded.
 */
#ifndef _CVOICE_CLIENT
#define _CVOICE_CLIENT

#include <SDL.h>
#include <atomic>
#include <deque>
#include <set>
#include <string>
#include <vector>
#include "CConstants.h"

class CGame;

class CVoice {
	public:
		CVoice(CGame *game);
		~CVoice();

		// settings (kept on this device)
		bool on;                    // voice chat on
		int mode;                   // 0 push to talk, 1 open mic
		int scope;                  // 0 everyone, 1 my city, 2 nearby
		int sensitivity;            // open mic: 0 low, 1 medium, 2 high
		int volume;                 // index into the volume table
		bool muteAll;
		std::set<std::string> blocked;     // names, lower case; remembered
		std::set<std::string> muted;       // names, lower case; this session only

		bool serverEnabled;         // the server offers voice
		std::string status;         // what to tell the player ("Microphone blocked", ...)

		void Init();                // loads the settings
		void Update();              // once a frame: keys, microphone, sending
		void OnState(const char *data, int length);     // smVoiceState
		void OnAudio(const char *data, int length);     // smVoice
		void Toggle();              // the V key
		void SetOn(bool value);
		void Save();
		void SendSettings();
		void OpenSettings();        // the settings dialog
		void Report(int slot);
		void Reset();               // back to the login screen

		bool Transmitting() const { return transmitting.load(); }
		float Level() const { return level.load(); }
		bool Speaking(int slot) const;
		bool Hidden(const std::string &name) const;        // muted or blocked
		static std::string Lower(const std::string &name);

		void Mix(float *mix, int frames, int rate);         // audio thread: adds everyone's voice to the game's sound
		void CaptureCallback(const int16_t *samples, int count);   // audio thread
		static const char *ScopeName(int scope);
		bool SelfTest();            // test hook: a tone goes through capture, the codec and playback

	private:
		CGame *p;
		SDL_AudioDeviceID capture;
		Uint32 openedAt, lastFrameAt;
		bool capturedAny;
		bool wasInGame;
		int sentScope;
		bool sentOn;
		std::atomic<bool> ptt, transmitting, wantOpen;
		std::atomic<float> level;
		std::atomic<int> sensitivityA;

		// capture side (audio thread)
		int16_t frame[320];
		int filled;
		int encPred, encIdx;
		unsigned short seq;
		int hangover;
		SDL_mutex *outLock;
		std::deque<std::vector<unsigned char> > outbound;

		// playback side
		struct Speaker {
			std::vector<float> ring;
			size_t head, count;
			bool playing;
			double frac;
			float last;
			Uint32 lastHeard;
		};
		SDL_mutex *mixLock;
		Speaker spk[MAX_PLAYERS];

		void OpenCapture();
		void CloseCapture();
};

extern CVoice *g_voice;      // read by the audio callback in CSound.cpp

#endif
