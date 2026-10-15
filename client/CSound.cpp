/*
===============================================================================

    Battle City - CSound
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
#include "CSound.h"

CSound::CSound(CGame *game)
{
	p = game;
	MIDIndex = 0;
	s_tanklaser = s_fire = s_engine = s_build = s_die = s_eXplode = s_turret = NULL;
	s_buzz = s_click = s_bigturret = s_demolish = s_screech = s_hit = s_cloak = s_flare = NULL;
}

CSound::~CSound()
{
	Shutdown();
}

void CSound::Init() {}
void CSound::PlayWav(int Index, int channel) {}
void CSound::PlaYMID(int Index) {}
void CSound::StopWav(int Index) {}
void CSound::StopMID(int all, int Index) {}
void CSound::Play3dSound(BCSample *sound, int volume, float x, float y) {}
void CSound::Update() {}
void CSound::Shutdown() {}
int CSound::musicCycle() { return 0; }
