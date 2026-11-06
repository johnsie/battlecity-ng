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
#include <vector>
#include <cmath>
#include <algorithm>

CDDraw::CDDraw(CGame *game)
{
    Canvas = NULL;
    canvasW = canvasH = 0;
    ttfTried = false;
    TextAtlas = NULL;
    ShadowTex = NULL;
    atlasScale = 0;
    atlasSize = 0;
    lastOutputW = lastOutputH = 0;
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

/// <summary>
/// Turns colour-keyed pixels into real alpha and spreads the colour of nearby opaque pixels into the
/// transparent ones, so smooth (linear) scaling never blends in the key colour (no pink/black fringes).
/// </summary>
static SDL_Surface *KeyToAlpha(SDL_Surface *src, bool useColorKey, Uint8 kr, Uint8 kg, Uint8 kb)
{
    SDL_Surface *s = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_RGBA32, 0);
    if (!s) return NULL;
    SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_BLEND);
    if (!useColorKey) return s;

    const int w = s->w, h = s->h, pitch = s->pitch;
    Uint8 *base = (Uint8 *)s->pixels;
    bool anyKeyed = false;
    for (int y = 0; y < h; y++) {
        Uint8 *row = base + y * pitch;
        for (int x = 0; x < w; x++) {
            Uint8 *px = row + x * 4;
            if (px[0] == kr && px[1] == kg && px[2] == kb) { px[3] = 0; anyKeyed = true; }
        }
    }
    if (!anyKeyed) return s;

    // Bleed colour outwards a few pixels (enough for 2x-4x upscaling filters)
    std::vector<Uint8> done((size_t)w * h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) done[(size_t)y * w + x] = base[y * pitch + x * 4 + 3] != 0;
    for (int pass = 0; pass < 3; pass++) {
        std::vector<Uint8> next = done;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                if (done[(size_t)y * w + x]) continue;
                int r = 0, g = 0, b = 0, n = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    int yy = y + dy;
                    if (yy < 0 || yy >= h) continue;
                    for (int dx = -1; dx <= 1; dx++) {
                        int xx = x + dx;
                        if (xx < 0 || xx >= w || !done[(size_t)yy * w + xx]) continue;
                        const Uint8 *q = base + yy * pitch + xx * 4;
                        r += q[0]; g += q[1]; b += q[2]; n++;
                    }
                }
                if (!n) continue;
                Uint8 *px = base + y * pitch + x * 4;
                px[0] = (Uint8)(r / n); px[1] = (Uint8)(g / n); px[2] = (Uint8)(b / n);
                next[(size_t)y * w + x] = 1;
            }
        }
        done.swap(next);
    }
    return s;
}

BCImage *CDDraw::LoadImage(const char *file, bool useColorKey, COLORREF colorKey)
{
    SDL_Surface *loaded = SDL_LoadBMP(file);
    if (!loaded) {
        p->Engine->logerror(std::string("Unable to load ") + file + ": " + SDL_GetError());
        return NULL;
    }
    SDL_Surface *surface = KeyToAlpha(loaded, useColorKey, GetRValue(colorKey), GetGValue(colorKey), GetBValue(colorKey));
    SDL_FreeSurface(loaded);
    if (!surface) return NULL;
    BCImage *img = new BCImage;
    img->width = surface->w;
    img->height = surface->h;
    img->texture = SDL_CreateTextureFromSurface(Renderer, surface);
    SDL_FreeSurface(surface);
    if (!img->texture) {
        delete img;
        return NULL;
    }
    SDL_SetTextureBlendMode(img->texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(img->texture, SDL_ScaleModeNearest);   // sprites are always drawn 1:1 into the canvas
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
    for (BCImage *glow : { imgLExplosion, imgSExplosion, imgMuzzleFlash })
        if (glow && glow->texture) SDL_SetTextureBlendMode(glow->texture, SDL_BLENDMODE_ADD);   // fire glows instead of painting over the ground
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
    if (Canvas) { SDL_DestroyTexture(Canvas); Canvas = NULL; }
    if (TextAtlas) { SDL_DestroyTexture(TextAtlas); TextAtlas = NULL; }
    if (ShadowTex) { SDL_DestroyTexture(ShadowTex); ShadowTex = NULL; }
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
    // Pixel art stays crisp: an integer multiple of the game's resolution is used when the window is just above
    // one (nearest filtering, thin borders); otherwise the picture is scaled smoothly to fill the window.
    float fit = std::min((float)ow / p->ResolutionX, (float)oh / p->ResolutionY);
    bool integerFit = fit >= 1.0f && (fit - std::floor(fit)) / fit < 0.08f;   // rounding down wastes under 8% of the window
    SDL_RenderSetIntegerScale(Renderer, integerFit ? SDL_TRUE : SDL_FALSE);
    if (Canvas) SDL_SetTextureScaleMode(Canvas, (fit < 1.0f || !integerFit) ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
}

/// <summary> (Re)creates the offscreen canvas when the game's resolution changed </summary>
void CDDraw::EnsureCanvas()
{
    if (Canvas && canvasW == p->ResolutionX && canvasH == p->ResolutionY) return;
    if (Canvas) { SDL_DestroyTexture(Canvas); Canvas = NULL; }
    Canvas = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, p->ResolutionX, p->ResolutionY);
    canvasW = p->ResolutionX;
    canvasH = p->ResolutionY;
    lastOutputW = lastOutputH = 0;     // re-apply the scale mode to the new texture
}

void CDDraw::Clear()
{
    if (!Renderer) return;
    EnsureCanvas();
    textQueue.clear();
    if (Canvas) SDL_SetRenderTarget(Renderer, Canvas);   // everything the game draws now lands in the canvas
    SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);
    SDL_RenderClear(Renderer);
}

