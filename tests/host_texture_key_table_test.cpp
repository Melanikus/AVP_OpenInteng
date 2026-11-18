// The test for host::TextureMapKeyCommand (src/host/texture_keys.cpp): the Texture Map's keys as inteng's key table FUN_00557ea0 gives them (TM-A of
// AvP-docs/engine/inteng/reassess/texture-map-closeout-frost.md; the table read in reassess/texture-map-closeout-drake.md section K and dump-orphans-4.txt).
// Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT CHECKS: every key of each sub-mode's table (0 the polygons, 1 the texture window, 2 Merge, 3 multiple nodes) with no modifier, Shift and Ctrl, against the
// commands the decompile returns; the keys that depend on state (Q by the node count, W by Polys Shown, F / Shift+F only while a flag View is on); Return, Tab and
// Backspace as inteng's own 6, 7 and 0x1a; Shift and Ctrl together giving nothing; a key no table names giving nothing.

#include <windows.h>
#include <stdio.h>

#include <string>

#include "texture_keys.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

struct Row
{
	int subMode;
	uint8_t key;
	bool shift;
	bool ctrl;
	int32_t command;
};

static std::string Hex(int32_t value)
{
	char text[16];

	sprintf_s(text, "0x%x", (unsigned)value);
	return text;
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	host::TextureKeyState state = {};

	state.selectAs = 3;
	state.texturedShown = true;

	Row const rows[] =
	{
		// sub-mode 0, no modifier (0x557fd8)
		{ 0, VK_RETURN, false, false, 0x2c }, { 0, VK_TAB, false, false, 0x3 }, { 0, VK_BACK, false, false, 0x2d }, { 0, '1', false, false, 0xae }, { 0, '2', false, false, 0xad },
		{ 0, '7', false, false, 0x3b }, { 0, '8', false, false, 0x3c }, { 0, '9', false, false, 0x3d }, { 0, 'B', false, false, 0xcd }, { 0, 'C', false, false, 0x2a },
		{ 0, 'E', false, false, 0x1b }, { 0, 'G', false, false, 0xbd }, { 0, 'H', false, false, 0xe5 }, { 0, 'I', false, false, 0x34 }, { 0, 'L', false, false, 0x2b },
		{ 0, 'M', false, false, 0x3a }, { 0, 'N', false, false, 0xe6 }, { 0, 'O', false, false, 0xbf }, { 0, 'P', false, false, 0x32 }, { 0, 'Q', false, false, 0xf },
		{ 0, 'R', false, false, 0x19 }, { 0, 'T', false, false, 0x2e }, { 0, 'U', false, false, 0x31 }, { 0, 'V', false, false, 0xc1 }, { 0, 'W', false, false, 0x11 },
		{ 0, 'X', false, false, 0x6 }, { 0, 'Y', false, false, 0x7 }, { 0, VK_OEM_PLUS, false, false, 0x117 }, { 0, VK_OEM_COMMA, false, false, 0x36 },
		{ 0, VK_OEM_MINUS, false, false, 0x118 }, { 0, VK_OEM_PERIOD, false, false, 0x37 }, { 0, 'A', false, false, 0 }, { 0, 'S', false, false, 0 }, { 0, 'F', false, false, 0 },
		// sub-mode 0, Shift (0x557f39)
		{ 0, '1', true, false, 0xa7 }, { 0, '2', true, false, 0xa6 }, { 0, 'B', true, false, 0xe7 }, { 0, 'G', true, false, 0xbe }, { 0, 'K', true, false, 0x40 },
		{ 0, 'N', true, false, 0x10f }, { 0, 'O', true, false, 0xc0 }, { 0, 'P', true, false, 0xe3 }, { 0, 'R', true, false, 0x9 }, { 0, 'T', true, false, 0x2f },
		{ 0, 'U', true, false, 0xb3 }, { 0, 'V', true, false, 0xc2 }, { 0, VK_OEM_COMMA, true, false, 0x38 }, { 0, VK_OEM_PERIOD, true, false, 0x39 }, { 0, 'X', true, false, 0 },
		// sub-mode 0, Ctrl (0x557ee7)
		{ 0, '0', false, true, 0x3f }, { 0, VK_OEM_5, false, true, 0x3f }, { 0, 'B', false, true, 0x13e }, { 0, 'K', false, true, 0x41 }, { 0, 'R', false, true, 0x33 },
		{ 0, 'T', false, true, 0x30 }, { 0, 'X', false, true, 0x5 }, { 0, 'Z', false, true, 0x144 }, { 0, 'L', false, true, 0 },
		// sub-mode 1, the texture window (0x55818a, 0x558149, 0x558106)
		{ 1, VK_TAB, false, false, 0x2 }, { 1, '0', false, false, 0xf2 }, { 1, '1', false, false, 0xe8 }, { 1, '2', false, false, 0xe9 }, { 1, '3', false, false, 0xea },
		{ 1, '4', false, false, 0xeb }, { 1, '6', false, false, 0xf4 }, { 1, '7', false, false, 0xf3 }, { 1, '8', false, false, 0xee }, { 1, '9', false, false, 0xf1 },
		{ 1, 'C', false, false, 0x2a }, { 1, 'E', false, false, 0x1b }, { 1, 'I', false, false, 0x105 }, { 1, 'N', false, false, 0xa }, { 1, 'O', false, false, 0x35 },
		{ 1, 'P', false, false, 0xb }, { 1, 'R', false, false, 0x19 }, { 1, 'T', false, false, 0x103 }, { 1, 'X', false, false, 0x6 }, { 1, 'Y', false, false, 0x7 },
		{ 1, VK_OEM_COMMA, false, false, 0x36 }, { 1, VK_OEM_PERIOD, false, false, 0x37 }, { 1, VK_RETURN, false, false, 0 }, { 1, VK_BACK, false, false, 0 }, { 1, 'L', false, false, 0 },
		{ 1, 'A', true, false, 0x26 }, { 1, 'N', true, false, 0x10f }, { 1, 'Q', true, false, 0x23 }, { 1, 'R', true, false, 0x9 }, { 1, 'S', true, false, 0x25 },
		{ 1, 'W', true, false, 0x24 }, { 1, 'A', false, true, 0x21 }, { 1, 'Q', false, true, 0x1e }, { 1, 'R', false, true, 0x33 }, { 1, 'S', false, true, 0x20 },
		{ 1, 'W', false, true, 0x1f }, { 1, 'X', false, true, 0x5 },
		// sub-mode 3, multiple nodes (0x558236): as the window but L, S, no corners, no 1-4, no O
		{ 3, VK_TAB, false, false, 0x2 }, { 3, 'L', false, false, 0x104 }, { 3, 'S', false, false, 0x102 }, { 3, '1', false, false, 0 }, { 3, 'O', false, false, 0 },
		{ 3, 'N', true, false, 0x10f }, { 3, 'R', true, false, 0x9 }, { 3, 'A', true, false, 0 }, { 3, 'R', false, true, 0x33 }, { 3, 'X', false, true, 0x5 },
		{ 3, 'Q', false, true, 0 },
		// sub-mode 2, Merge (0x5582d6)
		{ 2, VK_TAB, false, false, 0x2 }, { 2, 'P', false, false, 0x32 }, { 2, 'X', false, true, 0x5 }, { 2, 'X', false, false, 0 }, { 2, 'P', true, false, 0 },
	};

	size_t wrong = 0;

	for (Row const &row : rows)
	{
		int32_t const got = host::TextureMapKeyCommand(row.subMode, row.key, row.shift, row.ctrl, state);

		if (got != row.command)
		{
			wrong++;
			Check(false, "sub-mode " + std::to_string(row.subMode) + (row.shift ? " Shift+" : row.ctrl ? " Ctrl+" : " ") + Hex(row.key) + " gives " + Hex(got) + ", inteng " + Hex(row.command));
		}
	}

	Check(wrong == 0, "every key of the four tables gives inteng's command (" + std::to_string(sizeof(rows) / sizeof(rows[0])) + " keys)");

	// the keys that depend on state
	state.selectAs = 4;
	Check(host::TextureMapKeyCommand(0, 'Q', false, false, state) == 0xe, "Q with Select As Quad gives Triangle (0xe), with Triangle Quad (0xf, above)");
	state.texturedShown = false;
	Check(host::TextureMapKeyCommand(0, 'W', false, false, state) == 0x12, "W with Polys Shown Untextured gives Textured (0x12), with Textured Untextured (0x11, above)");
	state.flagViewOn = true;
	state.flagAdd = 0x71;
	state.flagRemove = 0x72;
	Check(host::TextureMapKeyCommand(0, 'F', false, false, state) == 0x71 && host::TextureMapKeyCommand(0, 'F', true, false, state) == 0x72,
		"while a flag View is on, F is its Add and Shift+F its Remove (nothing otherwise, above)");

	// Shift and Ctrl together: nothing, in every sub-mode
	bool none = true;

	for (int subMode = 0; subMode < 4; subMode++)
	{
		for (int key = 1; key < 256; key++)
		{
			none = none && host::TextureMapKeyCommand(subMode, (uint8_t)key, true, true, state) == 0;
		}
	}

	Check(none, "Shift and Ctrl together give no command in any sub-mode");
	Check(host::TextureMapKeyCommand(4, 'X', false, false, state) == 0, "a sub-mode with no table gives nothing");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
