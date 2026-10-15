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

/// <summary>
/// Audio. Sound effects are played by name through PlayWav / Play3dSound.
/// NOTE: the backend is currently silent (no-op) while the SDL mixer is being written.
/// </summary>
class CSound
{
public:
	CSound(CGame *game);
	~CSound();

	BCSample *s_tanklaser, *s_fire, *s_engine, *s_build, *s_die, *s_eXplode, *s_turret, *s_buzz, *s_click, *s_bigturret, *s_demolish, *s_screech, *s_hit, *s_cloak, *s_flare;

	void Init();
	void PlayWav(int Index, int channel); //0-14 valid (15)
	void PlaYMID(int Index);
	void StopWav(int Index);
	void StopMID(int all, int Index);

	void Play3dSound(BCSample *sound, int volume, float x, float y);

	void Update();      // once per frame
	void Shutdown();

	int MIDIndex;
	int musicCycle();
private:
	CGame *p;
protected:

};

#endif
