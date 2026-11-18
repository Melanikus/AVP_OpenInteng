// See text_scale.h. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "text_scale.h"

#include <string.h>

#include <windows.h>   // GetPrivateProfileStringA / WritePrivateProfileStringA: the settings file

namespace
{
	int32_t scaleNow = host::kTextScaleLeast;

	// a / b rounded down, for a b above 0 (C++ rounds toward 0, which is up for a number under 0)
	int64_t FloorDivide(int64_t a, int64_t b)
	{
		int64_t const quotient = a / b;
		return (a % b != 0 && a < 0) ? quotient - 1 : quotient;
	}

	// A 64-bit result held to what an int32_t holds (only numbers far outside any window get there)
	int32_t Held(int64_t value)
	{
		return value > INT32_MAX ? INT32_MAX : (value < INT32_MIN ? INT32_MIN : (int32_t)value);
	}
}

namespace host
{
	bool TextScale_Parse(char const *text, int32_t *thousandths)
	{
		if (!text || !thousandths)
		{
			return false;
		}

		// at most kTextScaleMostLetters letters are looked at: a longer value is refused without reading past them
		size_t length = 0;

		while (length <= kTextScaleMostLetters && text[length] != '\0')
		{
			length++;
		}

		if (length > kTextScaleMostLetters)
		{
			return false;
		}

		size_t first = 0;
		size_t end = length;

		while (first < end && (text[first] == ' ' || text[first] == '\t'))
		{
			first++;
		}

		while (end > first && (text[end - 1] == ' ' || text[end - 1] == '\t'))
		{
			end--;
		}

		int64_t whole = 0;
		size_t at = first;

		while (at < end && text[at] >= '0' && text[at] <= '9')
		{
			whole = whole * 10 + (text[at] - '0');   // at most 16 digits: under 10^16, inside an int64_t
			at++;
		}

		if (at == first)
		{
			return false;   // no whole part: empty, a sign, a letter, or ".5"
		}

		int64_t part = 0;
		int decimals = 0;

		if (at < end && text[at] == '.')
		{
			at++;

			while (at < end && text[at] >= '0' && text[at] <= '9')
			{
				if (++decimals > 3)
				{
					return false;   // finer than a thousandth
				}

				part = part * 10 + (text[at] - '0');
				at++;
			}
		}

		if (at != end)
		{
			return false;   // anything after the number: a second '.', a comma, an exponent, a letter, a space inside it
		}

		for (int d = decimals; d < 3; d++)
		{
			part *= 10;
		}

		int64_t const value = whole * 1000 + part;

		if (value < kTextScaleLeast || value > kTextScaleMost)
		{
			return false;
		}

		*thousandths = (int32_t)value;
		return true;
	}

	std::string TextScale_Words(int32_t thousandths)
	{
		std::string words = std::to_string((long long)(thousandths / 1000));
		int32_t part = thousandths % 1000;

		if (part > 0)
		{
			std::string decimals = std::to_string((long long)(1000 + part)).substr(1);

			while (!decimals.empty() && decimals.back() == '0')
			{
				decimals.pop_back();
			}

			words += "." + decimals;
		}

		return words;
	}

	int32_t TextScale_Get()
	{
		return scaleNow;
	}

	void TextScale_Set(int32_t thousandths)
	{
		scaleNow = thousandths < kTextScaleLeast ? kTextScaleLeast : (thousandths > kTextScaleMost ? kTextScaleMost : thousandths);
	}

	int32_t TextScale_ToWindow(int32_t own)
	{
		return Held(FloorDivide((int64_t)own * scaleNow + 500, 1000));
	}

	int32_t TextScale_ToOwn(int32_t window)
	{
		return Held(FloorDivide((int64_t)window * 1000, scaleNow));
	}

	void TextScale_BoxToWindow(int32_t x, int32_t y, int32_t width, int32_t height, int32_t *windowX, int32_t *windowY, int32_t *windowWidth, int32_t *windowHeight)
	{
		int32_t const left = TextScale_ToWindow(x);
		int32_t const top = TextScale_ToWindow(y);

		*windowX = left;
		*windowY = top;
		*windowWidth = Held((int64_t)TextScale_ToWindow(Held((int64_t)x + width)) - left);
		*windowHeight = Held((int64_t)TextScale_ToWindow(Held((int64_t)y + height)) - top);
	}

	bool TextScale_Load(char const *settingsFile)
	{
		TextScale_Set(kTextScaleLeast);

		if (!settingsFile || !settingsFile[0])
		{
			return true;
		}

		// one letter more than a value of ours can have, so a longer one comes back cut and is refused
		char words[kTextScaleMostLetters + 2] = "";
		GetPrivateProfileStringA("Editor", "FontScale", "", words, (DWORD)sizeof(words), settingsFile);

		if (!words[0])
		{
			return true;
		}

		int32_t thousandths = kTextScaleLeast;

		if (!TextScale_Parse(words, &thousandths))
		{
			return false;
		}

		TextScale_Set(thousandths);
		return true;
	}

	bool TextScale_Save(char const *settingsFile)
	{
		return settingsFile && settingsFile[0] && WritePrivateProfileStringA("Editor", "FontScale", TextScale_Words(scaleNow).c_str(), settingsFile) != FALSE;
	}

	char const *TextScale_Refusal()
	{
		return "FontScale= in AVP_OpenInteng.ini isn't a text scale from 1 to 3 (as 1.5): the text stays inteng's size";
	}

	char const *TextScale_Choose(int32_t step, char const *settingsFile)
	{
		static std::string said;

		if (step < 1 || step > (int32_t)(sizeof(kTextScaleSteps) / sizeof(kTextScaleSteps[0])))
		{
			said = "Text size " + TextScale_Words(scaleNow);
			return said.c_str();
		}

		TextScale_Set(kTextScaleSteps[step - 1]);
		said = "Text size " + TextScale_Words(scaleNow) + (TextScale_Save(settingsFile) ? " (kept as FontScale=" + TextScale_Words(scaleNow) + " in AVP_OpenInteng.ini)"
			: " (not kept: no settings file)");
		return said.c_str();
	}
}
