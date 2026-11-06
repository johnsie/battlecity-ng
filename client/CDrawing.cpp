#include <algorithm>
#include <cmath>
#include "CGame.h"

/***************************************************************
 * Constructor
 *
 * @param game
 **************************************************************/
CDrawing::CDrawing(CGame *game) {
	p = game;
	shakeAmp = 0; lastFrameTick = 0; fadeStart = -10000; wasDead = false;
	for (int i = 0; i < MAX_PLAYERS; i++) lastTankX[i] = lastTankY[i] = lastPuff[i] = lastMarkX[i] = lastMarkY[i] = 0;
	this->ClearDrawing();
}

/***************************************************************
 * Destructor
 *
 **************************************************************/
CDrawing::~CDrawing() {

}

/***************************************************************
 * Function:	DrawGame
 *
 **************************************************************/
void CDrawing::DrawGame() {
	int me = p->Winsock->MyIndex;
	int baseX = this->PlayerOffsetX, baseY = this->PlayerOffsetY;
	int shX = 0, shY = 0;

	// Screen shake: the world jolts, the player's own tank and the HUD stay put
	float dt = p->Tick - lastFrameTick;
	if (dt < 0 || dt > 200) dt = 16;
	lastFrameTick = p->Tick;
	if (shakeAmp > 0.5f && !getenv("BC_NO_FX")) {
		shX = (int)(((rand() % 201) - 100) / 100.0f * shakeAmp);
		shY = (int)(((rand() % 201) - 100) / 100.0f * shakeAmp);
		shakeAmp -= dt * 0.012f;
	}
	else shakeAmp = 0;

	this->PlayerOffsetX = baseX + shX;
	this->PlayerOffsetY = baseY + shY;
	this->DrawMap();
	this->DrawBuildings();
	this->DrawDust();
	this->DrawBullets();
	this->DrawItems();
	this->DrawPlayers();
	this->PlayerOffsetX = baseX;
	this->PlayerOffsetY = baseY;
	this->DrawPanel();
	this->DrawTank();
	this->PlayerOffsetX = baseX + shX;
	this->PlayerOffsetY = baseY + shY;
	this->DrawExplosions();
	this->PlayerOffsetX = baseX;
	this->PlayerOffsetY = baseY;
	this->DrawWeather();

	// Screen-wide mood: vignette, red pulse when badly hurt, grey-out when dead, fade-in on respawn
	bool dead = p->Player[me]->isDead;
	p->DDraw->DrawVignette(MaxMapX, MaxMapY, 110);
	if (!dead && p->Player[me]->HP > 0 && p->Player[me]->HP <= 12) {
		float pulse = 0.5f + 0.5f * sinf(p->Tick / 170.0f);
		p->DDraw->DrawVignette(MaxMapX, MaxMapY, (int)(30 + 80 * pulse), 210, 0, 0);
	}
	if (dead) {
		int a = (int)((p->Tick - p->InGame->timeDeath) / 6.0f);
		p->DDraw->DrawOverlay(MaxMapX, MaxMapY, 20, 10, 10, std::min(a, 120));
	}
	else if (wasDead) {
		fadeStart = p->Tick;
		Ring rg = { p->Player[me]->X, p->Player[me]->Y, p->Tick, 800.0f, 24, 28, 120, 120, 200, 255, 220 };
		rings.push_back(rg);
	}
	wasDead = dead;
	float fade = (p->Tick - fadeStart) / 600.0f;
	if (fade >= 0 && fade < 1.0f) p->DDraw->DrawOverlay(MaxMapX, MaxMapY, 0, 0, 0, (int)(255 * (1.0f - fade)));

	if (shakeAmp > 3.0f) p->DDraw->DrawOverlay(MaxMapX, MaxMapY, 255, 245, 220, (int)(shakeAmp * 9.0f));   // blinding flash of a close blast
	this->DrawRadar();
	this->DrawChat();
	this->DrawInfo();
	this->DrawInventory();
	this->DrawItemHint();
	this->DrawVoiceHud();

	// If the player is Mayor,
	if (p->Player[p->Winsock->MyIndex]->isMayor) {

		// If the build menu is on, draw the build menu
		if (p->InGame->ShowBuildMenu) {
			this->DrawBuildMenu();
		}

		// If the player has a building or demolish on the cursor, draw the building/demolish
		if (p->InGame->IsBuilding != 0) {
			this->DrawBuildPlacement();
		}
	}

	// If the minimap is on, draw the minimap
	if (p->InGame->ShowMap == 1) {
		this->DrawMiniMap();
	}
}

/***************************************************************
 * Function:	ApplyResolution
 *
 * Follows a change of CGame::ResolutionX/Y: the side panel stays on the
 * right edge and the player stays centred in the (wider/narrower) map view.
 **************************************************************/
void CDrawing::ApplyResolution() {
	int oldDefaultX = this->defaultPlayerOffsetX;
	int oldDefaultY = this->defaultPlayerOffsetY;
	this->MaxMapX = p->ResolutionX - 200;
	this->MaxMapY = p->ResolutionY;
	this->defaultPlayerOffsetX = (this->MaxMapX / 2) - 24;
	this->defaultPlayerOffsetY = (this->MaxMapY / 2) - 24;
	this->PlayerOffsetX += this->defaultPlayerOffsetX - oldDefaultX;
	this->PlayerOffsetY += this->defaultPlayerOffsetY - oldDefaultY;
	if (this->PlayerOffsetX < 0) this->PlayerOffsetX = 0;
	if (this->PlayerOffsetX > this->MaxMapX - 48) this->PlayerOffsetX = this->MaxMapX - 48;
	if (this->PlayerOffsetY < 0) this->PlayerOffsetY = 0;
	if (this->PlayerOffsetY > this->MaxMapY - 48) this->PlayerOffsetY = this->MaxMapY - 48;
}

/***************************************************************
 * Function:	ClearDrawing
 *
 **************************************************************/
void CDrawing::ClearDrawing() {

	// Set the map width and height based on resolution
	this->MaxMapX = p->ResolutionX - 200;
	this->MaxMapY = p->ResolutionY;

	// 1024
	if (p->Options->resolution1024) {
		this->defaultPlayerOffsetX = (this->MaxMapX / 2) - 24;
		this->defaultPlayerOffsetY = (this->MaxMapY / 2) - 24;
		this->groundTilesToDraw = 7;
		this->tilesToDraw = 18;
		this->demoCursorOffsetX = 4;
		this->demoCursorOffsetY = -12;
		this->buildingCursorOffsetX = this->demoCursorOffsetX - 48;
		this->buildingCursorOffsetY = this->demoCursorOffsetY - 48 - 12;
		this->minimapOffsetX = 30;
		this->minimapOffsetY = 25;
		this->clickOffsetX = 9;
		this->clickOffsetY = 9;
		this->chatBarWidth = 103;
		//this->chatBarWidth = 51;
	}
	// 800
	else {
		this->defaultPlayerOffsetX = (this->MaxMapX / 2) - 24;
		this->defaultPlayerOffsetY = (this->MaxMapY / 2) - 24;
		this->groundTilesToDraw = 6;
		this->tilesToDraw = 14;
		this->demoCursorOffsetX = -12;
		this->demoCursorOffsetY = 4;
		this->buildingCursorOffsetX = this->demoCursorOffsetX - 48;
		this->buildingCursorOffsetY = this->demoCursorOffsetY - 48 - 16;
		this->minimapOffsetX = 7;
		this->minimapOffsetY = 7;
		this->clickOffsetX = 7;
		this->clickOffsetY = 7;
		this->chatBarWidth = 75;
		//this->chatBarWidth = 37;
	}

	arrowTick = 0;
	lastArrow = 0;

	BuildMenuX = 0;
	BuildMenuY = 0;
}

/***************************************************************
 * Function:	DrawMap
 *
 **************************************************************/
void CDrawing::DrawMap() {
	int me = p->Winsock->MyIndex;
	int offX = (int)p->Player[me]->X % 48;
	int offY = (int)p->Player[me]->Y % 48;
	int goffX = (int) (p->Player[me]->X + this->PlayerOffsetX) % 128;
	int goffY = (int) (p->Player[me]->Y + this->PlayerOffsetY) % 128;
	int myX = (int)p->Player[me]->X / 48;
	int myY = (int)p->Player[me]->Y / 48;
	int i = 0;
	int j = 0;
	int tileX;
	int tileY;

	// Draw the ground tiles (groundTilesToDraw+1 total)
	// (the 128px ground texture is anchored at the view's top-left, so it must cover the whole map view,
	// which is wider than the original 824 pixels on widescreen windows)
	int groundCols = MaxMapX / 128 + 2, groundRows = MaxMapY / 128 + 2;
	for (i = -1; i < groundCols; i++) {
		for (j = -1; j < groundRows; j++) {
			p->DDraw->Draw(p->DDraw->imgGround, 128*i+goffX, 128*j+goffY, 144, 144, 0, 0, MaxMapX, MaxMapY);
		}
	}

	// Mottle the ground with big soft light/dark blotches fixed in the world, so the repeating texture is less obvious
	{
		float ox = p->Player[me]->X + this->PlayerOffsetX, oy = p->Player[me]->Y + this->PlayerOffsetY;
		int ix0 = (int)floorf((ox - MaxMapX) / 256.0f) - 1, ix1 = (int)floorf(ox / 256.0f) + 1;
		int iy0 = (int)floorf((oy - MaxMapY) / 256.0f) - 1, iy1 = (int)floorf(oy / 256.0f) + 1;
		for (int cx = ix0; cx <= ix1; cx++) {
			for (int cy = iy0; cy <= iy1; cy++) {
				unsigned int hsh = (unsigned int)(cx * 73856093) ^ (unsigned int)(cy * 19349663);
				hsh ^= hsh >> 13; hsh *= 0x5bd1e995; hsh ^= hsh >> 15;
				int sx = (int)(ox - (cx * 256 + 40 + (int)(hsh % 176)));
				int sy = (int)(oy - (cy * 256 + 40 + (int)((hsh >> 8) % 176)));
				int rad = 90 + (int)((hsh >> 16) % 80);
				if (hsh & 0x1000000) p->DDraw->DrawGlow(sx, sy, rad, 0, 0, 0, 26, MaxMapX, MaxMapY, false);
				else p->DDraw->DrawGlow(sx, sy, rad, 255, 245, 200, 16, MaxMapX, MaxMapY, false);
			}
		}
	}

	// Cloud shadows drift slowly across the world (each blotch stays attached to its cell, so nothing pops)
	{
		float drift = p->Tick * 0.011f;
		float ox = p->Player[me]->X + this->PlayerOffsetX, oy = p->Player[me]->Y + this->PlayerOffsetY;
		int ix0 = (int)floorf((ox - drift - MaxMapX) / 512.0f) - 1, ix1 = (int)floorf((ox - drift) / 512.0f) + 1;
		int iy0 = (int)floorf((oy - MaxMapY) / 512.0f) - 1, iy1 = (int)floorf(oy / 512.0f) + 1;
		for (int cx = ix0; cx <= ix1; cx++) {
			for (int cy = iy0; cy <= iy1; cy++) {
				unsigned int hsh = (unsigned int)(cx * 2654435761u) ^ (unsigned int)(cy * 40503u + 977u);
				hsh ^= hsh >> 16; hsh *= 0x45d9f3b; hsh ^= hsh >> 13;
				if (hsh % 3 == 0) continue;   // gaps of sunshine
				float wx = cx * 512 + 100 + (hsh % 300) + drift, wy = cy * 512 + 100 + ((hsh >> 9) % 300);
				p->DDraw->DrawGlow((int)(ox - wx), (int)(oy - wy), 190 + (int)((hsh >> 18) % 90), 0, 10, 30, 30, MaxMapX, MaxMapY, false);
			}
		}
	}

	// Warm sunlight from the top-left, matching the shadows
	p->DDraw->DrawGlow(MaxMapX / 5, MaxMapY / 6, MaxMapX / 2, 255, 235, 190, 20, MaxMapX, MaxMapY);

	// Drifting dust motes catching the light
	for (int m = 0; m < 24; m++) {
		unsigned int hm = (unsigned int)(m + 1) * 2654435761u;
		float sp = 0.006f + (hm % 100) * 0.00012f;
		float px = fmodf((hm >> 8) % MaxMapX + p->Tick * sp * 2.0f, (float)MaxMapX);
		float py = fmodf((hm >> 4) % MaxMapY + p->Tick * sp, (float)MaxMapY) + 6.0f * sinf(p->Tick / 900.0f + m);
		int tw = (int)(30 + 40 * (0.5f + 0.5f * sinf(p->Tick / 400.0f + m * 1.7f)));
		p->DDraw->DrawGlow((int)px, (int)py, 2 + (m & 1), 255, 250, 225, tw, MaxMapX, MaxMapY);
	}

	// Draw landscape and CC tiles on top of the ground (tilesToDraw^2 total)
	for (i = -this->tilesToDraw; i<this->tilesToDraw; i++) {
		for (j = -this->tilesToDraw; j<this->tilesToDraw; j++) {
			tileX = myX + i;
			tileY = myY + j;

			// If the tile is on the map,
			if (tileX > 0 && tileY > 0 && tileX <= 512 && tileY <= 512) {

				// If the map square is Lava, draw Lava
				if (p->Map->map[tileX][tileY] == MAP_SQUARE_LAVA) {
					p->DDraw->Draw(p->DDraw->imgLava, (myX-tileX)*48+this->PlayerOffsetX+offX, (myY-tileY)*48+this->PlayerOffsetY+offY, 48, 48, p->Map->tiles[tileX][tileY], 0, MaxMapX, MaxMapY);
					{   // a tile now and then sparkles
						unsigned int hg = (unsigned int)(tileX * 73856093) ^ (unsigned int)(tileY * 19349663);
						hg ^= hg >> 11; hg *= 0x9E3779B1u;
						float ph = sinf(p->Tick / 260.0f + (hg & 1023) * 0.37f);
						if (ph > 0.9f) p->DDraw->DrawGlow((myX-tileX)*48+this->PlayerOffsetX+offX+8+(int)(hg>>10)%32, (myY-tileY)*48+this->PlayerOffsetY+offY+8+(int)(hg>>16)%32, 4, 235, 245, 255, (int)((ph - 0.9f) * 2000.0f), MaxMapX, MaxMapY);
					}
				}
				// Else if the map square is Rock, draw Rock
				else if (p->Map->map[tileX][tileY] == MAP_SQUARE_ROCK) {
					p->DDraw->DrawShadow((myX-tileX)*48+this->PlayerOffsetX+offX+26, (myY-tileY)*48+this->PlayerOffsetY+offY+40, 50, 20, 100, MaxMapX, MaxMapY);
					p->DDraw->DrawGlow((myX-tileX)*48+this->PlayerOffsetX+offX+18, (myY-tileY)*48+this->PlayerOffsetY+offY+16, 14, 255, 240, 200, 26, MaxMapX, MaxMapY);
					p->DDraw->Draw(p->DDraw->imgRocks, (myX-tileX)*48+this->PlayerOffsetX+offX, (myY-tileY)*48+this->PlayerOffsetY+offY, 48, 48, p->Map->tiles[tileX][tileY], 0, MaxMapX, MaxMapY);	
				}

				// If the player does not have the sector loaded,
				if (p->InGame->HasSector[(tileX / SectorSize)][(tileY / SectorSize)] == 0) {
					p->DDraw->Draw(p->DDraw->imgLoading, (myX-tileX)*48+this->PlayerOffsetX+offX, (myY-tileY)*48+this->PlayerOffsetY+offY, 48, 48, 0, 0, MaxMapX, MaxMapY);
				}
			}

			// Else (tile is off the map), paint it black
			else {
				p->DDraw->DFillRect((myX-tileX)*48+this->PlayerOffsetX+offX, (myY-tileY)*48+this->PlayerOffsetY+offY, 48, 48);
			}
		}
	}
}

