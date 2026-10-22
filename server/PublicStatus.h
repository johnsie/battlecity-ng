#ifndef _PUBLICSTATUS
#define _PUBLICSTATUS

#include "CConstants.h"
#include <stddef.h>

struct PublicPlayerState {
	int state;
	int city;
	bool automated;
};

inline bool IsPublicActivePlayer(int state, int city, bool automated) {
	return state == State_Game && city >= 0 && city < MAX_CITIES && !automated;
}

inline int CountPublicActivePlayers(const PublicPlayerState *players, size_t count) {
	int active = 0;
	for (size_t i = 0; i < count; ++i) {
		if (IsPublicActivePlayer(players[i].state, players[i].city, players[i].automated)) ++active;
	}
	return active;
}

#endif
