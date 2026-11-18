// See bar_letter.h. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "bar_letter.h"

#include <string.h>

namespace host
{
	int32_t BarLetter_Title(ui::Menu const *menus, size_t count, uint32_t key)
	{
		if (!menus || key < 'A' || key > 'Z')
		{
			return -1;
		}

		for (size_t m = 0; m < count; m++)
		{
			char const *const title = menus[m].title;
			int32_t const at = menus[m].titleAccess;

			if (!title || at < 0 || (size_t)at >= strlen(title))
			{
				continue;
			}

			// the bar keeps the letter a capital (FUN_004b77e0 0x4b788b: a to z less 0x20), so either case of it is the key's
			char letter = title[at];

			if (letter >= 'a' && letter <= 'z')
			{
				letter = (char)(letter - 0x20);
			}

			if ((uint32_t)(unsigned char)letter == key)
			{
				return (int32_t)m;
			}
		}

		return -1;
	}
}
