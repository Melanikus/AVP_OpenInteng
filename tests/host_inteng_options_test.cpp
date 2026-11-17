// The test for the screen size inteng starts at (inteng_options.h; reassess/startup-screen-frost.md SS-1..SS-13): IntengOptions.ini read by inteng's rules (FUN_0057a410 / FUN_0057a560)
// with its reader's faults fixed (SS-4, SS-5, SS-5b, SS-6), and the window bounded to the monitor (SS-8, SS-9). Written 2026-11-16 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt. No window, none of the game.

#include <stdio.h>
#include <string.h>

#include <string>

#include "inteng_options.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static uint8_t const *Bytes(std::string const &text)
{
	return reinterpret_cast<uint8_t const *>(text.data());
}

// the number of a key in this text, or -999999 when not found / not a number
static int32_t Number(std::string const &text, char const *key)
{
	int32_t value = -999999;

	IntengOptions_Number(Bytes(text), text.size(), key, &value);
	return value;
}

static std::string Show(IntengStartWindow const &w)
{
	return std::to_string(w.width) + " " + std::to_string(w.height) + (w.popup ? " popup" : " window");
}

int main()
{
	printf("IntengOptions.ini's numbers, by inteng's rules (FUN_0057a410 / FUN_0057a560)\n");
	{
		// his file, as inteng writes it (the nine keys, CR LF)
		std::string const his = "Prefer Primary Video Card = No\r\nScreen Width = 2560\r\nScreen Height = 1600\r\nBilinear Filtering = Yes\r\nMax Texture Size = 1000000000\r\n"
		                        "Prefer Palettised Textures = No\r\nForce 2d texturing = No\r\nForce 3d texturing = Yes\r\nWindows Mouse Pointer = No\r\n";

		Check(Number(his, "Screen Width") == 2560, "his file: Screen Width 2560");
		Check(Number(his, "Screen Height") == 1600, "his file: Screen Height 1600");
		Check(Number(his, "Max Texture Size") == 1000000000, "his file: Max Texture Size 1000000000 (ten digits)");
	}

	Check(Number("screen WIDTH=1280\n", "Screen Width") == 1280, "the key is compared without case (_strnicmp), no spaces around '=' needed");
	Check(Number("  \t Screen Width \t=\t 800\n", "Screen Width") == 800, "spaces and tabs before the key, around '=' and before the value are skipped");
	Check(Number("Screen Width = 800\nScreen Width = 900\n", "Screen Width") == 800, "the FIRST line with the key and '=' decides");
	Check(Number("Screen Width 5\nScreen Width = 900\n", "Screen Width") == 900, "a line with the key but no '=' is passed over");
	Check(Number("Screen Widthless = 5\nScreen Width = 900\n", "Screen Width") == 900, "a longer key is not the key ('=' must follow)");
	Check(Number("Screen Width = big\nScreen Width = 900\n", "Screen Width") == -999999, "the first match is not a number: the old value stays (later lines are not looked at)");
	Check(Number("Screen Width = 1280abc\n", "Screen Width") == 1280, "\"%d\": what follows the digits is not looked at");
	Check(Number("Screen Width = +1280\n", "Screen Width") == 1280, "\"%d\": a plus sign");
	Check(Number("Screen Width = -5\n", "Screen Width") == -5, "\"%d\": a minus sign (bounded later, SS-9)");
	Check(Number("Screen Width = 1280", "Screen Width") == 1280, "the last line needs no CR / LF");
	Check(Number("Screen Width = 1280\r\n", "Screen Height") == -999999, "a key that isn't there: not found");
	Check(Number("", "Screen Width") == -999999, "an empty file: not found");
	Check(Number("Screen Width = 2147483647\n", "Screen Width") == 2147483647, "the largest 32-bit number is read (bounded later, SS-9)");
	Check(Number("Screen Width = 2147483648\n", "Screen Width") == -999999, "past the 32-bit range is not a number (SS-7: sscanf would wrap)");
	Check(Number("Screen Width = -2147483648\n", "Screen Width") == INT32_MIN, "the least 32-bit number is read");
	Check(Number("Screen Width = 00000000001\n", "Screen Width") == -999999, "more than 10 digits is not a number, even when small");

	printf("INTENG BUGS, FIXED (MAPPING-ORDERS section 15)\n");
	{
		// SS-4: inteng's bytes had no 0 at their end, so its scan ran on past the file. Here the text goes on in memory after `size`: those bytes are never read.
		std::string const longer = "Screen Width = 12345";
		size_t const cut = strlen("Screen Width = 12");
		int32_t value = -999999;

		Check(IntengOptions_Number(Bytes(longer), cut, "Screen Width", &value) && value == 12, "SS-4 (0x493d34): no 0 at the end: the scan stops at the bytes given (12, not 12345)");

		std::string const keyCut = "Screen Wid";
		std::string const after = keyCut + "th = 7";

		value = -999999;
		Check(!IntengOptions_Number(Bytes(after), keyCut.size(), "Screen Width", &value) && value == -999999, "SS-4: a key cut by the file's end is not matched from the bytes after it");
	}
	{
		// SS-5: inteng copied the value into a 200-byte stack buffer with no limit
		std::string const fits = "Screen Width = 1280" + std::string(199 - 4, 'x') + "\n";
		std::string const over = "Screen Width = 1280" + std::string(300, 'x') + "\n";

		Check(Number(fits, "Screen Width") == 1280, "SS-5: a value of 199 characters (inteng's buffer with its 0) is read");
		Check(Number(over, "Screen Width") == -999999, "SS-5 (0x57a4ce-0x57a4ed): a value of 304 characters is not a number (inteng wrote past its stack buffer)");
	}
	Check(Number("Screen Width =   ", "Screen Width") == -999999, "SS-5b (0x57a4fb): an empty value at the very end is not a number (inteng read an unwritten buffer)");
	Check(Number("Screen Width =\r\nScreen Width = 900\r\n", "Screen Width") == -999999, "an empty value before CR LF: not a number, the first line still decides");
	{
		std::string const zero = std::string("Screen Width = 1\0Screen Height = 2\n", 36);

		Check(Number(zero, "Screen Width") == 1, "a 0 byte ends the value, as in inteng");
		Check(Number(zero, "Screen Height") == -999999, "a 0 byte ends the text: nothing after it is found, as in inteng");
	}
	{
		int32_t value = 5;
		std::string const big(kIntengOptionsMostBytes + 1, ' ');

		Check(!IntengOptions_Number(Bytes(big), big.size(), "Screen Width", &value) && value == 5, "SS-6: more than 64 KB is not read");
		Check(!IntengOptions_Number(nullptr, 10, "Screen Width", &value) && value == 5, "no bytes: false");
		Check(!IntengOptions_Number(Bytes("Screen Width = 1"), 16, nullptr, &value) && value == 5, "no key: false");
		Check(!IntengOptions_Number(Bytes("Screen Width = 1"), 16, "", &value) && value == 5, "an empty key: false");
		Check(!IntengOptions_Number(Bytes("Screen Width = 1"), 16, "Screen Width", nullptr), "no place for the value: false");
	}

	printf("The command line's -r (sscanf \"%%dx%%d\", 0x58cc0d)\n");
	{
		auto r = [](char const *text)
		{
			IntengScreenSize size;
			bool const read = IntengOptions_Resolution(text, strlen(text), &size);

			return std::string(read ? "read " : "not ") + std::to_string(size.width) + " " + std::to_string(size.height);
		};

		Check(r("1280x720") == "read 1280 720", "1280x720 (it is " + r("1280x720") + ")");
		Check(r("  1280x 720") == "read 1280 720", "spaces before each number are skipped (it is " + r("  1280x 720") + ")");
		Check(r("1280 x720") == "read 1280 480", "'x' must follow the width at once: only the width is read (it is " + r("1280 x720") + ")");
		Check(r("1280") == "read 1280 480", "a width alone (it is " + r("1280") + ")");
		Check(r("x720") == "not 640 480", "no width: nothing is read (it is " + r("x720") + ")");
		Check(r("") == "not 640 480", "nothing (it is " + r("") + ")");
		Check(r("1280x720junk") == "read 1280 720", "what follows the height is not looked at (it is " + r("1280x720junk") + ")");
		Check(r("12345678901x5") == "not 640 480", "more than 10 digits is not a number (it is " + r("12345678901x5") + ")");

		std::string const cut = "1280x7205";
		IntengScreenSize size;

		Check(IntengOptions_Resolution(cut.c_str(), 7, &size) && size.width == 1280 && size.height == 72, "only `length` letters are looked at (1280x72)");
		Check(!IntengOptions_Resolution(nullptr, 5, &size) && !IntengOptions_Resolution("1x1", 3, nullptr), "no text or no size: false");
	}

	printf("The size: inteng's defaults 640 x 480 (0x602c70 / 0x602c74), then -r, then the file (read last: it wins)\n");
	{
		std::string const both = "Screen Width = 2560\r\nScreen Height = 1600\r\n";
		std::string const widthOnly = "Screen Width = 1024\r\n";
		IntengScreenSize a;
		IntengScreenSize b;
		IntengScreenSize c;
		IntengScreenSize d;

		IntengOptions_ScreenSize(Bytes(both), both.size(), &a);
		IntengOptions_ScreenSize(Bytes(widthOnly), widthOnly.size(), &b);
		IntengOptions_ScreenSize(nullptr, 0, &c);
		IntengOptions_Resolution("800x600", 7, &d);
		IntengOptions_ScreenSize(Bytes(widthOnly), widthOnly.size(), &d);

		Check(a.width == 2560 && a.height == 1600, "both keys: 2560 x 1600");
		Check(b.width == 1024 && b.height == 480, "Screen Width only: 1024 x 480 (the height keeps inteng's default)");
		Check(c.width == 640 && c.height == 480, "no file: inteng's 640 x 480");
		Check(d.width == 1024 && d.height == 600, "-r800x600, then a file with Screen Width 1024 only: 1024 x 600 (the file wins where it has a number)");
	}

	printf("The window, bounded to the monitor (Bishop 11-16; SS-8, SS-9): a monitor of 2560 x 1600, its work area 2560 x 1560, the frame adds 16 x 39\n");
	{
		auto start = [](int32_t w, int32_t h, bool framed = false)
		{
			IntengScreenSize wanted;

			wanted.width = w;
			wanted.height = h;
			return Show(IntengOptions_StartWindow(wanted, 2560, 1600, 2560, 1560, 16, 39, framed));
		};

		Check(start(2560, 1600) == "2560 1600 popup", "his 2560 x 1600: inteng's full screen, a popup over the whole monitor (it is " + start(2560, 1600) + ")");
		Check(start(1280, 720) == "1280 720 window", "1280 x 720: a framed window whose client is 1280 x 720 (it is " + start(1280, 720) + ")");
		Check(start(99999, 99999) == "2560 1600 popup", "99999 x 99999: bounded to the monitor (it is " + start(99999, 99999) + ")");
		Check(start(100, 100) == "640 480 window", "100 x 100: at least 640 x 480, the size inteng falls back to (0x465eb4) (it is " + start(100, 100) + ")");
		Check(start(-5, -5) == "640 480 window", "-5 x -5: 640 x 480 (it is " + start(-5, -5) + ")");
		Check(start(2544, 1521) == "2544 1521 window", "2544 x 1521: the framed window (2560 x 1560) just fits the work area (it is " + start(2544, 1521) + ")");
		Check(start(2545, 1500) == "2560 1600 popup", "2545 x 1500: framed it would be wider than the work area: full screen (it is " + start(2545, 1500) + ")");
		Check(start(2000, 1522) == "2560 1600 popup", "2000 x 1522: framed it would be taller than the work area: full screen (it is " + start(2000, 1522) + ")");

		// -w (Bishop 11-16): always framed, even when the size fills the monitor: the client made smaller until the framed window fits the work area
		Check(start(2560, 1600, true) == "2544 1521 window", "-w with 2560 x 1600: framed, its client 2544 x 1521 so it fits the work area (it is " + start(2560, 1600, true) + ")");
		Check(start(1280, 720, true) == "1280 720 window", "-w with 1280 x 720: framed, 1280 x 720 (it is " + start(1280, 720, true) + ")");
		Check(start(100, 100, true) == "640 480 window", "-w with 100 x 100: 640 x 480 (it is " + start(100, 100, true) + ")");

		IntengScreenSize wanted;

		wanted.width = 1280;
		wanted.height = 720;
		Check(Show(IntengOptions_StartWindow(wanted, 0, 0, 0, 0, 16, 39, false)) == "640 480 window", "a monitor Windows gave no size for: 640 x 480, framed");
	}

	printf(checksFailed == 0 ? "All checks passed\n" : "SOME CHECKS FAILED (%d)\n", checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