/***************************************************************
 * Function:	DrawPanel
 *
 **************************************************************/
void CDrawing::DrawPanel() {

	// Draw the top half of the interface
	p->DDraw->Draw(p->DDraw->imgInterface, MaxMapX, 0, 200, 430);

	// Draw the bottom half of the interface
	p->DDraw->Draw(p->DDraw->imgInterfaceBottom, MaxMapX, 430, 200, 170);

	// Crisp labels on the panel buttons (their faces are painted bare when the art is loaded)
	{
		const COLORREF ink = RGB(24, 38, 62);
		const char *names[6] = { "STAFF", "MAP", "INFO", "POINTS", "OPTIONS", "HELP" };
		bool flashing = p->Player[p->Winsock->MyIndex]->isMayor && p->Personnel->ApplicantWaiting && p->Personnel->ApplicantAnim == 1;
		for (int b = 0; b < 6; b++) {
			if (b == 0 && flashing) continue;   // the flashing Staff art carries its own label
			p->DDraw->DButtonLabel(MaxMapX + 148, 269 + b * 22 + 3, 40, names[b], ink);
		}
		p->DDraw->DButtonLabel(MaxMapX + 129, 401 + 3, 59, "BUILD", ink);
	}

	// The panel art is 600 pixels tall; on taller views continue it as a bevelled metal block instead of leaving black
	if (p->ResolutionY > 600) {
		const int top = 600, h = p->ResolutionY - top;
		p->DDraw->DFillRectColor(MaxMapX, top, 200, h, 176, 176, 176);
		p->DDraw->DFillRectColor(MaxMapX, top, 200, 2, 217, 217, 217);            // lit top edge
		p->DDraw->DFillRectColor(MaxMapX, top, 3, h, 205, 205, 205);              // lit left edge
		p->DDraw->DFillRectColor(MaxMapX + 3, top + 2, 1, h - 2, 191, 191, 191);
		p->DDraw->DFillRectColor(MaxMapX + 196, top, 4, h, 45, 45, 45);           // shaded right edge
		p->DDraw->DFillRectColor(MaxMapX + 193, top + 2, 3, h - 2, 120, 120, 120);
		p->DDraw->DFillRectColor(MaxMapX, p->ResolutionY - 3, 200, 3, 45, 45, 45); // bottom edge
		p->DDraw->DFillRectColor(MaxMapX + 3, p->ResolutionY - 4, 190, 1, 130, 130, 130);
	}

	//If the player is Mayor,
	if (p->Player[p->Winsock->MyIndex]->isMayor) {

		// Draw the money box
		p->DDraw->Draw(p->DDraw->imgMoneyBox, MaxMapX+2, 224, 120, 19);

		// If the city is losing money, draw moneyDown and the total in red
		if (p->InGame->getGrossIncome() < 0) {
			p->DDraw->Draw(p->DDraw->imgMoneyDown, MaxMapX+8, 225, 10, 14);
			p->DDraw->DTextOut(MaxMapX+21, 226, CashFormat(p->InGame->Cash), COLOR_RED);
		}
		// Else (city is gaining money), draw moneyUp and the total in green
		else {
			p->DDraw->Draw(p->DDraw->imgMoneyUp, MaxMapX+8, 226, 10, 14);
			p->DDraw->DTextOut(MaxMapX+21, 226, CashFormat(p->InGame->Cash), COLOR_GREEN);
		}

		// If the mayor has an applicant,
		if (p->Personnel->ApplicantWaiting) {

			// Make the Staff button flash on a timer
			if (p->Personnel->ApplicantAnim == 1) {
				p->DDraw->Draw(p->DDraw->imgBtnStaff, MaxMapX + 147, 269, 42, 19);
			}
			if (p->Tick > p->Personnel->ApplicantTick) {
				p->Personnel->ApplicantAnim = 1 - p->Personnel->ApplicantAnim;
				p->Personnel->ApplicantTick = p->Tick + 500;
			}
		}
	}

	// Draw the Arrow
	this->DrawArrow();

	// Draw the Health,
	this->DrawHealth();

	// If the player is an admin,
	if (p->Player[p->Winsock->MyIndex]->isAdmin()) {

		// Draw the staff button as the Admin button
		p->DDraw->Draw(p->DDraw->imgBtnStaff, MaxMapX + 110, 460, 42, 19);
	}

	// Display Panel-Text lines 1 through 7
	p->DDraw->DTextOut(MaxMapX + 12, 465, PanelLine1, COLOR_PANEL_HEAD);
	p->DDraw->DTextOut(MaxMapX + 12, 480, PanelLine2, COLOR_PANEL_BODY);
	p->DDraw->DTextOut(MaxMapX + 12, 495, PanelLine3, COLOR_PANEL_BODY);
	p->DDraw->DTextOut(MaxMapX + 12, 510, PanelLine4, COLOR_PANEL_BODY);
	p->DDraw->DTextOut(MaxMapX + 12, 525, PanelLine5, COLOR_PANEL_BODY);
	p->DDraw->DTextOut(MaxMapX + 12, 540, PanelLine6, COLOR_PANEL_BODY);
	p->DDraw->DTextOut(MaxMapX + 12, 555, PanelLine7, COLOR_PANEL_BODY);
}

/***************************************************************
 * Function:	DrawTank
 *
 **************************************************************/
void CDrawing::DrawTank() {
	int me = p->Winsock->MyIndex;
	int tankImage;

	// If the player is dead,
	if (p->Player[me]->isDead == true) {
		std::ostringstream convert;
		int timeleft = (int)(((int)(p->InGame->timeDeath + TIMER_RESPAWN) - p->Tick) / 1000);
		if (timeleft < 0) {
			timeleft = 0;
		}
		convert << timeleft;

		// Draw dead message and respawn timer
		{
			std::string line1 = "You have been blown up!";
			std::string line2 = "You will respawn in: " + convert.str();
			int cy = MaxMapY / 2 - 114;
			p->DDraw->DrawShadow(MaxMapX / 2, cy + 14, 340, 76, 170, MaxMapX, MaxMapY);
			p->DDraw->DTextOut((MaxMapX - p->DDraw->MeasureText(line1)) / 2, cy, line1, COLOR_WHITE);
			p->DDraw->DTextOut((MaxMapX - p->DDraw->MeasureText(line2)) / 2, cy + 15, line2, COLOR_WHITE);
		}
		return;
	}

	// Draw the tank, over a faint pool of light that makes it easy to find
	p->DDraw->DrawGlow(this->PlayerOffsetX + 24, this->PlayerOffsetY + 30, 44, 255, 255, 255, 22, MaxMapX, MaxMapY);
	tankImage = this->getPlayerTankImage(p->Player[me], p->Player[me]);
	p->DDraw->DrawShadow(this->PlayerOffsetX + 27, this->PlayerOffsetY + 37, 58, 26, 150, MaxMapX, MaxMapY);
	p->DDraw->Draw(p->DDraw->imgTanks, this->PlayerOffsetX, this->PlayerOffsetY, 48, 48, (p->Player[me]->Direction / 2) * 48, tankImage * 48);

	// If Names are on,
	if (p->Options->names) {

		// Draw the name
		this->drawPlayerName(p->Player[me], p->Player[me]);
	}
}
 
/***************************************************************
 * Function:	DrawBuildings
 *
 **************************************************************/
