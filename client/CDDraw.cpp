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
#include "CDDraw.h"

CDDraw::CDDraw(CGame *game)
{
    FullScreen = 0;
    XOff = 0;
    YOff = 0;
    TextWidth = 8;
    Renderer = NULL;
    Window = NULL;
    lastOutputW = lastOutputH = 0;
    DrawCalls = LastFrameDrawCalls = 0;
    Font = NULL;
    imgArrowsRed = NULL;
    imgArrows = NULL;
    imgBCLogo = NULL;
    imgBlackNumbers = NULL;
    imgBtnStaff = NULL;
    imgBuildIcons = NULL;
    imgBuildings = NULL;
    imgBullets = NULL;
    imgCompany = NULL;
    imgCursor = NULL;
    imgDemolish = NULL;
    imgGround = NULL;
    imgHealth = NULL;
    imgInterface = NULL;
    imgInterfaceBottom = NULL;
    imgInventorySelection = NULL;
    imgItems = NULL;
    imgLava = NULL;
    imgLoading = NULL;
    imgLExplosion = NULL;
    imgMiniMapColors = NULL;
    imgMoneyBox = NULL;
    imgMoneyDown = NULL;
    imgMoneyUp = NULL;
    imgMuzzleFlash = NULL;
    imgPopulation = NULL;
    imgRadarColors = NULL;
    imgResearch = NULL;
    imgResearchComplete = NULL;
    imgRocks = NULL;
    imgSExplosion = NULL;
    imgSmoke = NULL;
    imgStar = NULL;
    imgTanks = NULL;
    imgTurretBase = NULL;
    imgTurretHead = NULL;
    p = game;
}

CDDraw::~CDDraw()
{
    DestroySurfaces();
}

void CDDraw::CreateRenderer(HWND hWnd)
{
    Window = (SDL_Window *)hWnd;
#ifdef __EMSCRIPTEN__
    // requestAnimationFrame paces the browser build; asking SDL for vsync here would configure a main loop that does not exist yet
    Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_ACCELERATED);
#else
    Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#endif
    if (!Renderer) {
        // No GPU / headless: fall back to the software renderer
        Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!Renderer) {
        MessageBox(NULL, SDL_GetError(), "Error: SDL_CreateRenderer", 0);
        return;
    }
    // The game is laid out for a fixed resolution; SDL scales it to the window.
    SDL_RenderSetLogicalSize(Renderer, p->ResolutionX, p->ResolutionY);
    LoadSurfaces();
}

void CDDraw::InitWindowed(HWND hWnd)
{
    FullScreen = 0;
    CreateRenderer(hWnd);
}

void CDDraw::InitFullscreen(HWND hWnd)
{
    FullScreen = 1;
    SDL_SetWindowFullscreen((SDL_Window *)hWnd, SDL_WINDOW_FULLSCREEN_DESKTOP);
    CreateRenderer(hWnd);
}

BCImage *CDDraw::LoadImage(const char *file, bool useColorKey, COLORREF colorKey)
{
    SDL_Surface *surface = SDL_LoadBMP(file);
    if (!surface) {
        p->Engine->logerror(std::string("Unable to load ") + file + ": " + SDL_GetError());
        return NULL;
    }
    if (useColorKey) {
        SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, GetRValue(colorKey), GetGValue(colorKey), GetBValue(colorKey)));
    }
    BCImage *img = new BCImage;
    img->width = surface->w;
    img->height = surface->h;
    img->texture = SDL_CreateTextureFromSurface(Renderer, surface);
    SDL_FreeSurface(surface);
    if (!img->texture) {
        delete img;
        return NULL;
    }
    return img;
}

void CDDraw::FreeImage(BCImage *&img)
{
    if (img) {
        if (img->texture) SDL_DestroyTexture(img->texture);
        delete img;
        img = NULL;
    }
}