void CDDraw::Flip()
{
    if (!Renderer) return;
    LastFrameDrawCalls = DrawCalls;
    DrawCalls = 0;
    if (Canvas) {
        // One scaling step for the whole picture (no sampling across sprite-sheet neighbours, so no seams)
        UpdateScaleMode();
        SDL_SetRenderTarget(Renderer, NULL);
        SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);
        SDL_RenderClear(Renderer);
        // Scale explicitly (SDL's logical size is kept only so mouse coordinates map back to game pixels)
        int ow = 0, oh = 0;
        SDL_GetRendererOutputSize(Renderer, &ow, &oh);
        float fit = std::min((float)ow / canvasW, (float)oh / canvasH);
        bool integerFit = fit >= 1.0f && (fit - std::floor(fit)) / fit < 0.08f;
        float sc = integerFit ? std::floor(fit) : fit;
        SDL_Rect dst;
        dst.w = (int)(canvasW * sc + 0.5f);
        dst.h = (int)(canvasH * sc + 0.5f);
        dst.x = (ow - dst.w) / 2;
        dst.y = (oh - dst.h) / 2;
        SDL_RenderSetLogicalSize(Renderer, 0, 0);
        SDL_RenderCopy(Renderer, Canvas, NULL, &dst);
        FlushText(dst, sc);
        SDL_RenderSetLogicalSize(Renderer, p->ResolutionX, p->ResolutionY);
    }
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
    if (!ttfTried) { ttfTried = true; ttf.Load("Vera.ttf"); }
    if (ttf.Loaded() && Canvas) {
        // Smooth text is drawn after the game image is scaled, at the window's real resolution (see FlushText)
        textQueue.push_back({ X, Y, Text, ColorVal });
        return;
    }
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

/// <summary> A soft elliptical drop shadow (black, fading to nothing at the edge), clipped to the map view </summary>
void CDDraw::DrawShadow(int cx, int cy, int width, int height, int alpha, int clipW, int clipH)
{
    if (!Renderer) return;
    if (!ShadowTex) {
        const int w = 64, h = 32;
        std::vector<Uint32> px((size_t)w * h);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                float dx = (x + 0.5f - w / 2.0f) / (w / 2.0f), dy = (y + 0.5f - h / 2.0f) / (h / 2.0f);
                float d = std::sqrt(dx * dx + dy * dy);
                float a = d >= 1.0f ? 0.0f : (1.0f - d * d);        // smooth falloff
                a = a * a * (3.0f - 2.0f * a);
                px[(size_t)y * w + x] = ((Uint32)(a * 255.0f + 0.5f) << 24);   // black with alpha (RGBA32 little-endian: A in the top byte)
            }
        }
        SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(px.data(), w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
        if (surf) { ShadowTex = SDL_CreateTextureFromSurface(Renderer, surf); SDL_FreeSurface(surf); }
        if (!ShadowTex) return;
        SDL_SetTextureBlendMode(ShadowTex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(ShadowTex, SDL_ScaleModeLinear);
    }
    SDL_Rect clip = { XOff, YOff, clipW, clipH };
    SDL_RenderSetClipRect(Renderer, &clip);
    SDL_SetTextureAlphaMod(ShadowTex, (Uint8)std::min(255, std::max(0, alpha)));
    SDL_Rect dst = { XOff + cx - width / 2, YOff + cy - height / 2, width, height };
    SDL_RenderCopy(Renderer, ShadowTex, NULL, &dst);
    SDL_RenderSetClipRect(Renderer, NULL);
    DrawCalls++;
}

