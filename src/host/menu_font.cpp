// See menu_font.h. Written 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "menu_font.h"

#include <string.h>

namespace host
{
	bool MenuFont_Read(uint8_t const *bytes, size_t size, MenuFont *font, MenuFontRefusal *refusal)
	{
		MenuFontRefusal ignored;
		MenuFontRefusal *const why = refusal ? refusal : &ignored;

		*why = MenuFontRefusal::None;

		if (!bytes || !font || size < kMenuFontHeaderBytes)
		{
			*why = MenuFontRefusal::TooShort;
			return false;
		}

		if (size > kMenuFontMostBytes)
		{
			*why = MenuFontRefusal::TooLong;
			return false;
		}

		int16_t fixedWidth = 0;
		uint16_t height = 0;

		memcpy(&fixedWidth, bytes + 0x1c, 2);
		memcpy(&height, bytes + 0x1e, 2);

		int32_t const rowBytes = bytes[0x24];

		if (height == 0 || height > kMenuFontMostHeight)
		{
			*why = MenuFontRefusal::BadHeight;
			return false;
		}

		if (rowBytes == 0 || rowBytes > kMenuFontMostRowBytes)
		{
			*why = MenuFontRefusal::BadRowBytes;
			return false;
		}

		if (fixedWidth < 0 || fixedWidth > rowBytes * 8)
		{
			*why = MenuFontRefusal::BadFixedWidth;
			return false;
		}

		MenuFont read;
		size_t at = kMenuFontHeaderBytes;

		read.height = height;
		read.rowBytes = rowBytes;

		if (fixedWidth == 0)
		{
			// FUN_0046cb50 0x46cbf9-0x46cc28: a proportional font's 0x100 bytes of widths, 128 16-bit words
			if (size - at < 2 * kMenuFontLetters)
			{
				*why = MenuFontRefusal::ShortOfWidths;
				return false;
			}

			for (size_t i = 0; i < kMenuFontLetters; i++)
			{
				uint16_t width = 0;

				memcpy(&width, bytes + at + 2 * i, 2);

				if (width > rowBytes * 8)
				{
					*why = MenuFontRefusal::BadWidth;
					return false;
				}

				read.widths[i] = width;
			}

			at += 2 * kMenuFontLetters;
		}
		else
		{
			for (size_t i = 0; i < kMenuFontLetters; i++)
			{
				read.widths[i] = fixedWidth;
			}
		}

		size_t const letterBytes = kMenuFontLetters * (size_t)height * (size_t)rowBytes;

		if (size - at < letterBytes)
		{
			*why = MenuFontRefusal::ShortOfLetters;
			return false;
		}

		read.bits.assign(bytes + at, bytes + at + letterBytes);
		*font = read;
		return true;
	}

	char const *MenuFont_Why(MenuFontRefusal refusal)
	{
		switch (refusal)
		{
			case MenuFontRefusal::None: return "read";
			case MenuFontRefusal::TooShort: return "shorter than a font's header";
			case MenuFontRefusal::TooLong: return "longer than any menu font";
			case MenuFontRefusal::BadHeight: return "its letters' height is not 1 to 32";
			case MenuFontRefusal::BadRowBytes: return "its rows are not 1 to 4 bytes";
			case MenuFontRefusal::BadFixedWidth: return "its letters are wider than their rows";
			case MenuFontRefusal::ShortOfWidths: return "its letters' widths are cut short";
			case MenuFontRefusal::BadWidth: return "a letter is wider than its row";
			case MenuFontRefusal::ShortOfLetters: return "its letters are cut short";
		}

		return "refused";
	}

	bool MenuFont_Pixel(MenuFont const &font, unsigned letter, int32_t x, int32_t y)
	{
		if (letter >= kMenuFontLetters || x < 0 || y < 0 || y >= font.height || x >= font.rowBytes * 8)
		{
			return false;
		}

		size_t const at = ((size_t)letter * (size_t)font.height + (size_t)y) * (size_t)font.rowBytes + (size_t)(x / 8);

		return at < font.bits.size() && ((font.bits[at] >> (7 - x % 8)) & 1) != 0;
	}
}