void CDDraw::LoadSurfaces()
{
    struct Entry { BCImage **target; const char *file; bool keyed; COLORREF key; };
    const Entry images[] = {
        { &imgArrowsRed, "imgArrowsRed.bmp", true, COLOR_TRANSPARENCY },
        { &imgArrows, "imgArrows.bmp", true, COLOR_TRANSPARENCY },
        { &imgBCLogo, "imgBCLogo.bmp", true, RGB(0,0,0) },
        { &imgBlackNumbers, "imgBlackNumbers.bmp", true, COLOR_TRANSPARENCY },
        { &imgBtnStaff, "imgBtnStaff.bmp", true, RGB(0,0,0) },
        { &imgBuildIcons, "imgBuildIcons.bmp", true, RGB(0,0,0) },
        { &imgBuildings, "imgBuildings.bmp", true, COLOR_TRANSPARENCY },
        { &imgBullets, "imgBullets.bmp", true, COLOR_TRANSPARENCY },
        { &imgCompany, "imgCompany.bmp", true, RGB(0,0,0) },
        { &imgCursor, "imgCursor.bmp", true, COLOR_TRANSPARENCY },
        { &imgDemolish, "imgDemolish.bmp", true, COLOR_TRANSPARENCY },
        { &imgGround, "imgGround.bmp", true, RGB(0,0,0) },
        { &imgHealth, "imgHealth.bmp", true, RGB(0,0,0) },
        { &imgInterface, "imgInterface.bmp", true, COLOR_TRANSPARENCY },
        { &imgInterfaceBottom, "imgInterfaceBottom.bmp", true, COLOR_TRANSPARENCY },
        { &imgInventorySelection, "imgInventorySelection.bmp", true, RGB(0,0,0) },
        { &imgItems, "imgItems.bmp", true, COLOR_TRANSPARENCY },
        { &imgLava, "imgLava.bmp", true, COLOR_TRANSPARENCY },
        { &imgLoading, "imgLoading.bmp", true, COLOR_TRANSPARENCY },
        { &imgLExplosion, "imgLExplosion.bmp", true, COLOR_TRANSPARENCY },
        { &imgMiniMapColors, "imgMiniMapColors.bmp", true, RGB(0,0,0) },
        { &imgMoneyBox, "imgMoneyBox.bmp", true, COLOR_TRANSPARENCY },
        { &imgMoneyDown, "imgMoneyDown.bmp", true, COLOR_TRANSPARENCY },
        { &imgMoneyUp, "imgMoneyUp.bmp", true, COLOR_TRANSPARENCY },
        { &imgMuzzleFlash, "imgMuzzleFlash.bmp", true, COLOR_TRANSPARENCY },
        { &imgPopulation, "imgPopulation.bmp", true, COLOR_TRANSPARENCY },
        { &imgRadarColors, "imgRadarColors.bmp", true, RGB(0,0,0) },
        { &imgResearch, "imgResearch.bmp", true, COLOR_TRANSPARENCY },
        { &imgResearchComplete, "imgResearchComplete.bmp", true, COLOR_TRANSPARENCY },
        { &imgRocks, "imgRocks.bmp", true, COLOR_TRANSPARENCY },
        { &imgSExplosion, "imgSExplosion.bmp", true, COLOR_TRANSPARENCY },
        { &imgSmoke, "imgSmoke.bmp", true, COLOR_TRANSPARENCY },
        { &imgStar, "imgStar.bmp", true, COLOR_TRANSPARENCY },
        { &imgTanks, "imgTanks.bmp", true, COLOR_TRANSPARENCY },
        { &imgTurretBase, "imgTurretBase.bmp", true, COLOR_TRANSPARENCY },
        { &imgTurretHead, "imgTurretHead.bmp", true, COLOR_TRANSPARENCY },
    };
    for (const Entry &e : images) {
        FreeImage(*e.target);
        *e.target = LoadImage(e.file, e.keyed, e.key);
    }
    FreeImage(Font);
    Font = LoadImage("imgFont.bmp", true, RGB(0,0,0));
    TextWidth = 8;
    lastOutputW = lastOutputH = 0;     // re-apply the scale mode to the new textures
}