/// <summary> (Re)bakes the text atlas for the current output scale (quantised so it does not thrash) </summary>
bool CDDraw::EnsureTextAtlas(float scale)
{
    float q = std::floor(std::min(std::max(scale, 0.75f), 4.0f) * 8.0f + 0.5f) / 8.0f;
    if (TextAtlas && q == atlasScale) return true;
    if (TextAtlas) { SDL_DestroyTexture(TextAtlas); TextAtlas = NULL; }
    float px = 14.5f * q;
    int size = px > 24.0f ? 1024 : 512;
    std::vector<unsigned char> rgba;
    if (!ttf.Bake(px, size, rgba)) return false;
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(rgba.data(), size, size, 32, size * 4, SDL_PIXELFORMAT_RGBA32);
    if (!surf) return false;
    TextAtlas = SDL_CreateTextureFromSurface(Renderer, surf);
    SDL_FreeSurface(surf);
    if (!TextAtlas) return false;
    SDL_SetTextureBlendMode(TextAtlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(TextAtlas, SDL_ScaleModeNearest);
    atlasScale = q;
    atlasSize = size;
    return true;
}

/// <summary> Width in game pixels of text as DTextOut draws it </summary>
int CDDraw::MeasureText(const string &text)
{
    if (!ttfTried) { ttfTried = true; ttf.Load("Vera.ttf"); }
    if (!ttf.Loaded() || !Renderer || !EnsureTextAtlas(TextAtlas ? atlasScale : 1.0f)) return (int)text.length() * TextWidth;
    float w = 0;
    for (unsigned char c : text) {
        const BCGlyph *g = ttf.Glyph(c);
        const BCGlyph *sp = ttf.Glyph(' ');
        w += g ? g->advance : sp->advance;
    }
    return (int)(w / atlasScale + 0.5f);
}

/// <summary> Draws the frame's queued text on top of the scaled game image, at output resolution </summary>
void CDDraw::FlushText(const SDL_Rect &dst, float sc)
{
    if (textQueue.empty()) return;
    if (!EnsureTextAtlas(sc)) { textQueue.clear(); return; }
    const BCGlyph *space = ttf.Glyph(' ');
    int shadowX = std::max(1, (int)(sc + 0.5f)), shadowY = std::max(1, (int)(1.5f * sc + 0.5f));
    for (int pass = 0; pass < 2; pass++) {
        for (const QueuedText &t : textQueue) {
            COLORREF colour = (pass == 0) ? RGB(0,0,0) : t.colour;
            SDL_SetTextureColorMod(TextAtlas, GetRValue(colour), GetGValue(colour), GetBValue(colour));
            float pen = dst.x + t.x * sc + (pass == 0 ? shadowX : 0);
            float baseline = dst.y + (t.y + 12.5f) * sc + (pass == 0 ? shadowY : 0);
            for (unsigned char c : t.text) {
                const BCGlyph *g = ttf.Glyph(c);
                if (!g) { pen += space->advance; continue; }
                SDL_Rect src = { (int)g->x0, (int)g->y0, (int)(g->x1 - g->x0), (int)(g->y1 - g->y0) };
                SDL_Rect out = { (int)std::floor(pen + g->xoff + 0.5f), (int)std::floor(baseline + g->yoff + 0.5f), src.w, src.h };
                if (src.w > 0 && src.h > 0) SDL_RenderCopy(Renderer, TextAtlas, &src, &out);
                pen += g->advance;
            }
        }
    }
    textQueue.clear();
}

void CDDraw::DFillRectColor(int X, int Y, int width, int height, int r, int g, int b)
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
    SDL_SetRenderDrawColor(Renderer, (Uint8)r, (Uint8)g, (Uint8)b, 255);
    SDL_RenderFillRect(Renderer, &rect);
}

void CDDraw::DFillRect(int X, int Y, int width, int height)
{
    DFillRectColor(X, Y, width, height, 0, 0, 0);    // DirectDraw's colour fill with no colour set is black
}

void CDDraw::ApplyLogicalSize()
{
    if (!Renderer) return;
    SDL_RenderSetLogicalSize(Renderer, p->ResolutionX, p->ResolutionY);
    lastOutputW = lastOutputH = 0;    // re-pick the scaling mode for the new shape
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
    // Read the whole output: with logical scaling active the 'viewport' is only the letterboxed picture
    int lw = 0, lh = 0;
    SDL_RenderGetLogicalSize(Renderer, &lw, &lh);
    SDL_RenderSetLogicalSize(Renderer, 0, 0);
    bool ok = SDL_RenderReadPixels(Renderer, NULL, SDL_PIXELFORMAT_BGR24, shot->pixels, shot->pitch) == 0
              && SDL_SaveBMP(shot, path) == 0;
    SDL_RenderSetLogicalSize(Renderer, lw, lh);
    if (!ok) fprintf(stderr, "screenshot failed: %s\n", SDL_GetError());
    SDL_FreeSurface(shot);
    return ok;
}
