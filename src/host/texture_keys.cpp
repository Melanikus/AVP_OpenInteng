// texture_keys.cpp: see texture_keys.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "texture_keys.h"

#include <windows.h>   // only for the keys' numbers

#include "editor_actions.h"

namespace host
{
	bool TextureMapKeys_Own(uint8_t key, bool windowOn)
	{
		// TM-A (Frost 2026-11-17): every key inteng's key table gives a command (any state: Q and W have one either way), and ours (named): S with an arrow (arrow selection),
		// PageUp / PageDown (next / previous texture), and in the texture window its arrows, = / - and Esc
		TextureKeyState state = {};

		if (TextureMapKeyCommand(windowOn ? 1 : 0, key, false, false, state) != 0 || key == 'S' || key == VK_NEXT || key == VK_PRIOR)
		{
			return true;
		}

		return windowOn && (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN || key == VK_OEM_PLUS || key == VK_OEM_MINUS || key == VK_ESCAPE);
	}

	bool TextureMapKeys_OwnInSubMode(uint8_t key, int subMode)
	{
		if (subMode == 0 || subMode == 1)
		{
			return TextureMapKeys_Own(key, subMode == 1);
		}

		TextureKeyState state = {};

		// Merge's M, X and B are read straight from the key slots by FUN_005511f0 (0xd87a8d / 0xd87a98 / 0xd87a82), not through the table (TM-B 2a)
		return (subMode == 2 || subMode == 3) && (TextureMapKeyCommand(subMode, key, false, false, state) != 0 || (subMode == 2 && (key == 'M' || key == 'X' || key == 'B')));
	}

	size_t TextureMapKeys_CameraKeysLost(KeyLayout layout, bool windowOn, uint8_t *keys, size_t room)
	{
		size_t count = 0;

		for (int move = 0; move < (int)EditorMove::Count; move++)
		{
			uint8_t const key = EditorActions_MoveKey(layout, (EditorMove)move);

			if (key != 0 && TextureMapKeys_Own(key, windowOn))
			{
				if (keys && count < room)
				{
					keys[count] = key;
				}

				count++;
			}
		}

		return count;
	}

