/*
 * Battle City - Win32 dialog emulation API.
 *
 * The client's menus (login, new account, interview, meeting ...) were written as Win32 resource
 * dialogs. Rather than rewrite the logic in each dialog procedure, we keep it and provide the
 * handful of Win32 dialog functions it calls. Dialogs are rendered with Dear ImGui from layouts
 * generated out of BattleCityClient.rc (see tools/rc2dialogs.py and platform/dialogs.cpp).
 */
#ifndef BC_PLATFORM_DIALOGS_H
#define BC_PLATFORM_DIALOGS_H

#include "compat.h"
#include <functional>

typedef intptr_t INT_PTR;
typedef int (CALLBACK *DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef DLGPROC WNDPROC;

// Message identifiers (same values as Win32)
#define WM_PAINT        0x000F
#define WM_CLOSE        0x0010
#define WM_CHAR         0x0102
#define WM_INITDIALOG   0x0110
#define WM_COMMAND      0x0111
#define WM_VSCROLL      0x0115
#define WM_LBUTTONDBLCLK 0x0203
#define BM_GETCHECK     0x00F0
#define BM_SETCHECK     0x00F1
#define EM_LIMITTEXT    0x00C5
#define CB_ADDSTRING    0x0143
#define CB_GETCURSEL    0x0147
#define CB_GETLBTEXT    0x0148
#define CB_SETCURSEL    0x014E
#define LB_ADDSTRING    0x0180
#define LB_RESETCONTENT 0x0184
#define LB_SETCURSEL    0x0186
#define LB_GETCURSEL    0x0188
#define LB_GETTEXT      0x0189
#define LB_GETCOUNT     0x018B
#define LB_ERR          (-1)
#define BN_CLICKED      0
#define CBN_SELCHANGE   1
#define LBN_DBLCLK      2
#define BST_UNCHECKED   0
#define BST_CHECKED     1
#define SB_VERT         1
#define SB_THUMBTRACK   5
#define SIF_ALL         0x17
#define IDOK            1
#define IDCANCEL        2

#define MAKELONG(a, b)  ((LONG)(((WORD)((a) & 0xffff)) | ((DWORD)((WORD)((b) & 0xffff))) << 16))
#define LOWORD(l)       ((WORD)((DWORD)(l) & 0xffff))
#define HIWORD(l)       ((WORD)(((DWORD)(l) >> 16) & 0xffff))
#define MAKEINTRESOURCE(i) ((const char *)(uintptr_t)(i))

struct SCROLLINFO { UINT cbSize, fMask; int nMin, nMax; UINT nPage; int nPos, nTrackPos; };

// --- Dialog lifetime -----------------------------------------------------------------------
// Opens a dialog (by resource id). Unlike Win32 this does not block: the dialog is drawn every
// frame by BCDialogsRender() and the procedure is called with WM_INITDIALOG / WM_COMMAND.
INT_PTR BCDialogBox(HINSTANCE inst, const char *resourceId, HWND parent, DLGPROC proc);
bool    EndDialog(HWND dialog, INT_PTR result);
#define DialogBox(inst, id, parent, proc) BCDialogBox((inst), (id), (parent), (proc))

// --- Controls ------------------------------------------------------------------------------
HWND  GetDlgItem(HWND dialog, int id);
UINT  GetDlgItemText(HWND dialog, int id, char *buffer, int maxCount);
bool  SetDlgItemText(HWND dialog, int id, const char *text);
UINT  GetDlgItemInt(HWND dialog, int id, BOOL *translated, BOOL isSigned);
bool  SetDlgItemInt(HWND dialog, int id, UINT value, BOOL isSigned);
bool  CheckDlgButton(HWND dialog, int id, UINT check);
UINT  IsDlgButtonChecked(HWND dialog, int id);
LRESULT SendDlgItemMessage(HWND dialog, int id, UINT msg, WPARAM wParam, LPARAM lParam);

// Generic message send/post. Control handles come from GetDlgItem; the main window handle accepts WM_CLOSE.
LRESULT BCSendMessage(HWND target, UINT msg, WPARAM wParam, LPARAM lParam);
// (functions rather than macros: CSend has a member called SendMessage)
template <class W, class L> inline LRESULT SendMessage(HWND h, UINT m, W w, L l) { return BCSendMessage(h, m, (WPARAM)(w), (LPARAM)(l)); }
template <class W, class L> inline LRESULT PostMessage(HWND h, UINT m, W w, L l) { return BCSendMessage(h, m, (WPARAM)(w), (LPARAM)(l)); }

// Scroll helpers used to keep chat windows scrolled to the bottom
bool GetScrollInfo(HWND control, int bar, SCROLLINFO *info);

// Window procedure subclassing used by a few dialogs (list/chat controls)
#define GWL_WNDPROC (-4)
WNDPROC BCSetControlProc(HWND control, WNDPROC proc);
#define SetWindowLong(h, idx, v) BCSetControlProc((h), (WNDPROC)(v))
#define CallWindowProc(orig, h, m, w, l) ((orig) ? (orig)((h), (m), (WPARAM)(w), (LPARAM)(l)) : 0)

// Message box with a result callback (1 = Yes/OK, 2 = No). flags: 0 = OK, MB_YESNO = Yes/No.
void BCMessageBoxAsync(const char *text, const char *title, unsigned int flags, std::function<void(int)> done);
bool BCMessagesPending();

// Called by the main loop
void BCDialogsInit(void *sdlWindow, void *sdlRenderer);   // SDL_Window*, SDL_Renderer*
void BCDialogsProcessEvent(const void *sdlEvent);   // SDL_Event*, feeds Dear ImGui
void BCDialogsRender();                              // draws the active dialog on top of the frame
void BCDialogsCloseActive();                         // closes the open dialog (if any) without calling its procedure
bool BCDialogsActive();
HWND BCDialogsActiveHandle();                        // the open dialog (for tests), or NULL                              // true while a dialog is open (game ignores input)
void BCDialogsShutdown();

#endif
