/*
 * Battle City - DirectInput-style keyboard/mouse snapshot built from SDL.
 * CInput's game logic reads a 256-entry keyboard array indexed by DIK_* scancodes and a mouse
 * state struct; this header supplies those so that logic is unchanged.
 */
#ifndef BC_PLATFORM_INPUT_COMPAT_H
#define BC_PLATFORM_INPUT_COMPAT_H

// DirectInput scancodes used by the game (values as in dinput.h)
#define DIK_1        0x02
#define DIK_2        0x03
#define DIK_3        0x04
#define DIK_4        0x05
#define DIK_5        0x06
#define DIK_6        0x07
#define DIK_7        0x08
#define DIK_8        0x09
#define DIK_9        0x0A
#define DIK_0        0x0B
#define DIK_TAB      0x0F
#define DIK_LCONTROL 0x1D
#define DIK_LSHIFT   0x2A
#define DIK_RSHIFT   0x36
#define DIK_RCONTROL 0x9D
#define DIK_UP       0xC8
#define DIK_LEFT     0xCB
#define DIK_RIGHT    0xCD
#define DIK_DOWN     0xD0

struct DIMOUSESTATE {
	long lX, lY, lZ;
	unsigned char rgbButtons[4];   // bit 7 set while pressed; [0] = left, [1] = right
};

// Fills buffer[256] (bit 7 = pressed) and the mouse state (position in window pixels).
// Test hook: force a key to read as pressed/released regardless of the real keyboard
void BCTestSetKey(int dik, bool down);

void BCReadInput(char keyboard[256], DIMOUSESTATE *mouse, int *mouseWindowX, int *mouseWindowY);

#endif
