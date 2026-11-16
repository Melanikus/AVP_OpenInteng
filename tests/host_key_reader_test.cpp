// The test of key_reader (src/host/key_reader.cpp): while a scenario drives the editor the real keyboard is not read (Bishop 2026-11-16: a person holding Shift at the computer
// turned a scenario's S into Shift+S); outside a scenario it is, as inteng reads it. The real keyboard is a stand-in that says Shift is held. Written 2026-11-16 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <string.h>

#include <string>

#include <windows.h>

#include "key_reader.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// the person at the computer holds Shift (and nothing else)
static bool ShiftHeldByThePerson(int vk)
{
	return vk == VK_SHIFT;
}

int main()
{
	bool table[256];

	memset(table, 0, sizeof(table));
	host::KeyReader_SetRealReader(ShiftHeldByThePerson);

	Check(!host::KeyReader_Held(VK_SHIFT, table, true), "a scenario drives the editor: the person's real Shift is not read");
	Check(host::KeyReader_Held(VK_SHIFT, table, false), "no scenario: the real Shift is read, as inteng reads it");

	table[VK_SHIFT] = true;
	Check(host::KeyReader_Held(VK_SHIFT, table, true), "a scenario's own posted Shift (the table) is held");
	table[VK_SHIFT] = false;

	table[VK_DELETE] = true;
	Check(host::KeyReader_Held(VK_DELETE, table, true) && host::KeyReader_Held(VK_DELETE, table, false), "a key in the table is held in both");
	Check(!host::KeyReader_Held(VK_CONTROL, table, true) && !host::KeyReader_Held(VK_CONTROL, table, false), "a key neither says is not held");
	Check(!host::KeyReader_Held(-1, table, false) && !host::KeyReader_Held(256, table, false), "a key number outside 0..255 is not held");
	Check(!host::KeyReader_Held(VK_DELETE, nullptr, true), "no table: only the real keyboard, and not in a scenario");

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
