/*
===============================================================================
    Battle City - CWaypoints header file
    Tells each player where the things that matter are (their Command Center, a supply crate, a raid on their city, the
    bounty boss, a mission zone, the latest fight), so the client can point to them when they are off screen.
    See docs/waypoints.md.

    This file is part of Battle City.

    Battle City is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
===============================================================================
*/
#ifndef _CWAYPOINTS
#define _CWAYPOINTS

#include "CServer.h"

class CServer;

enum WaypointType {
	WP_HOME = 0,       // your Command Center
	WP_CRATE = 1,      // a supply crate your city can pick up
	WP_RAID = 2,       // the nearest raider attacking your city
	WP_BOSS = 3,       // the bounty boss
	WP_ZONE = 4,       // the mission zone to hold
	WP_GATE = 5,       // your next time-trial gate
	WP_DROP = 6,       // the latest supply crate of another city (cannot be picked up by yours)
	WP_FIGHT = 7       // where the latest fight happened
};

class CWaypoints {
	public:
		CWaypoints(CServer *server);
		void Cycle();                                 // once a second: sends every player their list
		void NoteFight(float x, float y);             // something died or was shot at here
		void Reset();

	private:
		CServer *p;
		float nextSend;
		bool haveFight;
		float fightX, fightY, fightAt;
};

#endif
