#include "CBuilding.h"

/***************************************************************
 * Constructor:	CBuilding
 *
 * @param X
 * @param Y
 * @param Type
 * @param City
 * @param id
 * @param Game
 **************************************************************/
CBuilding::CBuilding(unsigned short X, unsigned short Y, int Type, unsigned char City, unsigned short id, CGame *Game) {
	this->p = Game;
	this->City = City;
	this->X = X;
	this->Y = Y;
	this->Type = Type;
	this->pop = 0;
	this->Animation = rand()%6;
	this->timeAnimation = 0;
	this->Smoke = 0;
	this->timeSmoke = 0;
	this->id = id;
	this->ItemsLeft = 0;

	this->prev = 0;
	this->next = 0;
}

/***************************************************************
 * Destructor:	CBuilding
 *
 **************************************************************/
CBuilding::~CBuilding() {
}

/***************************************************************
 * Function:	isCC
 *
 * @return Returns true if this building is a CC
 **************************************************************/
bool CBuilding::isCC() {
	return this->isCC(this->Type);
}
/***************************************************************
 * Function:	isCC
 *
 * @param buildingType
 * @return Returns true if the buildingType is a CC
 **************************************************************/
bool CBuilding::isCC(int buildingType) {
	return (buildingType / 100 == 0);
}




/***************************************************************
 * Constructor:	CBuildingList
 *
 * @param game
 **************************************************************/
CBuildingList::CBuildingList(CGame *game) {
	this->p = game;
	this->buildingListHead = 0;
}

/***************************************************************
 * Destructor:	CBuildingList
 *
 **************************************************************/
CBuildingList::~CBuildingList() {
	while (this->buildingListHead)
		this->delBuilding(this->buildingListHead);
}

/***************************************************************
 * Constructor:	findBuilding
 *
 * @param id
 **************************************************************/
CBuilding *CBuildingList::findBuilding(unsigned short id) {
	CBuilding *bld = this->buildingListHead;

	// For each building,
	while (bld) {

		// If the building matches the id, return it
		if (bld->id == id) {
			return bld;
		}

		// Else, get the next building
		bld = bld->next;
	}

	// Return 0
	return 0;
}

/***************************************************************
 * Constructor:	findBuildingbyLocation
 *
 * @param X
 * @param Y
 **************************************************************/
CBuilding *CBuildingList::findBuildingbyLocation(int X, int Y) {
	CBuilding *bld = this->buildingListHead;

	// For each building,
	while (bld) {

		// If the building matches the X and Y, return it
		if (bld->X == X && bld->Y == Y) {
			return bld;
		}

		// Else, get the next building
		bld = bld->next;
	}

	// Return 0
	return 0;
}

/***************************************************************
 * Constructor:	newBuilding
 *
 * @param X
 * @param Y
 * @param City
 * @param Type
 * @param id
 **************************************************************/
CBuilding *CBuildingList::newBuilding(unsigned short X, unsigned short Y, unsigned char City, int Type, unsigned short id) {
	CBuilding *bld;

	// If the id is over 0,
	if (id > 0) {

		// If the id is already in use, return 0
		bld = this->findBuilding(id);
		if (bld) {
			return 0;
		}
	}

	// Create the building
	bld = new CBuilding(X, Y, Type, City, id, this->p);

	// If there are other buildings,
	if (this->buildingListHead) {

		// Tell the current head the new building is before it
		this->buildingListHead->prev = bld;
		bld->next = this->buildingListHead;
	}

	// Add the new building as the new head
	this->buildingListHead = bld;

	// Return a pointer to the new building
	return bld;
}

/***************************************************************
 * Constructor:	Cycle
 *
 **************************************************************/
void CBuildingList::Cycle() {
	int me = p->Winsock->MyIndex;
	float curTick = p->Tick;
	CBuilding *bld = this->buildingListHead;

	// For each building,
	while (bld) {

		// If the Animation Timer is up,
		if (curTick > bld->timeAnimation) {

			// Cycle the Animation
			bld->Animation++;
			if (bld->Animation > 5) {
				bld->Animation = 0;
			}

			// Reset the Animation timer
			bld->timeAnimation = curTick + 500;
		}

		// If the building is smoking,
		if (bld->Smoke > 0) {

			// If the Smoke timer is up,
			if (curTick > bld->timeSmoke) {

				// Cycle the smoke
				bld->Smoke++; 
				if (bld->Smoke > 5) {
					if (bld->ItemsLeft > 0)  {
						bld->Smoke = 1;
					}
					else  {
						bld->Smoke = 0;
					}
				}

				// Reset the Smoke timer
				bld->timeSmoke = curTick + 500;
			}
		}

		// Else if building isn't smoking, but building now has items left,
		else if (bld->ItemsLeft > 0)  {

			// Set Smoking on
			bld->Smoke = 1;
		}

		// Get the next building
		bld = bld->next;
	}
}

/***************************************************************
 * Constructor:	delBuilding
 *
 * @param del
 **************************************************************/
CBuilding *CBuildingList::delBuilding(CBuilding *del) {
	CBuilding *returnBuilding = del->next;

	// If building has a next, tell that next to skip this over node
	if (del->next) {
		del->next->prev = del->prev;
	}

	// If building has a prev, tell that prev to skip this over node
	if (del->prev) {
		del->prev->next = del->next;
	}
	// Else (building has no prev), building is head, point head to next node
	else {
		this->buildingListHead = del->next;
	}
	
	// Delete the building
	delete del;
	
	// Return what was del->next
	return returnBuilding;
}

/***************************************************************
 * Function:	deleteBuildingsByCity
 *
 * @param city
 **************************************************************/
void CBuildingList::deleteBuildingsByCity(int city) {
	CBuilding *bld = this->buildingListHead;

	// For each building in the linked list,
	while (bld) {

		// If the building belongs to this city, and building isn't a CC,
		if ((bld->City == city) && (bld->isCC() == false)) {

			// Delete the building and move the pointer to the next item in the linked list
			bld = this->delBuilding(bld);
		}
		// Else (building belongs to other city),
		else {

			// Move the pointer to the next building in the linked list
			bld = bld->next;
		}
	}
}

/***************************************************************
 * Function:	inRange
 *
 **************************************************************/
int CBuildingList::inRange() {
	CBuilding *bld;

	// If the player is an admin, return 1
	if (p->Player[p->Winsock->MyIndex]->isAdmin()) {
		return 1;
	}

	// Else, normal player,

	// For each building,
	bld = this->buildingListHead;
	while (bld) {

		// If the building belongs to the builder's city,
		if (bld->City == p->Player[p->Winsock->MyIndex]->City) {

			// If the building is the CC,
			if (bld->isCC()) {

				//If the building's location is close enough to the player's location, return 1
				if ((abs((bld->X*48) - p->Player[p->Winsock->MyIndex]->X) < DISTANCE_MAX_FROM_CC) && (abs((bld->Y*48) - p->Player[p->Winsock->MyIndex]->Y) < DISTANCE_MAX_FROM_CC)) {
					return 1;
				}
			}
		}

		// Get the next building
		bld = bld->next;
	}

	// If no building was found in range, return 0
	p->InGame->NewbieTip = "You cannot build this far away from your City Center!";

	return 0;
}
