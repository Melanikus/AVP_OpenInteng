// texture_keys.h: which keys the Texture Map mode takes for itself. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RULE (Bishop, 2026-11-15): a key is ONE action in a layout. The camera's keys (editor_actions.h, kEditorMoves) are held down to move; the Texture Map's own keys (X and Y flip, E and R turn, T wrap, C tile, U copy texturing, G goto, L deselect, S with an arrow
// key: arrow selection; with the texture window on also 1-4, the arrows, = and -, Esc, Tab, PageUp and PageDown) are pressed. Where the two meet - Modern WASD's R (up) and S (back) - the MODE'S key wins while the mode is on: the camera does not hear the key in Texture Map.
// The editor asks TextureMapKeys_Own for every key press in the mode and does not let the camera have the keys it says are the mode's.
#pragma once

#include <stdint.h>


namespace host
{
	// true when the Texture Map takes this key (a VK_ number; letters are the capital letter) for itself; windowOn = the texture window (Tab) is on
	bool TextureMapKeys_Own(uint8_t key, bool windowOn);

	// The same by inteng's sub-mode (DAT_00914bf8: 0 the polygons, 1 the texture window, 2 Merge, 3 multiple nodes): 0 and 1 are TextureMapKeys_Own's; 2 and 3 take the keys their
	// table gives a command (D1 of the TM-A / TM-B split; Dietrich 2026-11-18), and 2 also M, X and B, which its frame FUN_005511f0 reads straight from the key slots.
	bool TextureMapKeys_OwnInSubMode(uint8_t key, int subMode);

	// The camera's move keys of a layout that the Texture Map takes for itself, written into keys (at most room); returns how many. They are the keys the camera loses in the mode.
	size_t TextureMapKeys_CameraKeysLost(bool windowOn, uint8_t *keys, size_t room);

	// ---- INTENG'S KEY TABLE (TM-A, Frost 2026-11-17; reassess/texture-map-closeout-drake.md section K) ----
	// The Texture Map's menu object (DAT_00914bfc, FUN_00557e50) is the shared menu class with its key slot +0x40 = FUN_00557ea0: every key word goes through it, and the
	// command it gives is what the screen's command switch runs (FUN_0054d260 in sub-mode 0, the sub-mode frames FUN_00551f80 / FUN_00552bb0 / FUN_005511f0), the same switch the
	// menus feed. The key word holds the key's code in its low byte, Shift as 0x80000000, Ctrl as 0x40000000; Shift and Ctrl together give no command in any sub-mode.
	struct TextureKeyState
	{
		int selectAs;          // the selection's node count (DAT_00914d38 +0x9c): 3 Triangle, 4 Quad. Q gives 0xe (Triangle) when it is 4, else 0xf (Quad)
		bool texturedShown;    // Polys Shown is Textured (DAT_005fd6b4): W gives 0x11 (Untextured) when it is, else 0x12 (Textured)
		bool flagViewOn;       // a flag View is on (DAT_00914d84): only then F / Shift+F give its Add / Remove
		int32_t flagAdd;       // that View's Add command (DAT_00914d88)
		int32_t flagRemove;    // that View's Remove command (DAT_00914d8c)
	};

	// The command inteng's key table gives a key (a Windows VK_ number; letters are the capital letter; Return, Tab and Backspace are taken as inteng's own 6, 7 and 0x1a),
	// 0 for none. subMode: 0 the polygons, 1 the texture window, 2 Merge, 3 multiple nodes (DAT_00914bf8, the menu object's +0x14).
	int32_t TextureMapKeyCommand(int subMode, uint8_t key, bool shift, bool ctrl, TextureKeyState const &state);
}
