/*
===============================================================================

    Battle City - CDrawing header file
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
#ifndef _CDrawing
#define _CDrawing

#include <vector>
#include "CGame.h"

class CGame;

class CDrawing {

    public:
        CDrawing(CGame *game);
        ~CDrawing();

        void DrawGame();
        void ClearDrawing();
        void ApplyResolution();   // the game's resolution changed (window reshaped): re-centre the view

        void DrawMap();
        void DrawPanel();
        void DrawTank();
        void DrawBuildings();
        void DrawPlayers();
        void DrawBuildMenu();
        void DrawItemHint();
        void DrawVoiceHud();
        void DrawWaypoints();
        void SetWaypoints(const char *data, int length);   // smWaypoints
        bool ShowWaypoints;                                  // N turns the pointers on and off
        struct WaypointItem { int type; int x, y; };
        WaypointItem Waypoint[8];
        int WaypointCount;
        void SetLeaderboard(const char *data, int length);   // smLeaderboard: lines for the block below the side panel
        vector<string> BoardLine;
        unsigned long BoardTime;
        unsigned long WaypointTime;
        void DrawBuildPlacement();
        void DrawArrow();
        void DrawHealth();
        void DrawItems();
        void DrawChat();
        void DrawInfo();
        void DrawInventory();
        void DrawBullets();
        int refOffsetX();   // where the tank sat on screen in the original 1024x768 / 800x600 views (the click offsets were tuned for that)
        int refOffsetY();
        static int floorDiv(int a, int b) { int q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; }
        void DrawDust();   // dust puffs kicked up by moving tanks
        void DrawRadar();
        void DrawExplosions();
        void DrawWeather();   // rain and dust storms that roll through now and then
        void DrawMiniMap();

        string CashFormat(long Number);

        int defaultPlayerOffsetX;
        int defaultPlayerOffsetY;
        int PlayerOffsetX;
        int PlayerOffsetY;
        int MaxMapX;
        int MaxMapY;
        int groundTilesToDraw;
        int tilesToDraw;
        int buildingCursorOffsetX;
        int buildingCursorOffsetY;
        int demoCursorOffsetX;
        int demoCursorOffsetY;
        int minimapOffsetX;
        int minimapOffsetY;
        int clickOffsetX;
        int clickOffsetY;
        int chatBarWidth;

        struct DustPuff { float wx, wy, born; int jx, jy; int dark; };   // dark = smoke from a damaged tank
        std::vector<DustPuff> dust;
        struct Spark { float wx, wy, born, life; int ox, oy; float vx, vy; };
        std::vector<Spark> sparks;   // glowing embers thrown out by explosions
        std::vector<DustPuff> marks;   // tread marks left on the ground (long-lived, faint)
        float lastMarkX[MAX_PLAYERS], lastMarkY[MAX_PLAYERS];
        struct Ring { float wx, wy, born, life; int ox, oy, maxr, r, g, b, a; };
        std::vector<Ring> rings;        // expanding shockwaves
        std::vector<DustPuff> scorch;   // blast scorch marks on the ground
        std::vector<DustPuff> smoke;   // smoke clouds left by blasts
        std::vector<DustPuff> trail;   // short fading tails behind bullets
        float shakeAmp, lastFrameTick, fadeStart;
        bool wasDead;
        void AddShake(float amp) { if (amp > shakeAmp) shakeAmp = amp; }
        float lastTankX[MAX_PLAYERS], lastTankY[MAX_PLAYERS], lastPuff[MAX_PLAYERS];

        float arrowTick;
        float arrowFlashTick;
        int lastArrow;
        bool flashArrow;

        int BuildMenuX;
        int BuildMenuY;

        string PanelLine1;
        string PanelLine2;
        string PanelLine3;
        string PanelLine4;
        string PanelLine5;
        string PanelLine6;
        string PanelLine7;
        int PanelMode;
        void ClearPanel();
        void resetPlayerOffset();

        int getOffsetX(int x);
        int getOffsetY(int y);
        bool isPositionInSight(int myX, int myY, int positionX, int positionY);
        bool isTileInSight(int myX, int myY, int locationX, int locationY);
        COLORREF getPlayerCityColor(CPlayer *player, CPlayer *me);
        COLORREF getPlayerNameColor(CPlayer *player, CPlayer *me);
        int getPlayerTankImage(CPlayer *player, CPlayer *me);
        void drawPlayerName(CPlayer *player, CPlayer *me);

    private:
        CGame *p;

    protected:

};

#endif