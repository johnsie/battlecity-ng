/*
 * Battle City - portability layer.
 * Supplies the small subset of Win32 types and helpers that the game logic still uses,
 * so the same sources build on Linux, Windows (SDL2) and the web (Emscripten).
 */
#ifndef BC_PLATFORM_COMPAT_H
#define BC_PLATFORM_COMPAT_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>

#ifndef _WIN32
	#include <strings.h>
	#define _cdecl
	#define CALLBACK
	#define stricmp strcasecmp
	#define _stricmp strcasecmp
#endif

typedef uint32_t COLORREF;
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t  BYTE;
typedef int      BOOL;
typedef long     LONG;
typedef unsigned int UINT;
typedef uintptr_t WPARAM;
typedef intptr_t  LPARAM;
typedef intptr_t  LRESULT;
typedef void *HWND;
typedef void *HINSTANCE;
typedef void *HBRUSH;
typedef void *HDC;
typedef const char *LPWSTR;   // (the game passes narrow strings through it)

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

struct RECT  { LONG left, top, right, bottom; };
struct POINT { LONG x, y; };

// COLORREF layout is 0x00BBGGRR, exactly as in Win32
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r)) | ((WORD)((BYTE)(g)) << 8) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(c) ((BYTE)((c) & 0xFF))
#define GetGValue(c) ((BYTE)(((c) >> 8) & 0xFF))
#define GetBValue(c) ((BYTE)(((c) >> 16) & 0xFF))

// Time and sleeping (milliseconds)
unsigned long BCGetTicks();
void BCSleep(unsigned int ms);
#define Sleep(ms) BCSleep(ms)
#define GetTickCount() BCGetTicks()
#define timeGetTime() BCGetTicks()

// Message box (SDL_ShowSimpleMessageBox under the hood). Flags: 0 = OK, 1 = Yes/No.
#define MB_YESNO 1
#define MB_ICONQUESTION 0
int BCMessageBox(HWND parent, const char *text, const char *title, unsigned int flags);
#define MessageBox(h, text, title, flags) BCMessageBox((h), (text), (title), (flags))

// Win32 leftovers that are no-ops here
#define timeBeginPeriod(x) 0
#define timeEndPeriod(x) 0
char *BCStrlwr(char *s);
#define _strlwr(s) BCStrlwr(s)
#define strlwr(s) BCStrlwr(s)
#ifndef _WIN32
	#define __int64 long long
#endif

// Directories and files
void BCMakeDir(const char *path);
unsigned long BCGetCurrentDirectory(unsigned long size, char *buffer);
#define CreateDirectory(path, security) (BCMakeDir(path), 1)
#define GetCurrentDirectory(size, buffer) BCGetCurrentDirectory((size), (buffer))

// .ini files (GetPrivateProfile* equivalents)
unsigned long BCProfileGetString(const char *section, const char *key, const char *def, char *out, unsigned long size, const char *file);
unsigned int BCProfileGetInt(const char *section, const char *key, int def, const char *file);
bool BCProfileWriteString(const char *section, const char *key, const char *value, const char *file);
#define GetPrivateProfileString(section, key, def, out, size, file) BCProfileGetString((section), (key), (def), (out), (size), (file))
#define GetPrivateProfileInt(section, key, def, file) BCProfileGetInt((section), (key), (def), (file))
#define WritePrivateProfileString(section, key, value, file) BCProfileWriteString((section), (key), (value), (file))

// itoa is not standard C
char *BCItoa(int value, char *buffer, int radix);
#define itoa(value, buffer, radix) BCItoa((value), (buffer), (radix))

#endif
