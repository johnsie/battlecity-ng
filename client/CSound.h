/*
===============================================================================

    Battle City - CSound header file
    Copyright (C) 2005-2013  battlecity.org

    This file is part of Battle City.

    Battle City is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Battle City is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Battle City.  If not, see <http://www.gnu.org/licenses/>.
===============================================================================
*/
#ifndef _CSound
#define _CSound

#include "CGame.h"

class CGame;

/// <summary> A decoded sound effect (opaque to callers). </summary>
struct BCSample;
struct BCAudio;

/// <summary>
/// Audio: sound effects mixed by a small SDL-driven mixer, MIDI music rendered with TinySoundFont.
/// Everything runs from one SDL audio callback, so it behaves the same on Linux, Windows and in the browser.
/// </summary>
class CSound
{
public:
	CSound(CGame *game);
	~CSound();

	BCSample *s_tanklaser, *s_fire, *s_engine, *s_build, *s_die, *s_eXplode, *s_turret, *s_buzz, *s_click, *s_bigturret, *s_demolish, *s_screech, *s_hit, *s_cloak, *s_flare;

	void Init();
	/// <summary> Plays a sound effect by index (0-14). channel -1 = any free channel; otherwise a fixed channel
	/// (a new sound on a fixed channel cuts the previous one, as with FMOD). </summary>
	void PlayWav(int Index, int channel);
	void PlaYMID(int Index);               // 0-6
	void StopWav(int Index);               // stops a fixed channel
	void StopMID(int all, int Index);

	/// <summary> Positional sound: volume 0-255; x/y are world coordinates, attenuated and panned
	/// relative to the local player. </summary>
	void Play3dSound(BCSample *sound, int volume, float x, float y);

	void Update();      // once per frame: starts the next song when one ends
	void Shutdown();

	int MIDIndex;
	int musicCycle();
private:
	CGame *p;
	BCAudio *audio;
	BCSample *LoadSample(const char *file, bool loop);
public:
	void LoadMusicAssets();
private:
	void Play(BCSample *sample, int channel, float left, float right);
protected:

};

#endif
