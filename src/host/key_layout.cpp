// See key_layout.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "key_layout.h"

#include <windows.h>
#include <string.h>

const char kKeyLayoutClassicWord[] = "Classic";
const char kKeyLayoutModernWord[] = "ModernWASD";

// Whether the length letters of text are word, letter for letter, and nothing more. The length is the one the reading gave, not where
// the first 0 is, so a word with a 0 and more after it isn't the word.
static bool IsExactly(const char *text, size_t length, const char *word)
{
	size_t const wordLength = strlen(word);

	return length == wordLength && memcmp(text, word, wordLength) == 0;
}

const char *KeyLayout_FromSetting(const char *text, size_t length, size_t room, KeyLayout *layout)
{
	*layout = KeyLayout::Classic;

	if (!text || length == 0)
	{
		return "AVP_OpenInteng.ini has no KeyLayout= line";
	}

	if (room == 0 || length >= room - 1)
	{
		return "AVP_OpenInteng.ini's KeyLayout= line is too long";
	}

	if (IsExactly(text, length, kKeyLayoutClassicWord))
	{
		return 0;
	}

	if (IsExactly(text, length, kKeyLayoutModernWord))
	{
		*layout = KeyLayout::ModernWasd;
		return 0;
	}

	return "AVP_OpenInteng.ini's KeyLayout= is neither Classic nor ModernWASD";
}

const char *KeyLayout_Load(const char *settingsFile, KeyLayout *layout)
{
	if (!settingsFile)
	{
		*layout = KeyLayout::Classic;
		return "the editor has no settings file to read KeyLayout= from";
	}

	char text[kKeyLayoutSettingRoom];
	DWORD const length = GetPrivateProfileStringA("Editor", "KeyLayout", "", text, (DWORD)sizeof(text), settingsFile);

	return KeyLayout_FromSetting(text, length, sizeof(text), layout);
}

bool KeyLayout_Save(const char *settingsFile, KeyLayout layout)
{
	if (!settingsFile)
	{
		return false;
	}

	const char *const word = layout == KeyLayout::ModernWasd ? kKeyLayoutModernWord : kKeyLayoutClassicWord;

	return WritePrivateProfileStringA("Editor", "KeyLayout", word, settingsFile) != FALSE;
}
