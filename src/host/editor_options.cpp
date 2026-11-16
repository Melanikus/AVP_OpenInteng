// editor_options.cpp: see editor_options.h. Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "editor_options.h"

#include <string.h>

#include <windows.h>

namespace host
{
	namespace
	{
		char const kKeys[3][24] = { "SelectByPolygons", "PreferBmp", "PolygonViewDistance" };

		// 1 or 0 for the text "1" or "0" and nothing else; -1 for anything else (a longer text, a sign, a space, a letter)
		int Switch(char const *text)
		{
			if (text[0] == '0' && text[1] == '\0')
			{
				return 0;
			}

			if (text[0] == '1' && text[1] == '\0')
			{
				return 1;
			}

			return -1;
		}
	}

	EditorOptions EditorOptions_Defaults()
	{
		EditorOptions options;

		options.selectByPolygons = true;
		options.preferBmp = false;
		options.polygonViewDistance = false;
		return options;
	}

	EditorOptions EditorOptions_Load(char const *settingsFile, char const **why)
	{
		EditorOptions options = EditorOptions_Defaults();
		bool *const slots[3] = { &options.selectByPolygons, &options.preferBmp, &options.polygonViewDistance };

		if (why)
		{
			*why = nullptr;
		}

		if (!settingsFile || !settingsFile[0])
		{
			return options;
		}

		for (int k = 0; k < 3; k++)
		{
			char text[16] = "";
			char const missing[] = "missing";

			// a text longer than the room is cut, so it is neither 0 nor 1
			GetPrivateProfileStringA("Editor", kKeys[k], missing, text, sizeof(text), settingsFile);

			if (strcmp(text, missing) == 0)
			{
				continue;
			}

			int const value = Switch(text);

			if (value < 0)
			{
				if (why && !*why)
				{
					*why = kKeys[k];
				}

				continue;
			}

			*slots[k] = value == 1;
		}

		return options;
	}

	bool EditorOptions_Save(char const *settingsFile, char const *key, bool on)
	{
		if (!settingsFile || !settingsFile[0] || !key)
		{
			return false;
		}

		bool known = false;

		for (int k = 0; k < 3; k++)
		{
			known = known || strcmp(key, kKeys[k]) == 0;
		}

		return known && WritePrivateProfileStringA("Editor", key, on ? "1" : "0", settingsFile) != 0;
	}
}