void CDrawing::DrawBuildings() {
	int me = p->Winsock->MyIndex;
	int myOffsetX = this->getOffsetX(p->Player[me]->X);
	int myOffsetY = this->getOffsetY(p->Player[me]->Y);
	int offX = myOffsetX % 48;
	int offY = myOffsetY % 48;
	int myX = myOffsetX / 48;
	int myY = myOffsetY / 48;
	float curTick = p->Tick;
	CBuilding *bld;

	int tileX;
	int tileY;
	int buildingType;
	int buildingSubType;
	int by;
	int bX;

	int moffX;
	int moffY;
	int popX;
	int popY;
	int imgX;
	int firstno;
	int secondno;

	// For each building,
	bld = p->Build->buildingListHead;
	while (bld) {

		// If this building is within sight,
		if (this->isTileInSight(myOffsetX, myOffsetY, bld->X, bld->Y)) {

			// Draw the building
			tileX = (myX - bld->X) * 48;
			tileY = (myY - bld->Y) * 48;
			buildingType = bld->Type / 100;
			buildingSubType = bld->Type - buildingType*100;
			by = 144*buildingType;
			bX = (bld->Animation/2)*144;
			this->p->DDraw->DrawShadow(tileX+offX+84, tileY+offY+132, 150, 46, 130, MaxMapX, MaxMapY);
			this->p->DDraw->DrawShadow(tileX+offX+72, tileY+offY+126, 112, 20, 90, MaxMapX, MaxMapY);
			this->p->DDraw->Draw(this->p->DDraw->imgBuildings, tileX+offX, tileY+offY, 144, 144, bX, by, MaxMapX, MaxMapY);
			{
				float pl = 0.5f + 0.5f * sinf(p->Tick / 500.0f + bld->X);
				if (bld->isCC())            p->DDraw->DrawGlow(tileX+offX+72, tileY+offY+56, 64, 110, 170, 255, (int)(30 + 40 * pl), MaxMapX, MaxMapY);
				else if (bld->isHospital()) p->DDraw->DrawGlow(tileX+offX+72, tileY+offY+72, 58, 80, 255, 130, (int)(22 + 38 * pl), MaxMapX, MaxMapY);
				else if (bld->isFactory())  p->DDraw->DrawGlow(tileX+offX+72, tileY+offY+96, 52, 255, 130, 40, (int)(20 + 34 * pl), MaxMapX, MaxMapY);
			}

			moffX = 0;
			moffY = 0;
			popX = bld->pop * 48;
			popY = 0;
			imgX = buildingSubType * 32;
			firstno = bld->ItemsLeft / 10;
			secondno = bld->ItemsLeft - firstno*10;

			switch (buildingType) {

				// Building: CC
				case 0:
					popY = 48;
					this->p->DDraw->Draw(this->p->DDraw->imgPopulation, tileX+offX+96, tileY+offY+49, 48, 48, popX, popY, MaxMapX, MaxMapY);
					break;

				// Building: House
				case 2:
					this->p->DDraw->Draw(this->p->DDraw->imgPopulation, tileX+offX+96, tileY+offY+33, 48, 48, popX, popY, MaxMapX, MaxMapY);
					break;

				// Building: Hospital
				case 3:
					this->p->DDraw->Draw(this->p->DDraw->imgPopulation, tileX+offX+92, tileY+offY+92, 48, 48, popX, popY, MaxMapX, MaxMapY);
					break;

				// Building: Factory
				case 1:
					moffX = 56;
					moffY = 52;
					this->p->DDraw->Draw(this->p->DDraw->imgBlackNumbers, tileX+offX+56, tileY+offY+84, 16, 16, 16*firstno, 0, MaxMapX, MaxMapY);
					this->p->DDraw->Draw(this->p->DDraw->imgBlackNumbers, tileX+offX+72, tileY+offY+84, 16, 16, 16*secondno, 0, MaxMapX, MaxMapY);
					this->p->DDraw->Draw(this->p->DDraw->imgPopulation, tileX+offX+96, tileY+offY+48, 48, 48, popX, popY, MaxMapX, MaxMapY);

					if (bld->Smoke > 0) {
						this->p->DDraw->Draw(this->p->DDraw->imgSmoke, tileX+offX+6, tileY+offY-15, 180, 60, 0, (bld->Smoke-1)*60, MaxMapX, MaxMapY);
					}
					this->p->DDraw->Draw(this->p->DDraw->imgItems, tileX+offX+moffX, tileY+offY+moffY, 32, 32, imgX, 0, MaxMapX, MaxMapY);
					break;

				// Building: Research
				case 4:
					moffX = 14;
					moffY = 98;
					this->p->DDraw->Draw(this->p->DDraw->imgPopulation, tileX+offX+96, tileY+offY+90, 48, 48, popX, popY, MaxMapX, MaxMapY);
					this->p->DDraw->Draw(this->p->DDraw->imgItems, tileX+offX+moffX, tileY+offY+moffY, 32, 32, imgX, 0, MaxMapX, MaxMapY);
					break;
			}
		}

		// Get the next building
		bld = bld->next;
	}
}

/***************************************************************
 * Function:	DrawPlayers
 *
 **************************************************************/
void CDrawing::DrawPlayers() {
	int me = p->Winsock->MyIndex;
	int tankimg = 0;
	int myOffsetX = this->getOffsetX(p->Player[me]->X);
	int myOffsetY = this->getOffsetY(p->Player[me]->Y);
	int relativeX;
	int relativeY;
	int nameWidth;
	int centerPoint;
	string nameString;
	string townString;
	int lagWidth;
	int townWidth;

	// For each player,
	for (int i = 0; i < MAX_PLAYERS; i++) {

		// If the player is in game, isn't this player, and is in sight,
		if ((p->Player[i]->isInGame) && (me != i) && this->isPositionInSight(myOffsetX, myOffsetY, p->Player[i]->X, p->Player[i]->Y)) {
		
			// Get the player's position relative to my position
			relativeX = myOffsetX - p->Player[i]->X;
			relativeY = myOffsetY - p->Player[i]->Y;

			// Get the player's tank image
			tankimg = this->getPlayerTankImage(p->Player[i], p->Player[me]);

			// If tankimg != -1, draw the tank
			if (tankimg != -1) {
				p->DDraw->DrawShadow(relativeX + 27, relativeY + 37, 58, 26, 150, MaxMapX, MaxMapY);
				p->DDraw->Draw(p->DDraw->imgTanks, relativeX, relativeY, 48, 48, (p->Player[i]->Direction / 2) * 48, tankimg * 48, MaxMapX, MaxMapY);
			}

			// If Names are on,
			if (p->Options->names) {

				// Draw the name
				this->drawPlayerName(p->Player[i], p->Player[me]);
			}
		}
	}
}

/***************************************************************
 * Function:	DrawBuildMenu
 *
 **************************************************************/
void CDrawing::DrawBuildMenu() {
	const int R = BUILD_MENU_ROW;
	int minY = 0;

	// For each item,
	for (int i = 0; i < 26; i++) {

		// If the player can build the item, or the player is an admin,
		if (p->InGame->CanBuild[i] || p->Player[p->Winsock->MyIndex]->isAdmin()) {
			minY += R;
		}
	}

	// If the build menu needs more space than BuildMenuY, set BuildMenuY
	// This causes the build menu to not overflow past the top of the screen
	if (minY > this->BuildMenuY) {
		this->BuildMenuY = minY;
	}
	int DrawY = this->BuildMenuY;       // top of the Demolish row; the rows above it are the buildings

	// Draw the background box (a little padding around the rows)
	p->DDraw->DFillRect(BuildMenuX-24, DrawY-minY-3, 204, minY+R+6);

	// Highlight the row under the pointer so it is clear what a click will pick
	{
		int mx = p->Input->LastMouseX, my = p->Input->LastMouseY;
		if (mx >= BuildMenuX-26 && mx <= BuildMenuX+180 && my >= DrawY-minY && my < DrawY+R) {
			int rowTop = DrawY - R * ((DrawY + R - 1 - my) / R);
			p->DDraw->DFillRectColor(BuildMenuX-24, rowTop, 204, R, 70, 70, 120);
		}
	}

	// Draw Demolish
	p->DDraw->Draw(p->DDraw->imgBuildIcons, BuildMenuX - 20, DrawY + (R-16)/2, 16, 16, 13*16);
	p->DDraw->DTextOut(BuildMenuX + 2, DrawY + (R-16)/2, "Demolish", COLOR_YELLOW);
	DrawY -= R;

	// For each item,
	for (int i = 0; i < 26; i++) {

		// If the player can build the item, or the player is an admin,
		if (p->InGame->CanBuild[i] || p->Player[p->Winsock->MyIndex]->isAdmin()) { 

			p->DDraw->Draw(p->DDraw->imgBuildIcons, BuildMenuX - 20, DrawY + (R-16)/2, 16, 16, buildButton[i]*16);
			p->DDraw->DTextOut(BuildMenuX + 2, DrawY + (R-16)/2, buildNames[i], COLOR_YELLOW);
			DrawY -= R;
		}
	}
	
}

/***************************************************************
 * Function:	DrawBuildPlacement
 *
 **************************************************************/
int CDrawing::refOffsetX() { return p->Options->resolution1024 ? 388 : 276; }
int CDrawing::refOffsetY() { return p->Options->resolution1024 ? 360 : 276; }

void CDrawing::DrawBuildPlacement() {
	
	// If the player has a Building or Demolish on the cursor,
	if (p->InGame->IsBuilding != 0) {
		int me = p->Winsock->MyIndex;
		int offX = (int)p->Player[me]->X % 48;
		int offY = (int)p->Player[me]->Y % 48;
		// The offsets below were tuned with the tank at its original on-screen spot; shift by how far it is from there now
		int shiftX = this->PlayerOffsetX - this->refOffsetX(), shiftY = this->PlayerOffsetY - this->refOffsetY();
		int tX = floorDiv(p->Input->LastMouseX - shiftX, 48) * 48 + this->buildingCursorOffsetX + shiftX;
		int tY = floorDiv(p->Input->LastMouseY - shiftY, 48) * 48 + this->buildingCursorOffsetY + shiftY;
		int buildingType = buildingTypes[p->InGame->IsBuilding-1] / 100;
		int buildingSubType = buildingTypes[p->InGame->IsBuilding-1] - buildingType*100;
		int moffX = 0;
		int moffY = 0; 

		// If the player has a Building on the cursor,
		if (p->InGame->IsBuilding > 0) {
			tX = floorDiv(p->Input->LastMouseX - shiftX, 48) * 48 + this->buildingCursorOffsetX + shiftX;
			tY = floorDiv(p->Input->LastMouseY - shiftY, 48) * 48 + this->buildingCursorOffsetY + shiftY;

			p->DDraw->Draw(p->DDraw->imgBuildings, tX+offX, tY+offY, 144, 144, 0, (buildingTypes[p->InGame->IsBuilding-1]/100)*144, MaxMapX, MaxMapY);
			int imgX = buildingSubType * 32;

			// Factory
			if (buildingType == 1) {
				moffX = 56;
				moffY = 52;
				p->DDraw->Draw(p->DDraw->imgItems, tX+offX+moffX, tY+offY+moffY, 32, 32, imgX, 0, MaxMapX, MaxMapY); 
			}
			else if(buildingType == 4) {
				moffX = 14;
				moffY = 98;
				p->DDraw->Draw(p->DDraw->imgItems, tX+offX+moffX, tY+offY+moffY, 32, 32, imgX, 0, MaxMapX, MaxMapY); 
			}
		}

		// Else (player has Demolish on the cursor),
		else {
			tX = floorDiv(p->Input->LastMouseX - shiftX, 48) * 48 + this->demoCursorOffsetX + shiftX;
			tY = floorDiv(p->Input->LastMouseY - shiftY, 48) * 48 + this->demoCursorOffsetY + shiftY;

			p->DDraw->Draw(p->DDraw->imgDemolish, tX+offX+moffX, tY+offY+moffY, 32, 32, 0, 0, MaxMapX, MaxMapY); 
		}
	}
}

/***************************************************************
 * Function:	DrawArrow
 *
 **************************************************************/
void CDrawing::DrawArrow() {
	int me = this->p->Winsock->MyIndex;
	int difX;
	int difY;

	// If the Arrow timer is up,
	if (this->p->Tick > this->arrowTick) {

		// Reset the Arrow timer
		this->arrowTick = this->p->Tick + 100;

		// If the player is under attack, and the Arrow Flash timer is up,
		if ( (this->p->InGame->isUnderAttack) && (this->p->Tick > this->arrowFlashTick) ) {

			// Toggle flashArrow
			this->flashArrow = !this->flashArrow;

			// Reset the Flash Arrow timer
			this->arrowFlashTick = this->p->Tick + 500;
		}

		// Compare player's X coords to city's X coords
		difX = (int)this->p->Player[me]->CityX - (int)p->Player[me]->X;
		if (difX == 0) {
			difX = 1;
		}

		// Compare player's Y coords to city's Y coords
		difY = (int)this->p->Player[me]->CityY - (int)this->p->Player[me]->Y;
		if (difY == 0) {
			difY = 1;
		}

		// West
		if (this->p->Player[me]->X <= this->p->Player[me]->CityX) {

			// North West
			if (this->p->Player[me]->CityY >= this->p->Player[me]->Y)  {

				if ((difX) / (difY) > 2) {
					lastArrow = 160;
				}
				else if ((difY) / (difX) > 2) {
					lastArrow = 80;
				}
				else {
					lastArrow = 120;
				}
			}

			// South West
			if (p->Player[me]->CityY < p->Player[me]->Y) {
				if ((difX) / (difY) > 2) {
					lastArrow = 160;
				}
				else if ((difY) / (difX) > 2) {
					lastArrow = 240;
				}
				else {
					lastArrow = 200;
				}
			}
		}
		
		// East
		else  {

			// North East
			if (p->Player[me]->CityY >= p->Player[me]->Y) {
				if ((difX) / (difY) > 2) {
					lastArrow = 0;
				}
				else if ((difY) / (difX) > 2) {
					lastArrow = 80;
				}
				else {
					lastArrow = 40;
				}
			}

			// South East
			if (p->Player[me]->CityY < p->Player[me]->Y) {
				if ((difX) / (difY) > 2) {
					lastArrow = 0;
				}
				else if ((difY) / (difX) > 2) {
					lastArrow = 240;
				}
				else {
					lastArrow = 280;
				}
			}
		}
	}
	
	// If the player is under attack, draw red arrows
	if (p->InGame->isUnderAttack && this->flashArrow) {
		p->DDraw->Draw(p->DDraw->imgArrowsRed, MaxMapX + 5, 160, 40, 40, lastArrow);
	}

	// Else (not under attack), draw normal arrows
	else {
		p->DDraw->Draw(p->DDraw->imgArrows, MaxMapX + 5, 160, 40, 40, lastArrow);
	}
}

