// texture_keys.h: which keys the Texture Map mode takes for itself. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE RULE (Bishop, 2026-11-15): a key is ONE action in a layout. The camera's keys (editor_actions.h, kEditorMoves) are held down to move; the Texture Map's own keys (X and Y flip, E and R turn, T wrap, C tile, U copy texturing, G goto, L deselect, S with an arrow
// key: arrow selection; with the texture window on also 1-4, the arrows, = and -, Esc, Tab, PageUp and PageDown) are pressed. Where the two meet - Modern WASD's R (up) and S (back) - the MODE'S key wins while the mode is on: the camera does not hear the key in Texture Map.
// The editor asks TextureMapKeys_Own for every key press in the mode and does not let the camera have the keys it says are the mode's.
#pragma once

#include <stdint.h>

#include "key_layout.h"

namespace host
{
	// true when the Texture Map takes this key (a VK_ number; letters are the capital letter) for itself; windowOn = the texture window (Tab) is on
	bool TextureMapKeys_Own(uint8_t key, bool windowOn);

	// The camera's move keys of a layout that the Texture Map takes for itself, written into keys (at most room); returns how many. They are the keys the camera loses in the mode.
	size_t TextureMapKeys_CameraKeysLost(KeyLayout layout, bool windowOn, uint8_t *keys, size_t room);
}
