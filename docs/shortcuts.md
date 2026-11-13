# Shortcuts

**Options > Shortcuts...** opens the shortcut editor (client/CHotkeys.cpp), in the spirit of StarCraft's hotkey editor.

- Every command has two key slots. Click a slot and press the new key; Backspace clears it, Esc cancels.
- The keyboard picture colours each key by the kind of command it runs (blue movement, red combat, green items, gold interface, purple voice) and shows the command's icon on it. Pointing at a command outlines its keys. Clicking a key gives it to the selected command.
- Giving a key that is already used to another command moves it, and the editor says so.
- Building (mayors): **Build menu** (F) opens the menu at the pointer, **Demolish** (X) puts the wrecking cursor on the pointer, and there is one **Build ...** command for each of the 26 buildings (unbound until you give them a key; they put that building on the cursor, and only for buildings you can build). Pressing the same key again puts the building back.
- Presets: **Default layout** and **WASD layout** (W A S D drive, Space fires, Q drops items, arrows still work).
- Keys 1 to 0 pick the tank and cannot be changed. Chat must keep a key.
- Changed keys are shown in gold with a Reset button. Nothing changes until **Save**; Cancel (or Esc) throws the edits away.

Bindings are stored as SDL scancodes in one small text file named `hotkeys` (next to the login on the desktop, localStorage in the browser): one `command=key1,key2` line per command.

Movement, fire, flares and "look around" are polled every frame (`CHotkeys::Down`, used by `BCReadInput`). The other commands run on the key press (`CHotkeys::ActionForKey`, then `CGame::DoHotkey`); the key press that ran a command is not also typed into the chat.

Test script commands (`BC_TEST_SCRIPT`): `hotkeys` opens the editor, `meeting` opens the meeting room, `scancode <n>` sends a key press, `state` prints the map, pointer and chat state.

# What's New

The Meeting Room's news box has two tabs: **The Daily Bullet** (the server's news) and **What's New**. The text of the second is `client/Changelog.h`: add a block at the top for each release.