/***************************************************************
 * Function:	DrawHealth
 *
 **************************************************************/
void CDrawing::DrawHealth()
{
	float percent = (float(p->Player[p->Winsock->MyIndex]->HP) / float(40));
	if (p->Player[p->Winsock->MyIndex]->HP <= 12 && !p->Player[p->Winsock->MyIndex]->isDead)
		p->DDraw->DrawGlow(MaxMapX + 156, 236, 40, 255, 40, 40, (int)(50 + 70 * (0.5f + 0.5f * sinf(p->Tick / 170.0f))), p->ResolutionX, p->ResolutionY);
	p->DDraw->Draw(p->DDraw->imgHealth, (MaxMapX + 137), 250-int(percent * 87), 38, int(percent * 87));
}

/***************************************************************
 * Function:	DrawItems
 *
 **************************************************************/
void CDrawing::DrawItems() {
	int me = p->Winsock->MyIndex;
	int myOffsetX = p->Player[me]->X + this->PlayerOffsetX;
	int myOffsetY = p->Player[me]->Y + this->PlayerOffsetY;
	int offX = myOffsetX % 48;
	int offY = myOffsetY % 48;
	int myX = myOffsetX / 48;
	int myY = myOffsetY / 48;
	int tileX;
	int tileY;
	int orientation;
	CItem *itm;

	// For each item,
	itm = p->Item->itemListHead;
	while (itm) {

		// If the player has the item's sector,
		if (p->InGame->HasSector[itm->X / SectorSize][itm->Y / SectorSize]) {

			// If the item is in sight,
			if (this->isTileInSight(myOffsetX, myOffsetY, itm->X, itm->Y)) {

				tileX = ((myX - itm->X) * 48)+offX;
				tileY = ((myY - itm->Y) * 48)+10+offY;

				// Pick-ups float up and down above a small shadow
				int bob = 0;
				switch (itm->Type) {
					case ITEM_TYPE_ORB: case ITEM_TYPE_MEDKIT: case ITEM_TYPE_CLOAK: case ITEM_TYPE_ROCKET: case ITEM_TYPE_FLARE: case ITEM_TYPE_WALL:
						bob = (int)(2.5f * sinf(p->Tick / 320.0f + itm->X * 0.7f));
						p->DDraw->DrawShadow(tileX + 24, tileY + 40, 30, 11, 80, MaxMapX, MaxMapY);
						tileY += bob;
						break;
					default: break;
				}
				if (itm->Type == ITEM_TYPE_FLARE && itm->active) p->DDraw->DrawGlow(tileX + 24, tileY + 24, 150 + rand() % 12, 255, 200, 110, 55, MaxMapX, MaxMapY);

				// Pick-ups glow softly so they catch the eye (never mines or other hidden items)
				{
					float pulse = 0.5f + 0.5f * sinf(p->Tick / 260.0f + itm->X);
					switch (itm->Type) {
						case ITEM_TYPE_ORB:    p->DDraw->DrawGlow(tileX + 24, tileY + 26, 34, 170, 110, 255, (int)(60 + 70 * pulse), MaxMapX, MaxMapY); break;
						case ITEM_TYPE_BOMB:   if (itm->active && ((int)(p->Tick / 250) & 1)) p->DDraw->DrawGlow(tileX + 24, tileY + 26, 30, 255, 40, 30, 150, MaxMapX, MaxMapY); break;
						case ITEM_TYPE_PLASMA: p->DDraw->DrawGlow(tileX + 24, tileY + 12, 28, 90, 200, 255, (int)(40 + 50 * pulse), MaxMapX, MaxMapY); break;
						case ITEM_TYPE_MEDKIT: p->DDraw->DrawGlow(tileX + 24, tileY + 26, 30, 70, 255, 110, (int)(50 + 60 * pulse), MaxMapX, MaxMapY); break;
						case ITEM_TYPE_CLOAK:
						case ITEM_TYPE_ROCKET:
						case ITEM_TYPE_FLARE:
						case ITEM_TYPE_WALL:   p->DDraw->DrawGlow(tileX + 24, tileY + 26, 28, 255, 225, 140, (int)(35 + 45 * pulse), MaxMapX, MaxMapY); break;
						default: break;
					}
				}

				// Switch on Item Type
				switch (itm->Type) {

					// Type: BOMB
					case ITEM_TYPE_BOMB:
						if (itm->active) 
							p->DDraw->Draw(p->DDraw->imgItems, tileX, tileY, 48, 48, 144, 91, MaxMapX, MaxMapY);
						else 
							p->DDraw->Draw(p->DDraw->imgItems, tileX, tileY, 48, 48, itm->Type*48, 42, MaxMapX, MaxMapY);
						break;

					// Type: ORB
					case ITEM_TYPE_ORB:
						p->DDraw->Draw(p->DDraw->imgItems, tileX, tileY, 48, 48, itm->Type*48, 42 + itm->Animation * 48, MaxMapX, MaxMapY);
						break;

					// Type: MINE
					case ITEM_TYPE_MINE:

					// Type: DFG
					case ITEM_TYPE_DFG:
						if (itm->active && (itm->City != p->Player[me]->City) && (p->Player[me]->isAdmin() == false)) {
							//Don't draw
						}
						else {
							p->DDraw->Draw(p->DDraw->imgItems, tileX, tileY, 48, 48, itm->Type*48, 42, MaxMapX, MaxMapY);
						}
						break;

					// Type: SLEEPER
					case ITEM_TYPE_SLEEPER:
						if ((itm->target > -1) || (itm->City == p->Player[me]->City) || p->Player[p->Winsock->MyIndex]->isAdmin()) {
							p->DDraw->Draw(p->DDraw->imgTurretBase, tileX, tileY-10, 48, 48, itm->Animation * 48, (itm->Type-9)*48, MaxMapX, MaxMapY);
							orientation = (int)(itm->Angle/22.5);
							p->DDraw->Draw(p->DDraw->imgTurretHead, tileX, tileY-10, 48, 48, orientation*48, (itm->Type-9)*48, MaxMapX, MaxMapY);
						}
						break;

					case ITEM_TYPE_CLOAK:

					case ITEM_TYPE_ROCKET:

					case ITEM_TYPE_MEDKIT:

					case ITEM_TYPE_FLARE:

					case ITEM_TYPE_WALL:
						p->DDraw->Draw(p->DDraw->imgItems, tileX, tileY, 48, 48, itm->Type*48, 42, MaxMapX, MaxMapY);
						break;

					case ITEM_TYPE_TURRET:

					case ITEM_TYPE_PLASMA:
						p->DDraw->Draw(p->DDraw->imgTurretBase, tileX, tileY-10, 48, 48, itm->Animation * 48, (itm->Type-9)*48, MaxMapX, MaxMapY);
						orientation = (int)(itm->Angle/22.5);
						p->DDraw->Draw(p->DDraw->imgTurretHead, tileX, tileY-10, 48, 48, orientation*48, (itm->Type-9)*48, MaxMapX, MaxMapY);
						break;

				}
			}
		}

		// Get the next item
		itm = itm->next;
	}

}

/***************************************************************
 * Function:	DrawChat
 *
 **************************************************************/
void CDrawing::DrawChat() {
	int Y;

	// Draw info lines
	Y = 0;
	p->DDraw->DTextOut(0, Y, p->InGame->playerInfoLine1, p->InGame->playerInfoColor1);
	Y += 14;
	p->DDraw->DTextOut(0, Y, p->InGame->playerInfoLine2, p->InGame->playerInfoColor2);
	Y += 14;
	p->DDraw->DTextOut(0, Y, p->InGame->playerInfoLine3, p->InGame->playerInfoColor3);
	Y += 14;
	p->DDraw->DTextOut(0, Y, p->InGame->playerInfoLine4, p->InGame->playerInfoColor4);

	// If mouse is over chat,
	Y = MaxMapY - 126;
	if (p->Input->MouseOverChat) {
		// A translucent dark band with a soft top edge, so the map stays visible behind the chat
		for (int i = 0; i < 8; i++) p->DDraw->DFillRectAlpha(0, Y - 16 + i * 2, MaxMapX, 2, 0, 0, 0, 12 + i * 12);
		p->DDraw->DFillRectAlpha(0, Y, MaxMapX, 126, 0, 0, 0, 110);
	}

	// Draw chat lines
	p->DDraw->DTextOut(0, Y, p->InGame->chatLine1, p->InGame->chatColor1);
	Y += 14;
	p->DDraw->DTextOut(0, Y, p->InGame->chatLine2, p->InGame->chatColor2);
	Y += 14;
	p->DDraw->DTextOut(0, Y, p->InGame->chatLine3, p->InGame->chatColor3);
	Y += 14;
	p->DDraw->DTextOut(0, Y, p->InGame->chatLine4, p->InGame->chatColor4);
	Y += 14;
	p->DDraw->DTextOut(0, Y, p->InGame->chatLine5, p->InGame->chatColor5);
	Y += 14;
	p->DDraw->DTextOut(0, Y, p->InGame->chatLine6, p->InGame->chatColor6);
	Y += 14;
	p->DDraw->DTextOut(0, Y, p->InGame->chatLine7, p->InGame->chatColor7);
	Y += 14;
	p->DDraw->DTextOut(0, Y, p->InGame->chatLine8, p->InGame->chatColor8);
	Y += 14;

	// If chatting, fill just the chat bar with black
	if (p->InGame->IsChatting == 1) {
		p->DDraw->DFillRect(0, Y, MaxMapX, 14);
		p->DDraw->DTextOut(0, Y, p->InGame->ChatLine, COLOR_YELLOW);
	}
}

/***************************************************************
 * Function:	DrawInfo
 *
 **************************************************************/
void CDrawing::DrawInfo() {
}

/***************************************************************
 * Function:	DrawInventory
 *
 **************************************************************/
