// The hand-written half of inteng_menus.h: finding an item by its number, and a tag's words. (inteng_menus.cpp, the generated half, holds
// the screens.) Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (A6, the all-menus build). AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".

#include <string.h>

#include "inteng_menus.h"

namespace host
{
	const IntengItem *IntengMenus_Find(int32_t action)
	{
		// An action of 0 or below is no item's (every item's number is (its screen + 1) * 0x10000 and up)
		if (action <= 0)
		{
			return nullptr;
		}

		for (size_t s = 0; s < kIntengScreenCount; s++)
		{
			for (size_t m = 0; m < kIntengScreens[s].count; m++)
			{
				IntengMenu const &menu = kIntengScreens[s].menus[m];

				for (size_t i = 0; i < menu.count; i++)
				{
					if (menu.items[i].action == action)
					{
						return &menu.items[i];
					}
				}
			}

			// the other pages of a code-built screen (its row 1 page 0 and row 2 page 0 are among them)
			for (size_t p = 0; p < kIntengScreens[s].pageCount; p++)
			{
				IntengPage const &page = kIntengScreens[s].pages[p];

				for (size_t m = 0; m < page.count; m++)
				{
					for (size_t i = 0; i < page.menus[m].count; i++)
					{
						if (page.menus[m].items[i].action == action)
						{
							return &page.menus[m].items[i];
						}
					}
				}
			}
		}

		return nullptr;
	}

	const char *IntengMenus_Why(IntengTag tag)
	{
		switch (tag)
		{
			case IntengTag::Wip:
				return "Not in the editor yet: one of the old editor's items, coming in a later version";
			case IntengTag::Blender:
				return "Geometry and textures are edited in Blender (AVP_BlenderPlugin)";
			case IntengTag::NoAvp:
				return "The game never reads this (camera tracks, cutscenes, the other game's objects): shown so inteng's menus are complete";
			case IntengTag::Unfinished:
				return "The old editor never finished this item: its own handler does nothing, or only part of its job";
			default:
				return nullptr;
		}
	}

	bool IntengMenus_ScreenFromCommandWord(const wchar_t *word, size_t *screen, std::string *why)
	{
		size_t const kMostLetters = 64;
		std::string said;

		// Plain printable ASCII only, and not longer than the longest screen name could be: anything else is refused before it is compared
		bool plain = word != nullptr && word[0] != L'\0';

		for (size_t i = 0; plain && word[i] != L'\0'; i++)
		{
			if (i >= kMostLetters || word[i] < 0x20 || word[i] > 0x7E)
			{
				plain = false;
			}
			else
			{
				said += (char)word[i];
			}
		}

		if (plain)
		{
			for (size_t s = 0; s < kIntengScreenCount; s++)
			{
				if (said == kIntengScreens[s].name)
				{
					*screen = s;
					return true;
				}
			}
		}

		if (why)
		{
			*why = "-screen takes exactly one of these names, as written: ";

			for (size_t s = 0; s < kIntengScreenCount; s++)
			{
				*why += (s ? "; " : "");
				*why += kIntengScreens[s].name;
			}
		}

		return false;
	}

	const char *IntengMenus_TagWords(IntengTag tag)
	{
		switch (tag)
		{
			case IntengTag::Wip:
				return " [wip]";
			case IntengTag::Blender:
				return " [Blender]";
			case IntengTag::NoAvp:
				return " [no AvP]";
			case IntengTag::Unfinished:
				return " [unfinished in inteng]";
			default:
				return nullptr;
		}
	}
}
