// inteng_options.cpp: the screen size inteng starts at (inteng_options.h; reassess/startup-screen-frost.md SS-1..SS-13). Written 2026-11-16 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "inteng_options.h"

#include <string.h>

namespace host
{
	namespace
	{
		bool SpaceOrTab(uint8_t letter)
		{
			return letter == ' ' || letter == '\t';
		}

		uint8_t Lower(uint8_t letter)
		{
			return letter >= 'A' && letter <= 'Z' ? (uint8_t)(letter + ('a' - 'A')) : letter;   // _strnicmp's own fold (FUN_00591640: 'A'..'Z' only)
		}

		// "%d" over letters [at, end): spaces and tabs skipped (sscanf skips white space before a number), an optional sign, 1 to kIntengOptionsMostDigits digits, inside the 32-bit range
		// (past it sscanf would wrap: SS-7). *next: the letter after the digits.
		bool ReadNumber(uint8_t const *text, size_t at, size_t end, int32_t *value, size_t *next)
		{
			while (at < end && (SpaceOrTab(text[at]) || text[at] == '\r' || text[at] == '\n' || text[at] == '\v' || text[at] == '\f'))
			{
				at++;
			}

			bool minus = false;

			if (at < end && (text[at] == '+' || text[at] == '-'))
			{
				minus = text[at] == '-';
				at++;
			}

			int64_t number = 0;
			int digits = 0;

			while (at < end && text[at] >= '0' && text[at] <= '9')
			{
				if (++digits > kIntengOptionsMostDigits)
				{
					return false;
				}

				number = number * 10 + (text[at] - '0');
				at++;
			}

			if (digits == 0 || number > (minus ? 2147483648LL : 2147483647LL))
			{
				return false;
			}

			*value = (int32_t)(minus ? -number : number);
			*next = at;
			return true;
		}
	}

	bool IntengOptions_Number(uint8_t const *bytes, size_t size, char const *key, int32_t *value)
	{
		if (!bytes || !key || !key[0] || !value || size > kIntengOptionsMostBytes)
		{
			return false;
		}

		// a 0 byte ends the text, as inteng's string functions see it (SS-4: and nothing past `size` is ever looked at)
		size_t end = 0;

		while (end < size && bytes[end] != 0)
		{
			end++;
		}

		size_t const keyLength = strlen(key);
		size_t at = 0;

		while (at < end)
		{
			while (at < end && SpaceOrTab(bytes[at]))
			{
				at++;
			}

			bool same = end - at >= keyLength;

			for (size_t i = 0; same && i < keyLength; i++)
			{
				same = Lower(bytes[at + i]) == Lower((uint8_t)key[i]);
			}

			if (same)
			{
				size_t p = at + keyLength;

				while (p < end && SpaceOrTab(bytes[p]))
				{
					p++;
				}

				if (p < end && bytes[p] == '=')
				{
					p++;

					while (p < end && SpaceOrTab(bytes[p]))
					{
						p++;
					}

					// the value: to the line's end (CR, LF, or the text's end)
					size_t valueEnd = p;

					while (valueEnd < end && bytes[valueEnd] != '\r' && bytes[valueEnd] != '\n')
					{
						valueEnd++;
					}

					// SS-5 (inteng's 200-byte stack copy) and SS-5b (an empty value at the very end left that copy unwritten): neither is a number. The first match decides either way.
					if (valueEnd == p || valueEnd - p > kIntengOptionsLongestValue)
					{
						return false;
					}

					size_t next = 0;

					return ReadNumber(bytes, p, valueEnd, value, &next);
				}
			}

			// the next line (inteng: to CR / LF, then one CR and one LF)
			while (at < end && bytes[at] != '\r' && bytes[at] != '\n')
			{
				at++;
			}

			if (at < end && bytes[at] == '\r')
			{
				at++;
			}

			if (at < end && bytes[at] == '\n')
			{
				at++;
			}
		}

		return false;
	}

	bool IntengOptions_Resolution(char const *text, size_t length, IntengScreenSize *size)
	{
		if (!text || !size)
		{
			return false;
		}

		size_t end = 0;

		while (end < length && text[end] != 0)
		{
			end++;
		}

		uint8_t const *const letters = reinterpret_cast<uint8_t const *>(text);
		int32_t width = 0;
		size_t next = 0;

		if (!ReadNumber(letters, 0, end, &width, &next))
		{
			return false;
		}

		size->width = width;

		// "%dx%d": the 'x' must follow the width at once (a literal in the format skips nothing), then the height's "%d" skips white space
		int32_t height = 0;

		if (next < end && letters[next] == 'x' && ReadNumber(letters, next + 1, end, &height, &next))
		{
			size->height = height;
		}

		return true;
	}

	void IntengOptions_ScreenSize(uint8_t const *bytes, size_t length, IntengScreenSize *size)
	{
		if (!size)
		{
			return;
		}

		IntengOptions_Number(bytes, length, "Screen Width", &size->width);    // 0x493d74
		IntengOptions_Number(bytes, length, "Screen Height", &size->height);  // 0x493d84
	}

	IntengStartWindow IntengOptions_StartWindow(IntengScreenSize wanted, int32_t monitorWidth, int32_t monitorHeight, int32_t workWidth, int32_t workHeight, int32_t frameWidth,
		int32_t frameHeight, bool framed)
	{
		// a monitor Windows gave no sensible size for is taken as the least size
		int32_t const mostWidth = monitorWidth > kIntengScreenLeastWidth ? monitorWidth : kIntengScreenLeastWidth;
		int32_t const mostHeight = monitorHeight > kIntengScreenLeastHeight ? monitorHeight : kIntengScreenLeastHeight;
		int32_t const frameW = frameWidth > 0 ? frameWidth : 0;
		int32_t const frameH = frameHeight > 0 ? frameHeight : 0;
		int32_t const workW = workWidth > 0 ? workWidth : mostWidth;
		int32_t const workH = workHeight > 0 ? workHeight : mostHeight;

		IntengStartWindow start;

		start.width = wanted.width < kIntengScreenLeastWidth ? kIntengScreenLeastWidth : (wanted.width > mostWidth ? mostWidth : wanted.width);
		start.height = wanted.height < kIntengScreenLeastHeight ? kIntengScreenLeastHeight : (wanted.height > mostHeight ? mostHeight : wanted.height);

		bool const fits = (int64_t)start.width + frameW <= workW && (int64_t)start.height + frameH <= workH;

		if (framed)
		{
			// -w: framed whatever the size; the client made smaller until the framed window fits the work area, never under the least size
			if (!fits)
			{
				int32_t const roomW = workW - frameW;
				int32_t const roomH = workH - frameH;

				if (start.width > roomW)
				{
					start.width = roomW > kIntengScreenLeastWidth ? roomW : kIntengScreenLeastWidth;
				}

				if (start.height > roomH)
				{
					start.height = roomH > kIntengScreenLeastHeight ? roomH : kIntengScreenLeastHeight;
				}
			}

			return start;
		}

		if (!fits && monitorWidth >= kIntengScreenLeastWidth && monitorHeight >= kIntengScreenLeastHeight)
		{
			// inteng's full screen (SS-8): a popup over the whole monitor
			start.width = monitorWidth;
			start.height = monitorHeight;
			start.popup = true;
		}

		return start;
	}
}