void CDrawing::DrawInventory() {
	CItem *itm;
	int DrawX;
	int DrawY;
	ostringstream stringConverter;
	int OrbAnimation = 0;
	int ItemCount[12];
	memset(ItemCount, 0, sizeof(ItemCount));

	// For each item,
	itm = p->Inventory->itemListHead;
	while (itm) {

		// Type: ORB
		if (itm->Type == 5) {

			// Set orb Animation
			OrbAnimation = itm->Animation;
		}
		ItemCount[itm->Type]++;

		// Get the next item
		itm = itm->next;
	}

	// Laser
	if (ItemCount[0] > 0) {
		DrawX = (MaxMapX + 7);
		DrawY = 267;

		// If selected, draw selection image under item
		if (p->Inventory->SelectedItemType == 0) {
			p->DDraw->Draw(p->DDraw->imgInventorySelection, DrawX, DrawY, 32, 32, 0, 0);
		}
		p->DDraw->Draw(p->DDraw->imgItems, DrawX, DrawY, 32, 32, 0, 0);

		// If more than one, draw the number
		if (ItemCount[0] > 1) {
			stringConverter << ItemCount[0];
			p->DDraw->DTextOut(DrawX+22, DrawY+12, stringConverter.str(), COLOR_YELLOW);
			stringConverter.str("");
		}
	}

	// Rocket
	if (ItemCount[1] > 0) {
		DrawX = (MaxMapX + 42);
		DrawY = 267;

		// If selected, draw selection image under item
		if (p->Inventory->SelectedItemType == 1) {
			p->DDraw->Draw(p->DDraw->imgInventorySelection, DrawX, DrawY, 32, 32, 0, 0);
		}
		p->DDraw->Draw(p->DDraw->imgItems, DrawX, DrawY, 32, 32, 1*32, 0);

		// If more than one, draw the number
		if (ItemCount[1] > 1) {
			stringConverter << ItemCount[1];
			p->DDraw->DTextOut(DrawX+22, DrawY+12, stringConverter.str(), COLOR_YELLOW);
			stringConverter.str("");
		}
	}

	// Medkit
	if (ItemCount[2] > 0) {
		DrawX = (MaxMapX + 77);
		DrawY = 267;

		// If selected, draw selection image under item
		if (p->Inventory->SelectedItemType == 2) {
			p->DDraw->Draw(p->DDraw->imgInventorySelection, DrawX, DrawY, 32, 32, 0, 0);
		}
		p->DDraw->Draw(p->DDraw->imgItems, DrawX, DrawY, 32, 32, 2*32, 0);

		// If more than one, draw the number
		if (ItemCount[2] > 1) {
			stringConverter << ItemCount[2];
			p->DDraw->DTextOut(DrawX+22, DrawY+12, stringConverter.str(), COLOR_YELLOW);
			stringConverter.str("");
		}
	}

	// Bomb
	if (ItemCount[3] > 0) {
		DrawX = (MaxMapX + 7);
		DrawY = 302;

		// If selected, draw selection image under item
		if (p->Inventory->SelectedItemType == 3) {
			p->DDraw->Draw(p->DDraw->imgInventorySelection, DrawX, DrawY, 32, 32, 0, 0);
		}

		// If activated, draw active image
		if (p->InGame->BombsAreActivated) {
			p->DDraw->Draw(p->DDraw->imgItems, DrawX, DrawY, 32, 32, 152, 89);
		}
		// Else (not activated), draw normal image
		else {
			p->DDraw->Draw(p->DDraw->imgItems, DrawX, DrawY, 32, 32, 3*32, 0);
		}

		// If more than one, draw the number
		if (ItemCount[3] > 1) {
			stringConverter << ItemCount[3];
			p->DDraw->DTextOut(DrawX+22, DrawY+12, stringConverter.str(), COLOR_YELLOW);
			stringConverter.str("");
		}
	}

	// Mine
	if (ItemCount[4] > 0) {
		DrawX = (MaxMapX + 42);
		DrawY = 302;
		
		// If selected, draw selection image under item
		if (p->Inventory->SelectedItemType == 4) {
			p->DDraw->Draw(p->DDraw->imgInventorySelection, DrawX, DrawY, 32, 32, 0, 0);
		}
		p->DDraw->Draw(p->DDraw->imgItems, DrawX, DrawY, 32, 32, 4*32, 0);

		// If more than one, draw the number
		if (ItemCount[4] > 1) {
			stringConverter << ItemCount[4];
			p->DDraw->DTextOut(DrawX+22, DrawY+12, stringConverter.str(), COLOR_YELLOW);
			stringConverter.str("");
		}
	}

	// Orb
	if (ItemCount[5] > 0) {
		DrawX = (MaxMapX + 77);
		DrawY = 302;

		// If selected, draw selection image under item
		if (p->Inventory->SelectedItemType == 5) {
			p->DDraw->Draw(p->DDraw->imgInventorySelection, DrawX, DrawY, 32, 32, 0, 0);
		}
		p->DDraw->Draw(p->DDraw->imgItems, DrawX, DrawY, 32, 32, 250, 41 + (OrbAnimation * 32) + (OrbAnimation * 16));

		// If more than one, draw the number
		if (ItemCount[5] > 1) {
			stringConverter << ItemCount[5];
			p->DDraw->DTextOut(DrawX+22, DrawY+12, stringConverter.str(), COLOR_YELLOW);
			stringConverter.str("");
		}
	}

	// Flare
	if (ItemCount[6] > 0) {
		DrawX = (MaxMapX + 7);
		DrawY = 337;

		// If selected, draw selection image under item
		if (p->Inventory->SelectedItemType == 6) {
			p->DDraw->Draw(p->DDraw->imgInventorySelection, DrawX, DrawY, 32, 32, 0, 0);
		}
		p->DDraw->Draw(p->DDraw->imgItems, DrawX, DrawY, 32, 32, 6*32, 0);

		// If more than one, draw the number
		if (ItemCount[6] > 1) {
			stringConverter << ItemCount[6];
			p->DDraw->DTextOut(DrawX+22, DrawY+12, stringConverter.str(), COLOR_YELLOW);
			stringConverter.str("");
		}
	}

	// DFG
	if (ItemCount[7] > 0) {
		DrawX = (MaxMapX + 42);
		DrawY = 337;

		// If selected, draw selection image under item
		if (p->Inventory->SelectedItemType == 7) {
			p->DDraw->Draw(p->DDraw->imgInventorySelection, DrawX, DrawY, 32, 32, 0, 0);
		}
		p->DDraw->Draw(p->DDraw->imgItems, DrawX, DrawY, 32, 32, 7*32, 0);

		// If more than one, draw the number
		if (ItemCount[7] > 1) {
			stringConverter << ItemCount[7];
			p->DDraw->DTextOut(DrawX+22, DrawY+12, stringConverter.str(), COLOR_YELLOW);
			stringConverter.str("");
		}
	}

	// Wall
	if (ItemCount[8] > 0) {
		DrawX = (MaxMapX + 77);
		DrawY = 337;

		// If selected, draw selection image under item
		if (p->Inventory->SelectedItemType == 8) {
			p->DDraw->Draw(p->DDraw->imgInventorySelection, DrawX, DrawY, 32, 32, 0, 0);
		}
		p->DDraw->Draw(p->DDraw->imgItems, DrawX, DrawY, 32, 32, 8*32, 0);

		// If more than one, draw the number
		if (ItemCount[8] > 1) {
			stringConverter << ItemCount[8];
			p->DDraw->DTextOut(DrawX+22, DrawY+12, stringConverter.str(), COLOR_YELLOW);
			stringConverter.str("");
		}
	}

	// Turret
	if (ItemCount[9] > 0) {
		DrawX = (MaxMapX + 7);
		DrawY = 372;

		// If selected, draw selection image under item
		if (p->Inventory->SelectedItemType == 9) {
			p->DDraw->Draw(p->DDraw->imgInventorySelection, DrawX, DrawY, 32, 32, 0, 0);
		}
		p->DDraw->Draw(p->DDraw->imgItems, DrawX, DrawY, 32, 32, 9*32, 0);

		// If more than one, draw the number
		if (ItemCount[9] > 1) {
			stringConverter << ItemCount[9];
			p->DDraw->DTextOut(DrawX+22, DrawY+12, stringConverter.str(), COLOR_YELLOW);
			stringConverter.str("");
		}
	}

	// Sleeper
	if (ItemCount[10] > 0) {
		DrawX = (MaxMapX + 42);
		DrawY = 372;

		// If selected, draw selection image under item
		if (p->Inventory->SelectedItemType == 10) {
			p->DDraw->Draw(p->DDraw->imgInventorySelection, DrawX, DrawY, 32, 32, 0, 0);
		}
		p->DDraw->Draw(p->DDraw->imgItems, DrawX, DrawY, 32, 32, 10*32, 0);

		// If more than one, draw the number
		if (ItemCount[10] > 1) {
			stringConverter << ItemCount[10];
			p->DDraw->DTextOut(DrawX+22, DrawY+12, stringConverter.str(), COLOR_YELLOW);
			stringConverter.str("");
		}
	}

	// Plasma
	if (ItemCount[11] > 0) {
		DrawX = (MaxMapX + 77);
		DrawY = 372;

		// If selected, draw selection image under item
		if (p->Inventory->SelectedItemType == 11) {
			p->DDraw->Draw(p->DDraw->imgInventorySelection, DrawX, DrawY, 32, 32, 0, 0);
		}
		p->DDraw->Draw(p->DDraw->imgItems, DrawX, DrawY, 32, 32, 11*32, 0);

		// If more than one, draw the number
		if (ItemCount[11] > 1) {
			stringConverter << ItemCount[11];
			p->DDraw->DTextOut(DrawX+22, DrawY+12, stringConverter.str(), COLOR_YELLOW);
			stringConverter.str("");
		}
	}
}

/***************************************************************
 * Function:	DrawDust
 *
 * Moving tanks kick up small dust puffs that swell and fade. Drawn
 * before the tanks so the puffs trail out from underneath them.
 **************************************************************/
void CDrawing::DrawDust() {
	const float LIFE = 650.0f;
	int me = p->Winsock->MyIndex;
	float now = p->Tick;
	static float lastSmoke = 0;

	// Spawn: any visible tank that moved since the last frame
	for (int i = 0; i < MAX_PLAYERS; i++) {
		CPlayer *pl = p->Player[i];
		float dx = pl->X - lastTankX[i], dy = pl->Y - lastTankY[i];
		bool moved = (dx * dx + dy * dy) > 0.25f && (dx * dx + dy * dy) < 40000.0f;
		lastTankX[i] = pl->X;
		lastTankY[i] = pl->Y;
		if (!moved || !pl->isInGame || pl->isDead || (i != me && pl->isCloaked)) continue;
		// Direction of travel on screen: the view is mirrored (larger world X is further left), so "behind the tank" is +(dx,dy)
		float len = sqrtf(dx * dx + dy * dy);
		float nx = len > 0.01f ? dx / len : 0, ny = len > 0.01f ? dy / len : 0;
		float mx = pl->X - lastMarkX[i], my = pl->Y - lastMarkY[i];
		if (mx * mx + my * my > 100.0f && marks.size() < 400) {
			lastMarkX[i] = pl->X;
			lastMarkY[i] = pl->Y;
			// twin tracks, either side of the tank
			DustPuff m1 = { pl->X, pl->Y, now, (int)(-ny * 11.0f + nx * 6.0f), (int)(nx * 11.0f + ny * 6.0f), 0 };
			DustPuff m2 = { pl->X, pl->Y, now, (int)(ny * 11.0f + nx * 6.0f), (int)(-nx * 11.0f + ny * 6.0f), 0 };
			marks.push_back(m1);
			marks.push_back(m2);
		}
		if (now - lastPuff[i] < 70.0f || dust.size() >= 200) continue;
		lastPuff[i] = now;
		DustPuff d = { pl->X, pl->Y, now, rand() % 11 - 5 + (int)(nx * 26.0f), rand() % 9 - 4 + (int)(ny * 26.0f), 0 };
		dust.push_back(d);
	}

	// A badly damaged tank of mine smokes
	if (p->Player[me]->isInGame && !p->Player[me]->isDead && p->Player[me]->HP > 0 && p->Player[me]->HP <= 15 && now - lastSmoke > 110.0f && dust.size() < 200) {
		lastSmoke = now;
		DustPuff s = { p->Player[me]->X, p->Player[me]->Y, now, rand() % 15 - 7, rand() % 7 - 3, 1 };
		dust.push_back(s);
	}

	// Scorch marks from big blasts lie under everything
	{
		size_t sk = 0;
		for (size_t k = 0; k < scorch.size(); k++) {
			float t = (now - scorch[k].born) / 25000.0f;
			if (t >= 1.0f || t < 0.0f) continue;
			scorch[sk++] = scorch[k];
			int sx = (int)(p->Player[me]->X - scorch[k].wx) + this->PlayerOffsetX + 72;
			int sy = (int)(p->Player[me]->Y - scorch[k].wy) + this->PlayerOffsetY + 76;
			p->DDraw->DrawGlow(sx, sy, 46, 10, 8, 6, (int)(60.0f * (1.0f - t)), MaxMapX, MaxMapY, false);
			p->DDraw->DrawGlow(sx, sy, 24, 0, 0, 0, (int)(40.0f * (1.0f - t)), MaxMapX, MaxMapY, false);
		}
		scorch.resize(sk);
	}

	// Tread marks first, so they lie on the ground under everything else
	size_t mkeep = 0;
	for (size_t k = 0; k < marks.size(); k++) {
		float t = (now - marks[k].born) / 4000.0f;
		if (t >= 1.0f || t < 0.0f) continue;
		marks[mkeep++] = marks[k];
		int sx = (int)(p->Player[me]->X - marks[k].wx) + this->PlayerOffsetX + 24 + marks[k].jx;
		int sy = (int)(p->Player[me]->Y - marks[k].wy) + this->PlayerOffsetY + 28 + marks[k].jy;
		p->DDraw->DrawGlow(sx, sy, 5, 12, 8, 4, (int)(120.0f * (1.0f - t)), MaxMapX, MaxMapY, false);
	}
	marks.resize(mkeep);

	// Draw and expire
	size_t keep = 0;
	for (size_t k = 0; k < dust.size(); k++) {
		float t = (now - dust[k].born) / LIFE;
		if (t >= 1.0f || t < 0.0f) continue;
		dust[keep++] = dust[k];
		int sx = (int)(p->Player[me]->X - dust[k].wx) + this->PlayerOffsetX + 24 + dust[k].jx;
		int sy = (int)(p->Player[me]->Y - dust[k].wy) + this->PlayerOffsetY + 32 + dust[k].jy;
		if (dust[k].dark) {
			p->DDraw->DrawGlow(sx, sy - (int)(t * 34.0f) - 6, 6 + (int)(t * 12.0f), 48, 46, 44, (int)(120.0f * (1.0f - t)), MaxMapX, MaxMapY, false);
			continue;
		}
		int radius = 7 + (int)(t * 13.0f);
		int alpha = (int)(120.0f * (1.0f - t));
		p->DDraw->DrawGlow(sx, sy, radius, 190, 170, 135, alpha, MaxMapX, MaxMapY, false);
	}
	dust.resize(keep);
}

