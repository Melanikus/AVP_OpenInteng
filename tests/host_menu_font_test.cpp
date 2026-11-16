// host_menu_font_test: inteng's menu font file read as hostile input (menu_font.h). Every refusal is checked on a file made wrong in that one way, and named. Written
// 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>
#include <string.h>

#include <vector>

#include "menu_font.h"

using namespace host;

static int failures = 0;

static void Check(bool good, char const *what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what);
	failures += good ? 0 : 1;
}

// A font file of our own making (never inteng's bits): height, row bytes, fixed width (0: proportional with the given widths); every letter's rows filled with its number
static std::vector<uint8_t> MakeFont(uint16_t height, uint8_t rowBytes, int16_t fixedWidth, uint16_t width)
{
	std::vector<uint8_t> bytes(kMenuFontHeaderBytes, 0);

	memcpy(&bytes[0x1c], &fixedWidth, 2);
	memcpy(&bytes[0x1e], &height, 2);
	bytes[0x24] = rowBytes;

	if (fixedWidth == 0)
	{
		for (size_t i = 0; i < kMenuFontLetters; i++)
		{
			bytes.push_back((uint8_t)(width & 0xff));
			bytes.push_back((uint8_t)(width >> 8));
		}
	}

	for (size_t letter = 0; letter < kMenuFontLetters; letter++)
	{
		for (size_t r = 0; r < (size_t)height * rowBytes; r++)
		{
			bytes.push_back((uint8_t)letter);
		}
	}

	return bytes;
}

static MenuFontRefusal RefusalOf(std::vector<uint8_t> const &bytes)
{
	MenuFont font;
	MenuFontRefusal why = MenuFontRefusal::None;

	MenuFont_Read(bytes.data(), bytes.size(), &font, &why);
	return why;
}

int main()
{
	printf("A good file:\n");
	{
		std::vector<uint8_t> const bytes = MakeFont(16, 2, 0, 10);
		MenuFont font;
		MenuFontRefusal why = MenuFontRefusal::TooLong;

		Check(MenuFont_Read(bytes.data(), bytes.size(), &font, &why) && why == MenuFontRefusal::None, "a proportional 16-row font of 2-byte rows is read");
		Check(font.height == 16 && font.rowBytes == 2 && font.widths[65] == 10 && font.bits.size() == 128 * 16 * 2, "its height, row bytes, widths and bits are kept");
		// letter 2's rows are the byte 2 (0000 0010): pixel 6 of each byte is set, pixel 7 is not
		Check(MenuFont_Pixel(font, 2, 6, 0) && !MenuFont_Pixel(font, 2, 7, 0) && MenuFont_Pixel(font, 2, 14, 15), "a pixel is read leftmost bit first, in both bytes of a row");
		Check(!MenuFont_Pixel(font, 2, 16, 0) && !MenuFont_Pixel(font, 2, 0, 16) && !MenuFont_Pixel(font, 200, 0, 0) && !MenuFont_Pixel(font, 2, -1, 0), "a pixel outside the letter, or of no letter, is never set");

		std::vector<uint8_t> const fixed = MakeFont(8, 1, 8, 0);
		MenuFont small;

		Check(MenuFont_Read(fixed.data(), fixed.size(), &small, &why) && small.widths[0] == 8 && small.widths[127] == 8 && fixed.size() == 1100, "a fixed 8 x 8 font (1100 bytes, as inteng's font8.fnt) gives every letter width 8");
	}

	printf("Every refusal, each on a file made wrong in one way:\n");
	{
		std::vector<uint8_t> const good = MakeFont(16, 2, 0, 10);
		std::vector<uint8_t> bad;

		Check(RefusalOf(std::vector<uint8_t>(good.begin(), good.begin() + 0x4b)) == MenuFontRefusal::TooShort, "TooShort: one byte short of the header");
		bad = good;
		bad.resize(kMenuFontMostBytes + 1, 0);
		Check(RefusalOf(bad) == MenuFontRefusal::TooLong, "TooLong: a byte over the most a menu font can be");
		bad = good;
		bad[0x1e] = 0;
		bad[0x1f] = 0;
		Check(RefusalOf(bad) == MenuFontRefusal::BadHeight, "BadHeight: a height of 0");
		bad = good;
		bad[0x1e] = 33;
		Check(RefusalOf(bad) == MenuFontRefusal::BadHeight, "BadHeight: a height of 33");
		bad = good;
		bad[0x24] = 0;
		Check(RefusalOf(bad) == MenuFontRefusal::BadRowBytes, "BadRowBytes: rows of 0 bytes");
		bad = good;
		bad[0x24] = 5;
		Check(RefusalOf(bad) == MenuFontRefusal::BadRowBytes, "BadRowBytes: rows of 5 bytes");
		bad = MakeFont(8, 1, 9, 0);
		Check(RefusalOf(bad) == MenuFontRefusal::BadFixedWidth, "BadFixedWidth: letters 9 wide in rows of 8 bits");
		bad = MakeFont(8, 1, -1, 0);
		Check(RefusalOf(bad) == MenuFontRefusal::BadFixedWidth, "BadFixedWidth: a negative width");
		bad = std::vector<uint8_t>(good.begin(), good.begin() + kMenuFontHeaderBytes + 255);
		Check(RefusalOf(bad) == MenuFontRefusal::ShortOfWidths, "ShortOfWidths: the widths one byte short");
		bad = MakeFont(16, 2, 0, 17);
		Check(RefusalOf(bad) == MenuFontRefusal::BadWidth, "BadWidth: a letter 17 wide in rows of 16 bits");
		bad = std::vector<uint8_t>(good.begin(), good.end() - 1);
		Check(RefusalOf(bad) == MenuFontRefusal::ShortOfLetters, "ShortOfLetters: the last letter's last byte missing");
		Check(RefusalOf(std::vector<uint8_t>()) == MenuFontRefusal::TooShort, "TooShort: no bytes at all");

		MenuFont kept;

		kept.height = 7;
		MenuFont_Read(bad.data(), bad.size(), &kept, nullptr);
		Check(kept.height == 7, "a refused file leaves the font as it was");
		Check(strcmp(MenuFont_Why(MenuFontRefusal::ShortOfLetters), "its letters are cut short") == 0, "each refusal has its words for the status line");
	}

	printf(failures ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return failures ? 1 : 0;
}
