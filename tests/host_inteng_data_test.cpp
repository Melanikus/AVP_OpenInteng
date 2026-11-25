// host_inteng_data_test.cpp: INTENG'S OWN DATA, IN THE EDITOR (the owner, 2026-11-24: "Dude, I was specific about RIFs", "This ONLY applied to rifs": only Rebellion's RIF
// data stays out of our source). Each copy is pinned by its SHA-256, the one inteng's own bytes have, so it can't change without this test failing:
//   - texture_colour_table.h, Select Colour's 256 colours: inteng.exe's 768 bytes at 0x5d77c8;
//   - menu_font_data.h, inteng's text font: its Tools\font16.fnt, 4428 bytes, which the menus read through MenuFont_Read's checks (height 16);
// AvP-docs tools/frost/inteng-data/inteng-data.js made them from inteng's files and checks them against those files, one for one ("check"). Written 2026-11-24 by
// Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <stdint.h>

#include <string>

#include "sha256_stream.h"          // avpx 3dc: SHA-256 (FIPS 180-4)
#include "texture_colour_table.h"   // host::kIntengColourTable
#include "menu_font.h"              // host::MenuFont_Read
#include "menu_font_data.h"         // host::kIntengMenuFont16

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

static std::string Sha256Hex(uint8_t const *bytes, size_t size)
{
	Sha256Stream s;
	uint8_t digest[32];
	char hex[65];

	Sha256_Start(&s);
	Sha256_Add(&s, bytes, size);
	Sha256_Finish(&s, digest);

	for (int i = 0; i < 32; i++)
	{
		snprintf(hex + i * 2, 3, "%02x", digest[i]);
	}

	return std::string(hex, 64);
}

int main()
{
	// SELECT COLOUR'S TABLE: inteng.exe's bytes, as inteng-data.js "check" compared them (EQUAL, 2026-11-24)
	std::string const colours = Sha256Hex(host::kIntengColourTable, sizeof(host::kIntengColourTable));

	Check(sizeof(host::kIntengColourTable) == 768, "the colour table is 768 bytes: 256 colours of r, g, b");
	Check(colours == "49068d82489296538024839d491157213b8835c201b1c79ec7f88cd3e808c647", "its SHA-256 is inteng.exe's 768 bytes at 0x5d77c8 (" + colours + ")");

	bool inRange = true;

	for (size_t i = 0; i < sizeof(host::kIntengColourTable); i++)
	{
		inRange = inRange && host::kIntengColourTable[i] <= 63;
	}

	Check(inRange, "every value is 0..63, as inteng's colours are");
	Check(host::kIntengColourTable[0] == 0 && host::kIntengColourTable[1] == 0 && host::kIntengColourTable[2] == 0, "colour 0 is black");
	Check(host::kIntengColourTable[149 * 3] == 21 && host::kIntengColourTable[149 * 3 + 1] == 0 && host::kIntengColourTable[149 * 3 + 2] == 0,
		"colour 149 is 21 0 0, the one acceptance-texture-map-colour's click gives (word 95540000)");

	// THE MENU FONT: font16.fnt's bytes, as inteng-data.js "check" compared them (EQUAL, 2026-11-24), and the menus' own checks take them
	std::string const font = Sha256Hex(host::kIntengMenuFont16, sizeof(host::kIntengMenuFont16));
	host::MenuFont menuFont;
	host::MenuFontRefusal why = host::MenuFontRefusal::None;
	bool const read = host::MenuFont_Read(host::kIntengMenuFont16, sizeof(host::kIntengMenuFont16), &menuFont, &why);

	Check(sizeof(host::kIntengMenuFont16) == 4428, "the font is 4428 bytes, as font16.fnt is");
	Check(font == "8cc449f3e47cc99df5b432c52f8775bd1adc243374e9e9f5681ff017619781d3", "its SHA-256 is font16.fnt's (" + font + ")");
	Check(read, std::string("MenuFont_Read takes it, as it took the file (") + (read ? "read" : host::MenuFont_Why(why)) + ")");
	Check(read && menuFont.height == 16, "its letters are 16 high, as inteng's text font is (" + std::to_string(menuFont.height) + ")");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