/***************************************************************
 * Function:	DrawBullets
 *
 **************************************************************/
void CDrawing::DrawBullets() {
	CBullet *blt;
	int me = p->Winsock->MyIndex;
	int offX = (int)p->Player[me]->X % 48;
	int offY = (int)p->Player[me]->Y % 48;
	int tmpX;
	int tmpY;
	int dX;
	int dY;

	// Draw the tails first so the bullets sit on top
	{
		size_t keep = 0;
		for (size_t k = 0; k < trail.size(); k++) {
			float t = (p->Tick - trail[k].born) / 220.0f;
			if (t >= 1.0f || t < 0.0f) continue;
			trail[keep++] = trail[k];
			int sx = (int)(p->Player[me]->X - trail[k].wx) + this->PlayerOffsetX + 4;
			int sy = (int)(p->Player[me]->Y - trail[k].wy) + this->PlayerOffsetY + 4;
			p->DDraw->DrawGlow(sx, sy, 8 - (int)(t * 5.0f), 255, 150, 50, (int)(120.0f * (1.0f - t)), MaxMapX, MaxMapY);
		}
		trail.resize(keep);
	}

	// For each bullet,
	blt = p->Bullet->bulletListHead;
	while (blt) {
		tmpX = (int)p->Player[me]->X - (int)blt->x + this->PlayerOffsetX;
		tmpY = (int)p->Player[me]->Y - (int)blt->y + this->PlayerOffsetY;

		// If the bullet is in sight,
		if (tmpX > 0 && tmpY > 0 && tmpX < std::max(900, MaxMapX + 76) && tmpY < std::max(900, MaxMapY + 76)) {
			dX = blt->animation * 8;
			dY = dY = blt->type * 8;

			// Cycle through the bullet Animation
			blt->animation++;
			if (blt->animation > 3) {
				blt->animation = 0;
			}

			// A short fading tail behind it
			if (trail.size() < 400) { DustPuff tr = { (float)blt->x, (float)blt->y, p->Tick, 0, 0, 0 }; trail.push_back(tr); }

			// Draw the bullet, with a small warm glow around it
			p->DDraw->DrawGlow(tmpX + 4, tmpY + 4, 16, 255, 190, 90, 150, MaxMapX, MaxMapY);
			p->DDraw->Draw(p->DDraw->imgBullets, tmpX, tmpY, 8, 8, dX, dY, MaxMapX, MaxMapY);
		}

		// Get the next bullet
		blt = blt->next;
	}
}

/***************************************************************
 * Function:	DrawRadar
 *
 **************************************************************/
void CDrawing::DrawRadar() {
	// Your own tile, so the "tile X,Y" in server messages (and /where) can be matched to somewhere
	{
		int me0 = p->Winsock->MyIndex;
		std::ostringstream pos;
		pos << (int)(p->Player[me0]->X / 48) << "," << (int)(p->Player[me0]->Y / 48);
		std::string txt = pos.str();
		p->DDraw->DTextOut(MaxMapX + 99 - p->DDraw->MeasureText(txt) / 2, 122, txt, COLOR_WHITE);
	}
	// A sweeping beam with a fading trail, drawn first so the blips stay on top
	{
		int cx = MaxMapX + 97, cy = 71;
		float base = p->Tick / 1700.0f * 6.2832f;
		for (int tr = 0; tr < 5; tr++) {
			float a = base - tr * 0.16f;
			for (int s = 1; s <= 8; s++) {
				float r = s * 6.2f;
				p->DDraw->DrawGlow(cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r), 4, 90, 255, 150, (int)((90 - tr * 17) * (1.0f - s * 0.04f)), p->ResolutionX, p->ResolutionY);
			}
		}
	}
	int me = p->Winsock->MyIndex;
	int RadarSrcX = 0;
	int RadarX = 0;
	int RadarY = 0;
	int ratio = 24;

	// For each player,
	for (int i = 0; i < MAX_PLAYERS; i++) {

		// If the player is in game, and is not cloaked,
		if ( (p->Player[i]->isInGame) && (p->Player[i]->isCloaked == false) ) {

			// If the player is in range,
			if (
				(p->Player[i]->X >= p->Player[me]->X - RadarSize)
				&&
				(p->Player[i]->X <= p->Player[me]->X + RadarSize)
				&&
				(p->Player[i]->Y >= p->Player[me]->Y - RadarSize)
				&& 
				(p->Player[i]->Y <= p->Player[me]->Y + RadarSize)
				&&
				(p->Player[i]->X != 0)
				&&
				(p->Player[i]->Y != 0)
			) {

				RadarX = (int)((MaxMapX+100)-(p->Player[i]->X - p->Player[me]->X+70)/ratio);
				RadarY = (int)(80-(p->Player[i]->Y - p->Player[me]->Y+69)/ratio);
				
				// If the player is in my city, set color to 3
				if (p->Player[i]->City == p->Player[me]->City) {
					RadarSrcX = 3;
				}

				// Else if player is an admin, set color to 1
				else if (p->Player[i]->isAdmin()) {
					RadarSrcX = 1;
				}
				// Else (player is enemy), set color to 2
				else {
					RadarSrcX = 2;
				}

				// ???
				if (RadarX >= (MaxMapX+28) && RadarX <= (MaxMapX+166) && RadarY >= 8 && RadarY <= 146) {

					// If player is alive, draw the player
					if (p->Player[i]->isDead == false) {
						p->DDraw->Draw(p->DDraw->imgRadarColors, RadarX, RadarY, 2, 2, RadarSrcX * 2);
					}

					// Else (player is dead), ???
					else {
						p->DDraw->Draw(p->DDraw->imgMiniMapColors, RadarX, RadarY, 2, 2, 15);
					}
				}
			}
		}
	}
}

/***************************************************************
 * Function:	DrawExplosions
 *
 **************************************************************/
void CDrawing::DrawExplosions() {
	int me = p->Winsock->MyIndex;
	CExplosion *xpl;
	int tX;
	int tY;
		
	// For each explosion,
	xpl = this->p->Explode->explosionListHead;
	while (xpl) {

		tX = (int)(p->Player[me]->X - xpl->x + this->PlayerOffsetX);
		tY = (int)(p->Player[me]->Y - xpl->y + this->PlayerOffsetY);

		switch(xpl->type) {

			//Small explosion
			case 1: 
				SDL_SetTextureAlphaMod(p->DDraw->imgSExplosion->texture, (Uint8)(xpl->animation <= 5 ? 255 : std::max(40, 255 - (xpl->animation - 5) * 50)));
				if (xpl->animation < 2) p->DDraw->DrawGlow(tX + 24, tY + 24, 40, 255, 255, 235, 170 - xpl->animation * 70, MaxMapX, MaxMapY);
				p->DDraw->DrawGlow(tX + 24, tY + 24, 54 - xpl->animation * 3, 255, 150, 60, 150 - xpl->animation * 13, MaxMapX, MaxMapY);
				p->DDraw->Draw(p->DDraw->imgSExplosion, tX, tY, 48, 48, xpl->animation * 48, 0, MaxMapX, MaxMapY);
				break;

			//Large explosion
			case 2: 
				SDL_SetTextureAlphaMod(p->DDraw->imgLExplosion->texture, (Uint8)(xpl->animation <= 5 ? 255 : std::max(40, 255 - (xpl->animation - 5) * 50)));
				if (xpl->animation < 3) p->DDraw->DrawGlow(tX + 72, tY + 72, 110, 255, 255, 235, 190 - xpl->animation * 60, MaxMapX, MaxMapY);
				p->DDraw->DrawGlow(tX + 72, tY + 72, 150 - xpl->animation * 8, 255, 140, 50, 190 - xpl->animation * 17, MaxMapX, MaxMapY);
				p->DDraw->Draw(p->DDraw->imgLExplosion, tX, tY, 144, 144, 0, xpl->animation * 144, MaxMapX, MaxMapY);
				break;

			//Muzzle flash
			case 3: 
				p->DDraw->DrawGlow(tX + 6, tY + 6, 22, 255, 220, 140, 170 - xpl->animation * 50, MaxMapX, MaxMapY);
				p->DDraw->Draw(p->DDraw->imgMuzzleFlash, tX, tY, 12, 12, xpl->animation * 12, 0, MaxMapX, MaxMapY);
				break;
		}

		// A new blast throws out a burst of embers
		if (!xpl->sparked && (xpl->type == 1 || xpl->type == 2)) {
			xpl->sparked = true;
			if (xpl->type == 2) {
				float ddx = p->Player[me]->X - xpl->x, ddy = p->Player[me]->Y - xpl->y;
				float dist = sqrtf(ddx * ddx + ddy * ddy);
				if (dist < 500.0f) AddShake(7.0f * (1.0f - dist / 500.0f));
				Ring rg = { (float)xpl->x, (float)xpl->y, p->Tick, 650.0f, 72, 72, 190, 255, 190, 110, 200 };
				if (rings.size() < 40) rings.push_back(rg);
				DustPuff sc = { (float)xpl->x, (float)xpl->y, p->Tick, 0, 0, 0 };
				if (scorch.size() < 60) scorch.push_back(sc);
			}
			for (int k = 0; k < (xpl->type == 2 ? 7 : 2) && smoke.size() < 120; k++) {
				int sp = xpl->type == 2 ? 46 : 18;
				DustPuff sm = { (float)xpl->x, (float)xpl->y, p->Tick + 150.0f + rand() % 350, (xpl->type == 2 ? 72 : 24) + rand() % (2 * sp) - sp, (xpl->type == 2 ? 72 : 24) + rand() % (2 * sp) - sp, 0 };
				smoke.push_back(sm);
			}
			int n = xpl->type == 2 ? 24 : 9;
			int c = xpl->type == 2 ? 72 : 24;
			float speed = xpl->type == 2 ? 95.0f : 55.0f;
			for (int k = 0; k < n && sparks.size() < 300; k++) {
				float a = (rand() % 628) / 100.0f, s = speed * (0.4f + (rand() % 100) / 100.0f);
				Spark sp = { (float)xpl->x, (float)xpl->y, p->Tick, 450.0f + rand() % 350, c, c, cosf(a) * s, sinf(a) * s };
				sparks.push_back(sp);
			}
		}

		// Get the next explosion
		xpl = xpl->next;
	}

	// Smoke clouds billow up from the blast and drift away
	{
		size_t sk = 0;
		for (size_t k = 0; k < smoke.size(); k++) {
			float t = (p->Tick - smoke[k].born) / 1700.0f;
			if (t >= 1.0f) continue;
			smoke[sk++] = smoke[k];
			if (t < 0) continue;
			int sx = (int)(p->Player[me]->X - smoke[k].wx) + this->PlayerOffsetX + smoke[k].jx + (int)(t * 12.0f);
			int sy = (int)(p->Player[me]->Y - smoke[k].wy) + this->PlayerOffsetY + smoke[k].jy - (int)(t * 34.0f);
			float fadeIn = std::min(1.0f, t * 7.0f);
			p->DDraw->DrawGlow(sx, sy, 14 + (int)(t * 24.0f), 64, 60, 56, (int)(120.0f * (1.0f - t) * fadeIn), MaxMapX, MaxMapY, false);
		}
		smoke.resize(sk);
	}

	// Shockwave rings
	{
		size_t rk = 0;
		for (size_t k = 0; k < rings.size(); k++) {
			float t = (p->Tick - rings[k].born) / rings[k].life;
			if (t >= 1.0f || t < 0.0f) continue;
			rings[rk++] = rings[k];
			int sx = (int)(p->Player[me]->X - rings[k].wx) + this->PlayerOffsetX + rings[k].ox;
			int sy = (int)(p->Player[me]->Y - rings[k].wy) + this->PlayerOffsetY + rings[k].oy;
			float e = 1.0f - (1.0f - t) * (1.0f - t);   // decelerating growth
			p->DDraw->DrawRing(sx, sy, 8 + (int)(e * rings[k].maxr), rings[k].r, rings[k].g, rings[k].b, (int)(rings[k].a * (1.0f - t)), MaxMapX, MaxMapY);
		}
		rings.resize(rk);
	}

	// Draw and expire the embers: they slow down, shrink and cool from yellow to red
	size_t keep = 0;
	for (size_t k = 0; k < sparks.size(); k++) {
		float t = (p->Tick - sparks[k].born) / sparks[k].life;
		if (t >= 1.0f || t < 0.0f) continue;
		sparks[keep++] = sparks[k];
		float d = t * (2.0f - t) * 0.9f;   // eased distance travelled, in seconds of initial speed
		int sx = (int)(p->Player[me]->X - sparks[k].wx) + this->PlayerOffsetX + sparks[k].ox + (int)(sparks[k].vx * d);
		int sy = (int)(p->Player[me]->Y - sparks[k].wy) + this->PlayerOffsetY + sparks[k].oy + (int)(sparks[k].vy * d);
		int g = (int)(220.0f - 150.0f * t);
		p->DDraw->DrawGlow(sx, sy, 5 - (int)(t * 3.0f), 255, g, 40, (int)(255.0f * (1.0f - t)), MaxMapX, MaxMapY);
	}
	sparks.resize(keep);
}