	int32_t TextureMapKeyCommand(int subMode, uint8_t key, bool shift, bool ctrl, TextureKeyState const &state)
	{
		// inteng's own codes for three keys (FUN_00557320's help names them: 7 Tab "Change to texture mode", 6 Return "Save changes", 0x1a Backspace "Undo changes")
		uint32_t const k = key == VK_RETURN ? 6u : key == VK_TAB ? 7u : key == VK_BACK ? 0x1au : (uint32_t)key;

		if (shift && ctrl)
		{
			return 0;   // no table has a case with both (0x557ec5 / 0x557f1b and their sub-mode twins)
		}

		// FUN_00557ea0, sub-mode by sub-mode (dump-orphans-4.txt; the cases at 0x557ee7, 0x557f39, 0x557fd8, 0x558106, 0x558149, 0x55818a, 0x558236, 0x5582d6)
		if (subMode == 0)
		{
			if (ctrl)
			{
				switch (k)
				{
				case 0x30: case 0xdc: return 0x3f;   // Ctrl+0, Ctrl+\: select all polygons
				case 'B': return 0x13e;                // headlight
				case 'K': return 0x41;                 // apply the current colour
				case 'R': return 0x33;                 // select part of the texture
				case 'T': return 0x30;                 // stop wrap / tile
				case 'X': return 0x5;                  // Exit
				case 'Z': return 0x144;                // texture counts by zone ("(The Mummy)": NOT BUILT, the owner's ruling)
				default: return 0;
				}
			}

			if (shift)
			{
				switch (k)
				{
				case '1': return 0xa7;
				case '2': return 0xa6;
				case 'B': return 0xe7;
				case 'F': return state.flagViewOn ? state.flagRemove : 0;
				case 'G': return 0xbe;
				case 'K': return 0x40;
				case 'N': return 0x10f;
				case 'O': return 0xc0;
				case 'P': return 0xe3;
				case 'R': return 0x9;
				case 'T': return 0x2f;
				case 'U': return 0xb3;
				case 'V': return 0xc2;
				case 0xbc: return 0x38;
				case 0xbe: return 0x39;
				default: return 0;
				}
			}

			switch (k)
			{
			case 6: return 0x2c;
			case 7: return 0x3;
			case 0x1a: return 0x2d;
			case '1': return 0xae;
			case '2': return 0xad;
			case '7': return 0x3b;
			case '8': return 0x3c;
			case '9': return 0x3d;
			case 'B': return 0xcd;
			case 'C': return 0x2a;
			case 'E': return 0x1b;
			case 'F': return state.flagViewOn ? state.flagAdd : 0;
			case 'G': return 0xbd;
			case 'H': return 0xe5;
			case 'I': return 0x34;
			case 'L': return 0x2b;
			case 'M': return 0x3a;
			case 'N': return 0xe6;
			case 'O': return 0xbf;
			case 'P': return 0x32;
			case 'Q': return state.selectAs != 4 ? 0xf : 0xe;    // (+0x9c != 4) + 0xe, 0x55801d
			case 'R': return 0x19;
			case 'T': return 0x2e;
			case 'U': return 0x31;
			case 'V': return 0xc1;
			case 'W': return state.texturedShown ? 0x11 : 0x12;  // 0x12 - (DAT_005fd6b4 != 0), 0x558036
			case 'X': return 0x6;
			case 'Y': return 0x7;
			case 0xbb: return 0x117;
			case 0xbc: return 0x36;
			case 0xbd: return 0x118;
			case 0xbe: return 0x37;
			default: return 0;
			}
		}

		if (subMode == 1 || subMode == 3)
		{
			bool const window = subMode == 1;

			if (ctrl)
			{
				switch (k)
				{
				case 'R': return 0x33;
				case 'X': return 0x5;
				case 'A': return window ? 0x21 : 0;   // the clockwise corners (menu set 8 #2-#5): the window only
				case 'Q': return window ? 0x1e : 0;
				case 'S': return window ? 0x20 : 0;
				case 'W': return window ? 0x1f : 0;
				default: return 0;
				}
			}

			if (shift)
			{
				switch (k)
				{
				case 'N': return 0x10f;
				case 'R': return 0x9;
				case 'A': return window ? 0x26 : 0;   // the anti-clockwise corners (set 8 #7-#10): the window only
				case 'Q': return window ? 0x23 : 0;
				case 'S': return window ? 0x25 : 0;
				case 'W': return window ? 0x24 : 0;
				default: return 0;
				}
			}

			switch (k)
			{
			case 7: return 0x2;
			case '0': return 0xf2;
			case '6': return 0xf4;
			case '7': return 0xf3;
			case '8': return 0xee;
			case '9': return 0xf1;
			case 'C': return 0x2a;
			case 'E': return 0x1b;
			case 'I': return 0x105;
			case 'N': return 0xa;
			case 'P': return 0xb;
			case 'R': return 0x19;
			case 'T': return 0x103;
			case 'X': return 0x6;
			case 'Y': return 0x7;
			case 0xbc: return 0x36;
			case 0xbe: return 0x37;
			case '1': return window ? 0xe8 : 0;   // the nodes: the window only
			case '2': return window ? 0xe9 : 0;
			case '3': return window ? 0xea : 0;
			case '4': return window ? 0xeb : 0;
			case 'O': return window ? 0x35 : 0;
			case 'L': return window ? 0 : 0x104;  // multiple nodes only
			case 'S': return window ? 0 : 0x102;
			default: return 0;
			}
		}

		if (subMode == 2)
		{
			if (ctrl)
			{
				return k == 'X' ? 0x5 : 0;
			}

			if (shift)
			{
				return 0;
			}

			return k == 7 ? 0x2 : k == 'P' ? 0x32 : 0;
		}

		return 0;
	}
}
