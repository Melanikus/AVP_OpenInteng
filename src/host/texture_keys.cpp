// texture_keys.cpp: see texture_keys.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "texture_keys.h"

#include <windows.h>   // only for the keys' numbers

#include "editor_actions.h"

namespace host
{
	bool TextureMapKeys_Own(uint8_t key, bool windowOn)
	{
		switch (key)
		{
		case 'X': case 'Y': case 'E': case 'R': case 'T': case 'C': case 'U': case 'G': case 'L': case 'S':
			return true;
		default:
			break;
		}

		if (!windowOn)
		{
			return false;
		}

		return (key >= '1' && key <= '4') || key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN || key == VK_OEM_PLUS || key == VK_OEM_MINUS || key == VK_ESCAPE || key == VK_TAB || key == VK_NEXT || key == VK_PRIOR;
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
}
