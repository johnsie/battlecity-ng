#include "../PublicStatus.h"

#include <cassert>

int main() {
	assert(IsPublicActivePlayer(State_Game, 0, false));
	assert(IsPublicActivePlayer(State_Game, MAX_CITIES - 1, false));
	assert(!IsPublicActivePlayer(State_Connected, 0, false));
	assert(!IsPublicActivePlayer(State_Verified, 0, false));
	assert(!IsPublicActivePlayer(State_Chat, 0, false));
	assert(!IsPublicActivePlayer(State_Apply, 0, false));
	assert(!IsPublicActivePlayer(State_Game, -1, false));
	assert(!IsPublicActivePlayer(State_Game, MAX_CITIES, false));
	assert(!IsPublicActivePlayer(State_Game, 0, true));
	const PublicPlayerState players[] = {
		{State_Game, 0, false},
		{State_Game, 1, false},
		{State_Game, 2, true},
		{State_Chat, 3, false},
		{State_Game, -1, false},
	};
	assert(CountPublicActivePlayers(players, sizeof(players) / sizeof(players[0])) == 2);
	return 0;
}
