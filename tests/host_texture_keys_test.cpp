// The test for the keys the Texture Map takes for itself (texture_keys.h). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: a key is one action in a layout (Bishop, 2026-11-15). Arrow selection first took S, which is Modern WASD's camera Back: pressing it in the mode did both. The mode's keys win in the mode, so the camera must not hear them; this test pins which camera keys
// each layout loses, so a new Texture Map key that lands on a camera key is seen at once, and a camera key moved onto a Texture Map key too. Each check's words say what it proves. The program gives back a non-zero result if any check failed.
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "editor_actions.h"
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

static std::vector<uint8_t> Lost(KeyLayout layout, bool windowOn)
{
	uint8_t keys[16];
	size_t const count = host::TextureMapKeys_CameraKeysLost(layout, windowOn, keys, sizeof(keys));

	return std::vector<uint8_t>(keys, keys + (count < sizeof(keys) ? count : sizeof(keys)));
}

static bool Has(std::vector<uint8_t> const &keys, uint8_t key)
{
	return std::find(keys.begin(), keys.end(), key) != keys.end();
}

int main()
{
	// the mode's own letters
	for (char const *letters = "XYERTCUGLS"; *letters; letters++)
	{
		Check(host::TextureMapKeys_Own((uint8_t)*letters, false), std::string("Texture Map takes ") + *letters + " for itself");
	}

	// TM-A (Frost, 2026-11-17): the mode's keys are inteng's key table's (FUN_00557ea0, texture_keys.h): W is its Polys Shown (0x11 / 0x12), 1 / 2 its animation frames, Tab its texture window
	Check(host::TextureMapKeys_Own('W', false) && !host::TextureMapKeys_Own('A', false) && !host::TextureMapKeys_Own('D', false) && !host::TextureMapKeys_Own('F', false), "it takes W (inteng's Polys Shown) and leaves A, D and F (F only while a flag View is on) alone");
	Check(!host::TextureMapKeys_Own(VK_LEFT, false) && !host::TextureMapKeys_Own(VK_UP, false) && host::TextureMapKeys_Own('1', false) && host::TextureMapKeys_Own(VK_TAB, false) && !host::TextureMapKeys_Own('3', false), "the arrows are the mode's only with the texture window on; 1 and Tab are inteng's in the polygon screen too, 3 only in the window");
	Check(host::TextureMapKeys_Own(VK_LEFT, true) && host::TextureMapKeys_Own(VK_DOWN, true) && host::TextureMapKeys_Own('3', true) && host::TextureMapKeys_Own(VK_TAB, true) && host::TextureMapKeys_Own(VK_OEM_PLUS, true) && host::TextureMapKeys_Own(VK_NEXT, true),
		"with the texture window on the arrows, 1-4, Tab, = and PageDown are the mode's");

	// the camera's keys the mode costs: the rule's cases, layout by layout
	std::vector<uint8_t> const classic = Lost(KeyLayout::Classic, false);
	std::vector<uint8_t> const modern = Lost(KeyLayout::ModernWasd, false);
	std::vector<uint8_t> const modernWindow = Lost(KeyLayout::ModernWasd, true);

	Check(classic.empty(), "Classic's camera loses no key in Texture Map (A and Z and the keypad are free)");
	Check(modern.size() == 3 && Has(modern, 'R') && Has(modern, 'S') && Has(modern, 'W'), "Modern WASD's camera loses exactly R (up), S (back) and W (forward) in Texture Map: the mode's turn, arrow-select and Polys Shown keys win");
	Check(!Has(modern, 'A') && !Has(modern, 'D') && !Has(modern, 'F'), "and keeps A, D and F");
	Check(Has(modernWindow, 'R') && Has(modernWindow, 'S') && Has(modernWindow, VK_LEFT) && Has(modernWindow, VK_RIGHT) && Has(modernWindow, VK_UP) && Has(modernWindow, VK_DOWN) && modernWindow.size() == 6, "with the texture window on Modern WASD's camera also loses its four arrows");
	Check(Lost(KeyLayout::Classic, true).empty(), "Classic's camera, on the keypad, loses nothing with the texture window on either");
	Check(host::TextureMapKeys_CameraKeysLost(KeyLayout::ModernWasd, false, nullptr, 0) == 3, "asking with no room still counts the keys lost");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