void CDDraw::DestroySurfaces()
{
    FreeImage(imgArrowsRed);
    FreeImage(imgArrows);
    FreeImage(imgBCLogo);
    FreeImage(imgBlackNumbers);
    FreeImage(imgBtnStaff);
    FreeImage(imgBuildIcons);
    FreeImage(imgBuildings);
    FreeImage(imgBullets);
    FreeImage(imgCompany);
    FreeImage(imgCursor);
    FreeImage(imgDemolish);
    FreeImage(imgGround);
    FreeImage(imgHealth);
    FreeImage(imgInterface);
    FreeImage(imgInterfaceBottom);
    FreeImage(imgInventorySelection);
    FreeImage(imgItems);
    FreeImage(imgLava);
    FreeImage(imgLoading);
    FreeImage(imgLExplosion);
    FreeImage(imgMiniMapColors);
    FreeImage(imgMoneyBox);
    FreeImage(imgMoneyDown);
    FreeImage(imgMoneyUp);
    FreeImage(imgMuzzleFlash);
    FreeImage(imgPopulation);
    FreeImage(imgRadarColors);
    FreeImage(imgResearch);
    FreeImage(imgResearchComplete);
    FreeImage(imgRocks);
    FreeImage(imgSExplosion);
    FreeImage(imgSmoke);
    FreeImage(imgStar);
    FreeImage(imgTanks);
    FreeImage(imgTurretBase);
    FreeImage(imgTurretHead);
    FreeImage(Font);
    if (Renderer) {
        SDL_DestroyRenderer(Renderer);
        Renderer = NULL;
    }
}

/// <summary> Pixel art stays crisp when enlarged (nearest) and smooth when shrunk (linear) </summary>
void CDDraw::UpdateScaleMode()
{
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(Renderer, &ow, &oh);
    if (ow == lastOutputW && oh == lastOutputH) return;
    lastOutputW = ow;
    lastOutputH = oh;
    SDL_ScaleMode mode = (ow < p->ResolutionX || oh < p->ResolutionY) ? SDL_ScaleModeLinear : SDL_ScaleModeNearest;
    BCImage **all[] = { &imgArrows, &imgArrowsRed, &imgBCLogo, &imgBlackNumbers, &imgBtnStaff, &imgBuildIcons, &imgBuildings, &imgBullets, &imgCompany, &imgCursor, &imgDemolish, &imgGround, &imgHealth, &imgInterface, &imgInterfaceBottom, &imgInventorySelection, &imgItems, &imgLava, &imgLoading, &imgLExplosion, &imgMiniMapColors, &imgMoneyBox, &imgMoneyDown, &imgMoneyUp, &imgMuzzleFlash, &imgPopulation, &imgRadarColors, &imgResearch, &imgResearchComplete, &imgRocks, &imgSExplosion, &imgSmoke, &imgStar, &imgTanks, &imgTurretBase, &imgTurretHead };
    for (BCImage **img : all) if (*img && (*img)->texture) SDL_SetTextureScaleMode((*img)->texture, mode);
}

void CDDraw::Clear()
{
    if (!Renderer) return;
    SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);
    SDL_RenderClear(Renderer);
}

void CDDraw::Flip()
{
    if (!Renderer) return;
    LastFrameDrawCalls = DrawCalls;
    DrawCalls = 0;
    UpdateScaleMode();
    BCDialogsRender();   // Dear ImGui dialogs are drawn on top of the game frame
    SDL_RenderPresent(Renderer);
}

