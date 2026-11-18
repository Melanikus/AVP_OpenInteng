// host_bar_letter_test: M-3, a letter key opens the bar's title whose underlined letter it is (bar_letter.h; inteng's FUN_004b7740 with the capital FUN_004b77e0 keeps). Written
// 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>

#include <string>

#include "bar_letter.h"

using namespace host;

static int checks = 0;
static int failures = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checks++;
	failures += good ? 0 : 1;
}

int main()
{
	// Construction's bar as inteng makes it (inteng-menus.json, table 0x5ced30): the place of each title's underlined letter (the '&' taken out)
	ui::Menu menus[6] = {};
	char const *const titles[6] = { "File", "Mode", "Object", "Shape", "Miscellaneous", "Options" };
	int32_t const access[6] = { 0, 0, 0, 0, 6, 0 };   // Miscel&laneous: its small l

	for (int i = 0; i < 6; i++)
	{
		menus[i].title = titles[i];
		menus[i].titleAccess = access[i];
	}

	puts("1. a title's letter opens it");
	Check(BarLetter_Title(menus, 6, 'F') == 0, "F is File's");
	Check(BarLetter_Title(menus, 6, 'M') == 1, "M is Mode's (the first title with it)");
	Check(BarLetter_Title(menus, 6, 'O') == 2, "O is Object's, the first of Object and Options");
	Check(BarLetter_Title(menus, 6, 'S') == 3, "S is Shape's");
	Check(BarLetter_Title(menus, 6, 'L') == 4, "L is Miscellaneous's: its underlined letter is a small l, which inteng keeps a capital (0x4b788b)");

	puts("2. anything else opens nothing");
	Check(BarLetter_Title(menus, 6, 'K') == -1, "K names no title");
	Check(BarLetter_Title(menus, 6, 'l') == -1, "a key code is never small (0x6c is not a letter key)");
	Check(BarLetter_Title(menus, 6, '1') == -1 && BarLetter_Title(menus, 6, 0x11) == -1 && BarLetter_Title(menus, 6, 0) == -1, "a digit, Control itself, nothing");
	Check(BarLetter_Title(nullptr, 6, 'F') == -1 && BarLetter_Title(menus, 0, 'F') == -1, "no titles");

	ui::Menu none[1] = {};
	none[0].title = "Help";
	none[0].titleAccess = -1;
	Check(BarLetter_Title(none, 1, 'H') == -1, "a title with no underlined letter is never opened by one");

	ui::Menu past[1] = {};
	past[0].title = "Hi";
	past[0].titleAccess = 7;
	Check(BarLetter_Title(past, 1, 'H') == -1, "an underline place past the title's end is not read");

	printf("%d checks, %d failed\n", checks, failures);
	printf(failures ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return failures ? 1 : 0;
}