/***************************************************************
 * Function:	DrawWeather
 *
 * Every five minutes of play has a chance of a spell of rain or a dust storm. The schedule comes from the clock alone, so
 * it needs no server support. BC_WEATHER=rain|dust|off forces or disables it (tests).
 **************************************************************/
void CDrawing::DrawWeather() {
	static int forced = -1;
	if (forced == -1) {
		const char *w = getenv("BC_WEATHER");
		forced = !w ? 0 : (!strcmp(w, "rain") ? 1 : (!strcmp(w, "dust") ? 2 : (!strcmp(w, "off") ? 3 : 0)));
	}
	if (forced == 3) return;
	const float PERIOD = 300000.0f;
	float now = p->Tick;
	int period = (int)(now / PERIOD);
	unsigned int hs = (unsigned int)(period * 2654435761u) ^ 0x9e3779b9u;
	hs ^= hs >> 15; hs *= 0x2c1b3c6du; hs ^= hs >> 12;
	int kind = forced ? forced : ((hs % 10) < 4 ? 0 : ((hs % 10) < 8 ? 1 : 2));    // 40% clear, 40% rain, 20% dust
	float start = 25000.0f + (hs >> 8) % 90000, dur = 70000.0f + (hs >> 16) % 80000;
	float t = fmodf(now, PERIOD);
	float intensity = 0;
	if (forced) intensity = 1.0f;
	else if (kind != 0 && t > start && t < start + dur) {
		float ramp = 9000.0f;
		intensity = std::min(1.0f, std::min((t - start) / ramp, (start + dur - t) / ramp));
	}
	if (intensity <= 0.01f) return;

	std::vector<int> lines;
	if (kind == 1) {
		// rain: slanted streaks falling at slightly different speeds, with a cool wash over everything
		p->DDraw->DrawOverlay(MaxMapX, MaxMapY, 10, 22, 44, (int)(34 * intensity));
		int n = (int)(300 * intensity);
		for (int i = 0; i < n; i++) {
			unsigned int hi = (unsigned int)(i + 1) * 2654435761u;
			float sp = 0.55f + ((hi >> 4) % 50) / 100.0f;
			int x = (int)fmodf((float)((hi >> 8) % MaxMapX) - now * 0.10f * sp + 100000.0f, (float)MaxMapX);
			int y = (int)fmodf((float)((hi >> 16) % MaxMapY) + now * 0.62f * sp, (float)MaxMapY);
			int x1 = x, y1 = y, x2 = x - 5, y2 = y + 17;
			if (x2 < 0 || y2 >= MaxMapY) continue;
			lines.push_back(x1); lines.push_back(y1); lines.push_back(x2); lines.push_back(y2);
		}
		p->DDraw->DrawStreaks(lines, 200, 222, 250, (int)(150 * intensity));
		// splashes where drops land
		for (int i = 0; i < (int)(26 * intensity); i++) {
			unsigned int hi = (unsigned int)((i + 1) * 40503u) ^ (unsigned int)((int)(now / 180.0f) * 2654435761u);
			hi ^= hi >> 13; hi *= 0x5bd1e995u;
			p->DDraw->DrawGlow((int)((hi >> 3) % MaxMapX), (int)((hi >> 14) % MaxMapY), 3, 200, 220, 240, 70, MaxMapX, MaxMapY);
		}
		// now and then, lightning
		float ph = fmodf(now, 23000.0f);
		if (intensity > 0.8f && ph < 110.0f) p->DDraw->DrawOverlay(MaxMapX, MaxMapY, 235, 240, 255, (int)(90 * (1.0f - ph / 110.0f)));
	}
	else {
		// dust storm: a tan haze and long streaks blowing across
		p->DDraw->DrawOverlay(MaxMapX, MaxMapY, 160, 118, 66, (int)(78 * intensity));
		int n = (int)(140 * intensity);
		for (int i = 0; i < n; i++) {
			unsigned int hi = (unsigned int)(i + 1) * 2246822519u;
			float sp = 0.8f + ((hi >> 4) % 60) / 100.0f;
			int x = (int)fmodf((float)((hi >> 8) % (MaxMapX + 120)) + now * 0.55f * sp, (float)(MaxMapX + 120)) - 60;
			int y = (int)((hi >> 16) % MaxMapY) + (int)(3.0f * sinf(now / 600.0f + i));
			int len = 26 + (int)((hi >> 3) % 34);
			int x1 = x, x2 = x + len;
			if (x1 < 0 || x2 >= MaxMapX || y < 0 || y >= MaxMapY) continue;
			lines.push_back(x1); lines.push_back(y); lines.push_back(x2); lines.push_back(y + 1);
		}
		p->DDraw->DrawStreaks(lines, 236, 206, 160, (int)(130 * intensity));
		p->DDraw->DrawVignette(MaxMapX, MaxMapY, (int)(70 * intensity), 120, 85, 40);
	}
}

/***************************************************************
 * Function:	DrawMiniMap
 *
 **************************************************************/
void CDrawing::DrawMiniMap() {
	CBuilding *bld;
	int me = p->Winsock->MyIndex;
	int myX = (int)p->Player[me]->X / 48;
	int myY = (int)p->Player[me]->Y / 48;
	int tX;
	int tY;
	int X;
	int Y;

	int radius = 48;
	int tilesize = 5;

	Rect build_rect = {0,0,3*tilesize,3*tilesize};
	Rect plr_rect = {242,242,tilesize,tilesize};
	Rect terrain_rect = {0,0,tilesize,tilesize};
	char DrawX = 0;

	p->DDraw->DFillRect(10, 10, 460, 460);

	/************************************************
	 * Terrain
	 ************************************************/
	// For each square within {radius} of my position
	for (Y = myY-radius; Y <= myY+radius; Y++) {
		for (X = myX-radius; X <= myX+radius; X++) {

			// If the square is on the map,
			if (X > 0 && Y > 0 && X < 512 && Y < 512) {

				// Square: LAVA
				if (p->Map->map[X][Y] == MAP_SQUARE_LAVA) {
					DrawX = 0;
				}

				// Square: LAVA
				else if (p->Map->map[X][Y] == MAP_SQUARE_ROCK) {
					DrawX = 1;
				}

				// Square: OTHER
				else {
					DrawX = -1;
				}

				// If LAVA or ROCK,
				if (DrawX != -1) {
					terrain_rect.X = this->PlayerOffsetX+((myX-X-this->minimapOffsetX)*48)/(tilesize*2);
					terrain_rect.Y = this->PlayerOffsetY+((myY-Y-this->minimapOffsetY)*48)/(tilesize*2);

					// Check some constraints on the position of the square... ???
					if (terrain_rect.X > 10 && terrain_rect.X < 464 && terrain_rect.Y > 10 && terrain_rect.Y < 464) {

						// Draw the color on the mini map
						p->DDraw->Draw(p->DDraw->imgMiniMapColors, terrain_rect.X, terrain_rect.Y, terrain_rect.w, terrain_rect.h, DrawX*15);
					}
				}
			}
		}
	}

	/************************************************
	 * Buildings
	 ************************************************/
	// For each building,
	bld = p->Build->buildingListHead;
	while (bld) {

		tX = (myX - bld->X - this->minimapOffsetX) * 48;
		tY = (myY - bld->Y - this->minimapOffsetY) * 48;
		build_rect.X = this->PlayerOffsetX + (tX) / (2*tilesize);
		build_rect.Y = this->PlayerOffsetY + (tY) / (2*tilesize);

		if (build_rect.X > 10 && build_rect.X < 464 && build_rect.Y > 10 && build_rect.Y < 464) {

			// If the building is my CC, draw 2
			if ((bld->City == this->p->Player[me]->City) && (bld->isCC())) {
				DrawX = 2; 
			}
			// Else (not my CC), draw 3
			else {
				DrawX = 3;
			}
			this->p->DDraw->Draw(this->p->DDraw->imgMiniMapColors, build_rect.X, build_rect.Y, build_rect.w, build_rect.h, DrawX*15);
		}

		// Get the next building
		bld = bld->next;
	}

	// Draw me
	DrawX = 2;
	this->p->DDraw->Draw(this->p->DDraw->imgMiniMapColors, plr_rect.X, plr_rect.Y, plr_rect.w, plr_rect.h, DrawX*15);
}

/***************************************************************
 * Function:	CashFormat
 *
 **************************************************************/
string CDrawing::CashFormat(long Number) {
	char strCash[20];
	char output[20];
	memset(output, 0, 20);
	itoa(Number, strCash, 10);
	strcpy(output, "$");

	switch (strlen(strCash)) {

		case 4:
			memcpy(&output[1], strCash, 1);
			memcpy(&output[2], ",", 1);
			memcpy(&output[3], &strCash[2], 3);
			break;

		case 5:
			memcpy(&output[1], strCash, 2);
			memcpy(&output[3], ",", 1);
			memcpy(&output[4], &strCash[2], 3);
			break;

		case 6:
			memcpy(&output[1], strCash, 3);
			memcpy(&output[4], ",", 1);
			memcpy(&output[5], &strCash[3], 3);
			break;

		case 7:
			memcpy(&output[1], strCash, 1);
			memcpy(&output[2], ",", 1);
			memcpy(&output[3], &strCash[1], 3);
			memcpy(&output[6], ",", 1);
			memcpy(&output[7], &strCash[4], 3);
			break;

		case 8:
			memcpy(&output[1], strCash, 2);
			memcpy(&output[3], ",", 1);
			memcpy(&output[4], &strCash[2], 3);
			memcpy(&output[7], ",", 1);
			memcpy(&output[8], &strCash[5], 3);
			break;

		case 1:

		case 2:

		case 3:

		default:
			strcpy(&output[strlen(output)], strCash);
			break;
	}

	string Final;
	Final = output;
	return Final;
}

/***************************************************************
 * Function:	ClearPanel
 *
 **************************************************************/
void CDrawing::ClearPanel() {
	PanelLine1 = "";
	PanelLine2 = "";
	PanelLine3 = "";
	PanelLine4 = "";
	PanelLine5 = "";
	PanelLine6 = "";
	PanelLine7 = "";
	PanelMode = modeEmpty;
}

/***************************************************************
 * Function:	resetPlayerOffset
 *
 **************************************************************/
void CDrawing::resetPlayerOffset() {
	this->PlayerOffsetX = this->defaultPlayerOffsetX;
	this->PlayerOffsetY = this->defaultPlayerOffsetY;
}

/***************************************************************
 * Function:	getOffsetX
 *
 * @param x
 **************************************************************/
int CDrawing::getOffsetX(int x) {
	return x + this->PlayerOffsetX;
}

/***************************************************************
 * Function:	getOffsetY
 *
 * @param y
 **************************************************************/
int CDrawing::getOffsetY(int y) {
	return y + this->PlayerOffsetY;
}

/***************************************************************
 * Function:	isPositionInSight
 *
 * @param myX
 * @param myY
 * @param positionX
 * @param positionY
 **************************************************************/
bool CDrawing::isPositionInSight(int myX, int myY, int positionX, int positionY) {
	return this->isTileInSight(myX, myY, positionX/48, positionY/48);
}

/***************************************************************
 * Function:	isTileInSight
 *
 * @param myX
 * @param myY
 * @param tileX
 * @param tileY
 **************************************************************/
bool CDrawing::isTileInSight(int myX, int myY, int tileX, int tileY) {
	// How far the view reaches: the original 18-tile limit was sized for the 800/1024 pixel view; a widescreen view is wider,
	// and anything beyond the limit used to vanish (and pop back in as you drove)
	int reachX = std::max(this->tilesToDraw, this->MaxMapX / 48 + 3);
	int reachY = std::max(this->tilesToDraw, this->MaxMapY / 48 + 3);
	int dx = (myX / 48) - tileX, dy = (myY / 48) - tileY;   // positive: towards the right/bottom of the screen (the view is mirrored)
	return dx > -this->tilesToDraw && dx < reachX && dy > -this->tilesToDraw && dy < reachY;
}

