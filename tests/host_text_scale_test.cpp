// host_text_scale_test: the editor's text scale (text_scale.h): FontScale= read as hostile input, its words, and the two conversions between the editor's own pixels and the
// window's, exact at 1.0. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>
#include <string.h>

#include <string>

#include <windows.h>   // a settings file of the test's own, in the temporary folder

#include "text_scale.h"

using namespace host;

static int checks = 0;
static int failures = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checks++;
	failures += good ? 0 : 1;
}

// What TextScale_Parse makes of the words: the thousandths, or -1 when it refuses (and it must leave the number it was given alone then)
static int32_t Parsed(char const *words)
{
	int32_t got = -1;
	bool const read = TextScale_Parse(words, &got);
	return read ? got : (got == -1 ? -1 : -2);
}

int main()
{
	puts("1. FontScale= as hostile input");
	{
		struct { char const *words; int32_t want; } const good[] = {
			{ "1", 1000 }, { "1.0", 1000 }, { "1.25", 1250 }, { "1.5", 1500 }, { "2", 2000 }, { "2.000", 2000 }, { "3", 3000 }, { "3.0", 3000 }, { "2.125", 2125 },
			{ " 1.5 ", 1500 }, { "1.", 1000 },
		};

		for (auto const &one : good)
		{
			Check(Parsed(one.words) == one.want, std::string("\"") + one.words + "\" reads as " + std::to_string(one.want) + " thousandths");
		}

		char const *const bad[] = { "", " ", "0", "0.99", "0.999", "3.001", "3.5", "4", "-1", "+1.5", "1.2.3", "1,5", "1.2345", "1e0", "abc", "1.5x", "x1.5", ".5", "2 0", "nan", "inf",
			"00000000000000001", "1.25\t\t\t\t\t\t\t\t\t\t\t\t\t" };

		for (char const *words : bad)
		{
			Check(Parsed(words) == -1, std::string("\"") + words + "\" is refused, and the number given is left as it was");
		}

		Check(!TextScale_Parse(nullptr, nullptr), "no words at all are refused");
		int32_t kept = 1234;
		Check(!TextScale_Parse(nullptr, &kept) && kept == 1234, "no words leave the number alone");
	}

	puts("2. the words a scale is written as");
	{
		Check(TextScale_Words(1000) == "1", "1000 thousandths are written 1");
		Check(TextScale_Words(1250) == "1.25", "1250 are 1.25");
		Check(TextScale_Words(1500) == "1.5", "1500 are 1.5");
		Check(TextScale_Words(2000) == "2", "2000 are 2");
		Check(TextScale_Words(2125) == "2.125", "2125 are 2.125");
		Check(TextScale_Words(3000) == "3", "3000 are 3");

		for (int32_t step : kTextScaleSteps)
		{
			int32_t back = 0;
			Check(TextScale_Parse(TextScale_Words(step).c_str(), &back) && back == step, "the step " + std::to_string(step) + " reads back from its own words");
		}
	}

	puts("3. setting the scale holds it to 1 .. 3");
	{
		Check(TextScale_Get() == 1000, "it starts at 1000 (inteng's own size)");
		TextScale_Set(500);
		Check(TextScale_Get() == 1000, "under 1000 is held at 1000");
		TextScale_Set(9000);
		Check(TextScale_Get() == 3000, "over 3000 is held at 3000");
		TextScale_Set(1500);
		Check(TextScale_Get() == 1500, "1500 is kept");
	}

	puts("4. at 1.0 both conversions give back exactly what they were given");
	{
		TextScale_Set(1000);
		bool same = true;

		for (int32_t v = -5000; v <= 20000; v++)
		{
			same = same && TextScale_ToWindow(v) == v && TextScale_ToOwn(v) == v;
		}

		Check(same, "every number from -5000 to 20000 is itself both ways");

		int32_t x = 0, y = 0, w = 0, h = 0;
		TextScale_BoxToWindow(7, 9, 13, 21, &x, &y, &w, &h);
		Check(x == 7 && y == 9 && w == 13 && h == 21, "a box is itself");
	}

	puts("5. at 2.0 a length doubles and a press is in its own pixel");
	{
		TextScale_Set(2000);
		Check(TextScale_ToWindow(16) == 32, "inteng's 16-pixel line is 32 window pixels");
		Check(TextScale_ToWindow(-3) == -6, "a place left of the window doubles too");
		Check(TextScale_ToOwn(0) == 0 && TextScale_ToOwn(1) == 0 && TextScale_ToOwn(2) == 1 && TextScale_ToOwn(3) == 1, "window pixels 0 and 1 are own pixel 0, 2 and 3 own pixel 1");
		Check(TextScale_ToOwn(-1) == -1 && TextScale_ToOwn(-2) == -1 && TextScale_ToOwn(-3) == -2, "left of the window it rounds down too");
		Check(TextScale_ToOwn(2560) == 1280 && TextScale_ToOwn(1440) == 720, "the owner's 2560 x 1440 is 1280 x 720 of the editor's own pixels");
	}

	puts("6. at 1.25 the boxes that touch still touch");
	{
		TextScale_Set(1250);
		Check(TextScale_ToWindow(16) == 20, "a 16-pixel line is 20");
		Check(TextScale_ToWindow(19) == 24, "19 is 23.75, rounded to 24");
		Check(TextScale_ToOwn(2560) == 2048, "2560 window pixels are 2048 own ones");

		bool touching = true;

		for (int32_t row = 0; row < 200; row++)
		{
			int32_t x0, y0, w0, h0, x1, y1, w1, h1;
			TextScale_BoxToWindow(0, row * 19, 50, 19, &x0, &y0, &w0, &h0);
			TextScale_BoxToWindow(0, (row + 1) * 19, 50, 19, &x1, &y1, &w1, &h1);
			touching = touching && y0 + h0 == y1 && h0 >= 23 && h0 <= 24;
		}

		Check(touching, "200 rows of 19 own pixels, scaled: each starts where the one above ends, none overlaps, each 23 or 24 tall");

		bool inside = true;

		for (int32_t window = 0; window < 3000; window++)
		{
			int32_t const own = TextScale_ToOwn(window);
			inside = inside && TextScale_ToWindow(own) <= window + 1 && TextScale_ToWindow(own + 1) > window - 1;
		}

		Check(inside, "every window pixel's own pixel is the one whose scaled place is at it or just before it");
	}

	puts("7. at 3.0, the most");
	{
		TextScale_Set(3000);
		Check(TextScale_ToWindow(16) == 48 && TextScale_ToOwn(1440) == 480, "16 is 48, and 1440 window pixels are inteng's 480");
		Check(TextScale_ToWindow(20000000) == 60000000 && TextScale_ToOwn(60000000) == 20000000, "large numbers don't overflow");
	}

	puts("8. FontScale= in the settings file, read and saved");
	{
		char folder[MAX_PATH] = "";
		char file[MAX_PATH] = "";

		if (GetTempPathA(MAX_PATH, folder) > 0 && GetTempFileNameA(folder, "tsc", 0, file) != 0)
		{
			TextScale_Set(2000);
			Check(TextScale_Load(file) && TextScale_Get() == 1000, "a file with no FontScale= line: 1.0, and nothing to say");

			WritePrivateProfileStringA("Editor", "FontScale", "2", file);
			Check(TextScale_Load(file) && TextScale_Get() == 2000, "FontScale=2 is read as 2.0");

			WritePrivateProfileStringA("Editor", "FontScale", "big", file);
			Check(!TextScale_Load(file) && TextScale_Get() == 1000, "FontScale=big is refused: 1.0");
			Check(strstr(TextScale_Refusal(), "FontScale=") != nullptr && strstr(TextScale_Refusal(), "1 to 3") != nullptr, "and the status line's words say what is wrong and what it takes");

			WritePrivateProfileStringA("Editor", "FontScale", "1.50000000000000000000000000000000000000000000000000000000000001", file);
			Check(!TextScale_Load(file) && TextScale_Get() == 1000, "a value far longer than a scale is refused, read no further than its first letters");

			WritePrivateProfileStringA("Editor", "FontScale", "5", file);
			Check(!TextScale_Load(file) && TextScale_Get() == 1000, "FontScale=5 is refused (over 3)");

			TextScale_Set(1250);
			Check(TextScale_Save(file), "1.25 is saved");
			char words[32] = "";
			GetPrivateProfileStringA("Editor", "FontScale", "", words, sizeof(words), file);
			Check(strcmp(words, "1.25") == 0, "as FontScale=1.25");
			TextScale_Set(1000);
			Check(TextScale_Load(file) && TextScale_Get() == 1250, "and read back as 1.25");
			Check(!TextScale_Save("") && TextScale_Load("") && TextScale_Get() == 1000, "no settings file: nothing saved, 1.0 read");

			DeleteFileA(file);
		}
		else
		{
			Check(false, "the test's own settings file in the temporary folder");
		}
	}

	puts("9. the Options menu's steps: set at once, kept, and said");
	{
		char folder[MAX_PATH] = "";
		char file[MAX_PATH] = "";

		if (GetTempPathA(MAX_PATH, folder) > 0 && GetTempFileNameA(folder, "tss", 0, file) != 0)
		{
			std::string const said = TextScale_Choose(3, file);
			Check(TextScale_Get() == 1500, "step 3 is 1.5");
			char words[32] = "";
			GetPrivateProfileStringA("Editor", "FontScale", "", words, sizeof(words), file);
			Check(strcmp(words, "1.5") == 0, "and is kept as FontScale=1.5");
			Check(said.find("1.5") != std::string::npos && said.find("FontScale=") != std::string::npos, "and the status line says so: " + said);

			TextScale_Choose(1, file);
			Check(TextScale_Get() == 1000, "step 1 is 1.0, inteng's own size");
			TextScale_Choose(4, file);
			Check(TextScale_Get() == 2000, "step 4 is 2.0");
			TextScale_Choose(2, file);
			Check(TextScale_Get() == 1250, "step 2 is 1.25");

			TextScale_Choose(0, file);
			Check(TextScale_Get() == 1250, "no step 0: the scale stays");
			TextScale_Choose(5, file);
			Check(TextScale_Get() == 1250, "no step 5: the scale stays");

			std::string const unkept = TextScale_Choose(4, "");
			Check(TextScale_Get() == 2000 && unkept.find("not kept") != std::string::npos, "no settings file: set at once, and said not kept");

			DeleteFileA(file);
		}
		else
		{
			Check(false, "the test's own settings file in the temporary folder");
		}
	}

	TextScale_Set(1000);
	printf("%d checks, %d failed\n", checks, failures);
	printf(failures ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return failures ? 1 : 0;
}