void CDDraw::Draw(BCImage* Surface, int X, int Y, int width, int height, int _sX, int _sY, int _clipwidth, int _clipheight)
{
    if (_clipwidth == -1) _clipwidth = p->ResolutionX;
    if (_clipheight == -1) _clipheight = p->ResolutionY;

    X += XOff;
    Y += YOff;
    _clipwidth += XOff;
    _clipheight += YOff;

    int l = 0, t = 0, r = 0, b = 0;

    if (!Renderer) return;
    if (!Surface) return;

    // Clip against the left/top edges
    if (X < 0) {l = abs(X); X = 0; width = width - l; if (width <= 0) return;}
    if (Y < 0) {t = abs(Y); Y = 0; height = height - t; if (height <= 0) return;}

    l = l + _sX;
    t = t + _sY;
    r = l + width;
    b = t + height;

    // Clip against the right/bottom clip region
    if (X + width > _clipwidth) {r = r - (X + width - _clipwidth); if (r <= l) return;}
    if (Y + height > _clipheight) {b = b - (Y + height - _clipheight); if (b <= t) return;}

    SDL_Rect src = { l, t, r - l, b - t };
    SDL_Rect dst = { X, Y, r - l, b - t };
    SDL_RenderCopy(Renderer, Surface->texture, &src, &dst);
    DrawCalls++;
}

void CDDraw::DTextOut(int X, int Y, string Text, COLORREF ColorVal)
{
    if (!Renderer || !Font) return;
    const int cw = 8, ch = 16, cols = 16;

    // Drop shadow first (black, offset by 1,2) then the text itself, like the original
    for (int pass = 0; pass < 2; pass++) {
        COLORREF colour = (pass == 0) ? RGB(0,0,0) : ColorVal;
        int ox = XOff + X + (pass == 0 ? 1 : 0);
        int oy = YOff + Y + (pass == 0 ? 2 : 0);
        SDL_SetTextureColorMod(Font->texture, GetRValue(colour), GetGValue(colour), GetBValue(colour));
        for (size_t i = 0; i < Text.length(); i++) {
            unsigned char c = (unsigned char)Text[i];
            if (c < 32) continue;
            int index = c - 32;
            SDL_Rect src = { (index % cols) * cw, (index / cols) * ch, cw, ch };
            SDL_Rect dst = { ox + (int)i * cw, oy, cw, ch };
            SDL_RenderCopy(Renderer, Font->texture, &src, &dst);
        }
    }
}

void CDDraw::DFillRect(int X, int Y, int width, int height)
{
    if (!Renderer) return;
    X += XOff;
    Y += YOff;

    SDL_Rect rect;
    rect.x = X;
    rect.y = Y;
    int right = X + width;
    int bottom = Y + height;
    if (right > p->ResolutionX + XOff) right = p->ResolutionX + XOff;
    if (bottom > p->ResolutionY + YOff) bottom = p->ResolutionY + YOff;
    if (rect.x < 0) rect.x = 0;
    if (rect.y < 0) rect.y = 0;
    rect.w = right - rect.x;
    rect.h = bottom - rect.y;
    if (rect.w <= 0 || rect.h <= 0) return;

    // DirectDraw's colour fill with no colour set is black
    SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(Renderer, &rect);
}

void CDDraw::WindowToGame(int wx, int wy, int *gx, int *gy)
{
    float lx = (float)wx, ly = (float)wy;
    if (Renderer) SDL_RenderWindowToLogical(Renderer, wx, wy, &lx, &ly);
    *gx = (int)lx;
    *gy = (int)ly;
}

bool CDDraw::SaveScreenshot(const char *path)
{
    if (!Renderer) return false;
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(Renderer, &w, &h);     // the real output, which can be larger than the logical size
    SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, SDL_PIXELFORMAT_BGR24);
    if (!shot) return false;
    bool ok = SDL_RenderReadPixels(Renderer, NULL, SDL_PIXELFORMAT_BGR24, shot->pixels, shot->pitch) == 0
              && SDL_SaveBMP(shot, path) == 0;
    SDL_FreeSurface(shot);
    return ok;
}