/***************************************************************
 * Function:	getPlayerTankImage
 *
 * @param playerIndex
 **************************************************************/
int CDrawing::getPlayerTankImage(CPlayer *player, CPlayer *me) {

	// If the player is alive, return -1 (none)
	if (player->isDead) {
		return -1;
	}

	// If the player has a custom tank, return it
	else if (player->Tank > 0) {
		return player->Tank;
	}

	// Else if the player is in my city
	else if (player->City == me->City) {

		// If Mayor, set friend mayor tank
		if (player->isMayor) {
			return 1;
		}

		// Else (not mayor), set friend commando tank
		else {
			return 0;
		}
	}

	// Else (player not in my city),
	else {
	
		// If mayor, set enemy mayor tank
		if (player->isMayor) {
			return 3;
		}

		// Else (not mayor), set enemy commando tank
		else {
			return 2;
		}
	}

	// Return -1 (none)
	return -1;
}


/***************************************************************
 * Function:	getPlayerNameColor
 *
 * @param playerIndex
 * @param myIndex
 **************************************************************/
COLORREF CDrawing::getPlayerNameColor(CPlayer *player, CPlayer *me) {

	// Color: DEAD
	if (player->isDead) {
		return COLOR_DEAD_NAME;
	}

	// Color: ADMIN
	else if (player->isAdmin()) {
		return COLOR_ADMIN_NAME;
	}

	// Color: LAGGING
	else if (player->isLagging) {
		return COLOR_LAGGING_NAME;
	}

	// Color: CUSTOM
	else if ((player->Red > 0) || (player->Green > 0) || (player->Blue > 0)) {
		
		// Color: RAINBOW
		if (player->RainbowName == 1) {
			return RGB(rand()%255, rand()%255, rand()%255);
		}

		// Color: CUSTOM
		else {
			return RGB(player->Red, player->Green, player->Blue);
		}
	}

	// Color: TEAM
	else if (player->City == me->City) {
		return COLOR_TEAM_NAME;
	}

	// Color: RED
	else {
		return COLOR_ENEMY_NAME;
	}
}

/***************************************************************
 * Function:	getPlayerCityColor
 *
 * @param playerIndex
 * @param myIndex
 **************************************************************/
COLORREF CDrawing::getPlayerCityColor(CPlayer *player, CPlayer *me) {

	// Color: DEAD
	if (player->isDead) {
		return COLOR_DEAD_NAME;
	}

	// Color: ADMIN
	else if (player->isAdmin()) {
		return COLOR_ADMIN_NAME;
	}

	// Color: LAGGING
	else if (player->isLagging) {
		return COLOR_LAGGING_NAME;
	}

	// Color: TEAM
	else if (player->City == me->City) {
		return COLOR_TEAM_NAME;
	}

	// Color: RED
	else {
		return COLOR_ENEMY_NAME;
	}
}

/***************************************************************
 * Function:	drawPlayerName
 *
 * @param player
 * @param me
 **************************************************************/
void CDrawing::drawPlayerName(CPlayer *player, CPlayer *me) {
	int nameWidth;
	int townWidth;
	int centerPoint;

	// Get the player's position relative to my position
	int relativeX = this->getOffsetX(me->X) - player->X;
	int relativeY = this->getOffsetY(me->Y) - player->Y;

	// Get text to show
	string nameString = player->NameString;
	string townString = player->TownString;

	// If alive and lagging, add Lagging
	if ((player->isDead == 0) && player->isLagging) {
		nameString += " (Lagging)";
	}

	// Get text lengths
	nameWidth = p->DDraw->MeasureText(nameString) / 2;
	if (player->Member) {
		nameWidth -= 8;
	}
	townWidth = p->DDraw->MeasureText(townString) / 2;
	centerPoint = relativeX + 16 + 3;

	// A soft dark backdrop keeps the labels readable on any ground
	p->DDraw->DrawShadow(centerPoint, relativeY - 16, std::max(nameWidth, townWidth) * 2 + 40, 40, 110, MaxMapX, MaxMapY);

	// Draw text
	p->DDraw->DTextOut(centerPoint - nameWidth, relativeY - 31, nameString, this->getPlayerNameColor(player, me));
	p->DDraw->DTextOut(centerPoint - townWidth, relativeY - 16, townString, this->getPlayerCityColor(player, me));

	// If the player is a member, draw a star
	if (player->Member) {
		p->DDraw->Draw(p->DDraw->imgStar, centerPoint - nameWidth - 17, relativeY - 31, 16, 16);
	}
}

/***************************************************************
 * Function:	DrawItemHint
 *
 * When the tank is on top of an item, says how to pick it up and what it does
 * once it is in the inventory.
 **************************************************************/
void CDrawing::DrawItemHint() {
	CPlayer *me = p->Player[p->Winsock->MyIndex];
	if (!me || !me->isInGame || me->isDead) return;
	CItem *item = p->Item->findItembyLocation(((int)me->X + 24) / 48, ((int)me->Y + 24) / 48);
	if (!item) return;
	static const char *names[] = { "Cloak", "Cougar Missiles", "Medkit", "Bomb", "Mine", "Orb", "Flares", "DFG", "Wall", "Turret", "Sleeper", "Plasma" };
	static const char *use[] = { "press C to cloak", "fire while standing still", "press H to heal", "press B to drop it", "drops hidden",
		"press O on an enemy Command Center", "hold CTRL to fire", "drops hidden", "drops as a wall", "press D to drop it", "drops hidden", "drops as a turret" };
	int t = item->Type;
	if (t < 0 || t > 11) return;
	string line1 = string(names[t]) + ": press U to pick it up";
	string line2 = string("Then ") + use[t] + " (see the Inventory box)";
	int cx = PlayerOffsetX + 24;
	int w1 = p->DDraw->MeasureText(line1), w2 = p->DDraw->MeasureText(line2);
	int w = (w1 > w2 ? w1 : w2) + 20;
	int x = cx - w / 2, y = PlayerOffsetY - 84;   // clear of the name and city lines above the tank
	if (x < 4) x = 4;
	if (x + w > MaxMapX - 4) x = MaxMapX - 4 - w;
	if (y < 60) y = PlayerOffsetY + 56;
	p->DDraw->DFillRectColor(x, y, w, 36, 0, 0, 0);
	p->DDraw->DTextOut(x + 6, y + 2, line1, COLOR_YELLOW);
	p->DDraw->DTextOut(x + 6, y + 18, line2, COLOR_WHITE);
}


/***************************************************************
 * Function:	DrawVoiceHud
 *
 * A small button under the info text: a microphone icon and a short label (grey when voice is off, green when it is on,
 * red while you are transmitting). Pointing at it shows how to use it; a click opens the voice settings. Players who are
 * talking are listed under it with a speaker icon.
 **************************************************************/
static void VoiceMicIcon(CDDraw *d, int ix, int iy, int r, int g, int b) {
	d->DFillRectColor(ix + 4, iy, 4, 1, r, g, b);               // the capsule
	d->DFillRectColor(ix + 3, iy + 1, 6, 8, r, g, b);
	d->DFillRectColor(ix + 4, iy + 9, 4, 1, r, g, b);
	d->DFillRectColor(ix + 1, iy + 6, 1, 4, r, g, b);           // the holder around it
	d->DFillRectColor(ix + 10, iy + 6, 1, 4, r, g, b);
	d->DFillRectColor(ix + 2, iy + 10, 1, 1, r, g, b);
	d->DFillRectColor(ix + 9, iy + 10, 1, 1, r, g, b);
	d->DFillRectColor(ix + 3, iy + 11, 6, 1, r, g, b);
	d->DFillRectColor(ix + 5, iy + 12, 2, 2, r, g, b);          // the stem and base
	d->DFillRectColor(ix + 3, iy + 14, 6, 1, r, g, b);
}

static void VoiceSpeakerIcon(CDDraw *d, int x, int y, int r, int g, int b) {
	d->DFillRectColor(x, y + 3, 3, 4, r, g, b);
	d->DFillRectColor(x + 3, y + 2, 1, 6, r, g, b);
	d->DFillRectColor(x + 4, y + 1, 1, 8, r, g, b);
	d->DFillRectColor(x + 6, y + 3, 1, 4, r, g, b);             // sound waves
	d->DFillRectColor(x + 8, y + 2, 1, 6, r, g, b);
}

void CDrawing::DrawVoiceHud() {
	CVoice *v = p->Voice;
	if (!v || !v->serverEnabled) return;
	CDDraw *d = p->DDraw;
	const int x = 4, y = 62, w = 92, h = 22;
	bool talking = v->on && v->Transmitting();
	int pulse = (int)(p->Tick / 250) & 1;
	int br, bg, bb, ir, ig, ib;        // background and icon colours
	string label;
	if (!v->on) {
		br = 30; bg = 34; bb = 44; ir = 160; ig = 165; ib = 175; label = "Voice";
	} else if (talking) {
		br = pulse ? 150 : 120; bg = 20; bb = 28; ir = 255; ig = 235; ib = 235; label = "Talking";
	} else if (!v->status.empty()) {
		br = 90; bg = 70; bb = 10; ir = 255; ig = 220; ib = 90; label = "Mic?";
	} else {
		br = 16; bg = 78; bb = 44; ir = 150; ig = 255; ib = 170; label = (v->mode == 0) ? "Hold `" : "Voice on";
	}
	// the button: a pill (corners cut), a lighter edge and the icon
	d->DFillRectAlpha(x + 2, y, w - 4, h, br, bg, bb, 210);
	d->DFillRectAlpha(x, y + 2, w, h - 4, br, bg, bb, 210);
	d->DFillRectAlpha(x + 1, y + 1, w - 2, h - 2, br, bg, bb, 210);
	d->DFillRectAlpha(x + 2, y, w - 4, 1, ir, ig, ib, 150);
	d->DFillRectAlpha(x + 2, y + h - 1, w - 4, 1, ir, ig, ib, 150);
	d->DFillRectAlpha(x, y + 2, 1, h - 4, ir, ig, ib, 150);
	d->DFillRectAlpha(x + w - 1, y + 2, 1, h - 4, ir, ig, ib, 150);
	VoiceMicIcon(d, x + 8, y + 3, ir, ig, ib);
	if (!v->on) {
		for (int k = 0; k < 14; k++) d->DFillRectColor(x + 7 + k, y + 18 - k, 2, 2, 235, 70, 70);    // a slash through it
	}
	d->DTextOut(x + 26, y + 3, label, COLOR_WHITE);
	if (v->on && v->mode == 1 && !talking) {
		int lw = (int)(v->Level() * 500.0f);                    // open mic: how loud you are, against the level that opens it
		if (lw > 60) lw = 60;
		if (lw > 1) d->DFillRectColor(x + 26, y + h - 4, lw, 2, 120, 255, 150);
	}

	// pointing at it says what it does; a mic problem is always shown
	int mx = p->Input->LastMouseX, my = p->Input->LastMouseY;
	bool hover = mx >= x && mx < x + w && my >= y && my < y + h;
	string tip;
	if (!v->status.empty()) tip = v->status;
	else if (hover) {
		if (!v->on) tip = "Voice chat is off. Click for settings, or press V to turn it on.";
		else if (v->mode == 0) tip = "Hold ` to talk to " + string(CVoice::ScopeName(v->scope)) + ". Click for settings, V turns it off.";
		else tip = "Open mic: you are heard when you speak (" + string(CVoice::ScopeName(v->scope)) + "). Click for settings, V turns it off.";
	}
	int row = 0;
	if (!tip.empty()) {
		d->DFillRectAlpha(0, y + h + 3, d->MeasureText(tip) + 10, 17, 0, 0, 0, 150);
		d->DTextOut(4, y + h + 4, tip, v->status.empty() ? COLOR_WHITE : COLOR_YELLOW);
		row = 1;
	}
	for (int i = 0; i < MAX_PLAYERS && row < 6; i++) {
		if (!v->Speaking(i)) continue;
		string who = p->Player[i]->Name;
		int ry = y + h + 4 + 18 * row;
		d->DFillRectAlpha(0, ry - 1, d->MeasureText(who) + 28, 17, 0, 0, 0, 150);
		VoiceSpeakerIcon(d, 4, ry + 2, 120, 255, 140);
		d->DTextOut(18, ry, who, RGB(150,255,170));
		row++;
	}
}
