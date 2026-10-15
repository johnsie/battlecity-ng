/*
===============================================================================

    Battle City - CDDraw header file
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
#ifndef _CDDRAW
#define _CDDRAW

#include "CGame.h"
#include <SDL.h>

/// <summary> A loaded bitmap: a texture plus its size. </summary>
struct BCImage {
    SDL_Texture *texture;
    int width;
    int height;
};

/// <summary>
/// 2D renderer. Historically DirectDraw (hence the name); now backed by SDL2 so the same
/// drawing code runs on Windows, Linux and the web.
/// </summary>
class CDDraw
{
public:
    void InitWindowed(HWND hWnd);
    void InitFullscreen(HWND hWnd);
    void LoadSurfaces();
    void DestroySurfaces();

    void Clear();
    void Flip();
    void Draw(BCImage* Surface, int X, int Y, int width, int height, int _sX = 0, int _sY = 0, int _clipwidth = -1, int _clipheight = -1);
    void DTextOut(int X, int Y, string text, COLORREF ColorVal);
    void DFillRect(int X, int Y, int width, int height);

    /// <summary> Convert window pixel coordinates to game coordinates (the game is drawn at a fixed logical size). </summary>
    void WindowToGame(int wx, int wy, int *gx, int *gy);
    /// <summary> Save the current frame as a BMP (used by tests and screenshots). </summary>
    bool SaveScreenshot(const char *path);

    SDL_Renderer *Renderer;

    BCImage *imgArrowsRed;
    BCImage *imgArrows;
    BCImage *imgBCLogo;
    BCImage *imgBlackNumbers;
    BCImage *imgBtnStaff;
    BCImage *imgBuildIcons;
    BCImage *imgBuildings;
    BCImage *imgBullets;
    BCImage *imgCompany;
    BCImage *imgCursor;
    BCImage *imgDemolish;
    BCImage *imgGround;
    BCImage *imgHealth;
    BCImage *imgInterface;
    BCImage *imgInterfaceBottom;
    BCImage *imgInventorySelection;
    BCImage *imgItems;
    BCImage *imgLava;
    BCImage *imgLoading;
    BCImage *imgLExplosion;
    BCImage *imgMiniMapColors;
    BCImage *imgMoneyBox;
    BCImage *imgMoneyDown;
    BCImage *imgMoneyUp;
    BCImage *imgMuzzleFlash;
    BCImage *imgPopulation;
    BCImage *imgRadarColors;
    BCImage *imgResearch;
    BCImage *imgResearchComplete;
    BCImage *imgRocks;
    BCImage *imgSExplosion;
    BCImage *imgSmoke;
    BCImage *imgStar;
    BCImage *imgTanks;
    BCImage *imgTurretBase;
    BCImage *imgTurretHead;

    CDDraw(CGame *game);
    ~CDDraw();

    char FullScreen;

    // Kept from the DirectDraw days (window frame offsets); always 0 with SDL.
    int XOff;
    int YOff;

    int TextWidth;

private:
    BCImage *LoadImage(const char *file, bool useColorKey, COLORREF colorKey);
    void FreeImage(BCImage *&img);
    void CreateRenderer(HWND hWnd);

    SDL_Window *Window;
    BCImage *Font;

    CGame *p;
protected:
};

#endif
