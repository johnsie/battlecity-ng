/*
===============================================================================
    Battle City - CVoice header file
    Live voice chat carried by the game connection. See docs/voice-chat.md.

    This file is part of Battle City.

    Battle City is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
===============================================================================
*/
#ifndef _CVOICE
#define _CVOICE

#include "CServer.h"

class CServer;

enum VoiceScope {
	VOICE_EVERYONE = 0,     // everyone who has voice on and also hears the sender's channel
	VOICE_CITY = 1,         // players of the same city
	VOICE_NEARBY = 2        // players within VOICE_NEARBY_RANGE pixels
};

#define VOICE_NEARBY_RANGE 1800.0f

/// <summary>
/// Passes voice packets between the players who may hear each other. The server never decodes, mixes or stores audio.
/// Gameplay always comes first: voice from a player who floods is dropped, and a listener whose connection is backed up
/// simply misses voice packets (game packets are never delayed by them).
/// </summary>
class CVoice {
	public:
		CVoice(CServer *server);
		bool enabled;                                   // BC_VOICE=0 switches voice off

		void SendState(int slot);                       // tells a joining player whether voice is available and their settings
		void HandleSet(const char *data, int length, int slot);
		void HandleAudio(const char *data, int length, int slot);
		void HandleReport(const char *data, int length, int slot);
		void Reset(int slot);                           // a player left
		bool Hears(int speaker, int listener);          // would the listener hear the speaker right now

	private:
		struct State {
			bool on;
			unsigned char scope;
			double tokens;                              // flood control (a token bucket)
			double lastFill;
			double reportTimes[5];
			int reports;
		};
		CServer *p;
		State st[MAX_PLAYERS];

		bool Includes(int a, int scope, int b);
		static double Now();
};

#endif
