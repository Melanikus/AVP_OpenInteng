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

	bool EditorOptions_StartNumber(char const *text, int32_t *value)
	{
		if (!text || !value)
		{
			return false;
		}

		int32_t number = 0;
		size_t digits = 0;

		for (; text[digits] != '\0'; digits++)
		{
			if (digits >= 5 || text[digits] < '0' || text[digits] > '9')
			{
				return false;   // a sixth digit, a sign, a space or a letter: not a size (five digits can't pass 99999, so nothing wraps)
			}

			number = number * 10 + (text[digits] - '0');
		}

		if (digits == 0 || number < 1 || number > kEditorStartMostSide)
		{
			return false;
		}

		*value = number;
		return true;
	}

	int EditorOptions_StartFlag(char const *text)
	{
		if (!text)
		{
			return -1;
		}

		if (strcmp(text, "1") == 0 || _stricmp(text, "true") == 0)
		{
			return 1;
		}

		if (strcmp(text, "0") == 0 || _stricmp(text, "false") == 0)
		{
			return 0;
		}

		return -1;
	}

	EditorStart EditorOptions_Start(char const *settingsFile, char const **why)
	{
		EditorStart start;

		if (why)
		{
			*why = nullptr;
		}

		if (!settingsFile || !settingsFile[0])
		{
			return start;
		}

		char const missing[] = "missing";
		char const *const keys[3] = { "StartWidth", "StartHeight", "Windowed" };
		char texts[3][16];
		bool found[3];

		for (int k = 0; k < 3; k++)
		{
			// a text longer than the room is cut, so it is no number and no word
			GetPrivateProfileStringA("Editor", keys[k], missing, texts[k], sizeof(texts[k]), settingsFile);
			found[k] = strcmp(texts[k], missing) != 0;
		}

		int32_t width = 0;
		int32_t height = 0;
		bool const widthRead = found[0] && EditorOptions_StartNumber(texts[0], &width);
		bool const heightRead = found[1] && EditorOptions_StartNumber(texts[1], &height);
		int const windowed = found[2] ? EditorOptions_StartFlag(texts[2]) : 0;

		for (int k = 0; k < 3 && why && !*why; k++)
		{
			bool const refused = found[k] && (k == 0 ? !widthRead : k == 1 ? !heightRead : windowed < 0);

			if (refused)
			{
				*why = keys[k];
			}
		}

		// both sizes, or neither
		if (widthRead && heightRead)
		{
			start.sizeGiven = true;
			start.width = width;
			start.height = height;
		}

		start.windowed = windowed == 1;
		return start;
	}
}
