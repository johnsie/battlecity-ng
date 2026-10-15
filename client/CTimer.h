/*
===============================================================================

    Battle City - CTimer header file
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
#ifndef _CTimer
#define _CTimer

#include "CGame.h"

class CTime
{
private:
	unsigned long start;							// SDL tick count when the timer was initialised

public:
	CTime();
	void Update();
	float GetTime() const;							// Milliseconds since Initialize()
	void Initialize();
};

#endif