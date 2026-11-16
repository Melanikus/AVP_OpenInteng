// See screenshot_name.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (P6). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include <stdio.h>
#include <string.h>

#include "save_copy.h"
#include "screenshot_name.h"

namespace host
{
	namespace
	{
		char const kEditedEnd[] = "-edited.rif";
	}

	std::string ScreenshotName(char const *openedPath, int number)
	{
		if (number < 0 || number >= kScreenshotNumbers)
		{
			return std::string();
		}

		// The saved copy's name is "<safe name>-edited.rif": the safe name is what is kept
		std::string name = SaveCopyName(openedPath);
		size_t const endLength = sizeof(kEditedEnd) - 1;

		if (name.size() > endLength && name.compare(name.size() - endLength, endLength, kEditedEnd) == 0)
		{
			name.erase(name.size() - endLength);
		}

		char digits[8];

		_snprintf_s(digits, sizeof(digits), _TRUNCATE, "P%02d.bmp", number);
		return name + digits;
	}

	int ScreenshotFirstFree(char const *openedPath, bool (*taken)(std::string const &name, void *context), void *context)
	{
		if (!taken)
		{
			return -1;
		}

		for (int number = 0; number < kScreenshotNumbers; number++)
		{
			if (!taken(ScreenshotName(openedPath, number), context))
			{
				return number;
			}
		}

		return -1;
	}

	std::string ScreenshotFolderFromCopyFolder(std::string const &copyFolder)
	{
		char const edited[] = "edited\\";
		size_t const length = sizeof(edited) - 1;

		if (copyFolder.size() < length || copyFolder.compare(copyFolder.size() - length, length, edited) != 0)
		{
			return std::string();
		}

		return copyFolder.substr(0, copyFolder.size() - length) + "pictures\\";
	}
}
