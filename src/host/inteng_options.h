// inteng_options.h: the screen size inteng starts at, from its command line's -r and its IntengOptions.ini (reassess/startup-screen-frost.md SS-1..SS-13). Written 2026-11-16 by
// Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// INTENG (FUN_0058c9e0): 640 x 480 (0x602c70 / 0x602c74), then the command line's -r<w>x<h> (sscanf "%dx%d", 0x58cc0d), then IntengOptions.ini's "Screen Width" / "Screen Height"
// (FUN_00493ce0, read last, so the file wins), each key found by FUN_0057a410's rules and read by sscanf "%d" (FUN_0057a560). Then the window covers the screen and DirectDraw takes it
// exclusively at that display mode (FUN_0058bdf0, 0x494c40, FUN_00465d50), or with -w a framed window whose client is that size.
//
// THE FILE IS HOSTILE INPUT. Inteng's reader had four faults, fixed here (MAPPING-ORDERS section 15): its bytes were never ended by a 0, so the scan ran past the file (SS-4); a value was copied
// into a 200-byte stack buffer with no limit (SS-5); an empty value at the file's very end left that buffer unwritten (SS-5b); any size of file was read whole (SS-6). Every scan here is
// bounded by the bytes given, a value longer than inteng's buffer or empty is not a number, and the caller reads at most kIntengOptionsMostBytes. The file is only read, never written (SS-11).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace host
{
	size_t const kIntengOptionsMostBytes = 64 * 1024;     // inteng's own file is under 400 bytes
	size_t const kIntengOptionsLongestValue = 199;        // inteng's value buffer: 200 bytes with its closing 0 (FUN_0057a410 local_c8)
	int const kIntengOptionsMostDigits = 10;              // with the 32-bit range below: more is not a number here (sscanf "%d" would wrap: SS-7)
	int32_t const kIntengScreenLeastWidth = 640;          // the size inteng falls back to when a display mode can't be had (0x465eb4)
	int32_t const kIntengScreenLeastHeight = 480;

	// One number of the file, by inteng's rules (FUN_0057a410 + FUN_0057a560): line by line, leading spaces and tabs skipped, the key compared WITHOUT CASE for its own length, spaces and tabs,
	// '=' (else the next line), spaces and tabs, then the value to the line's end. The FIRST line with the key and '=' decides: its value read as "%d" (an optional sign, then digits; what
	// follows the digits is not looked at). True and *value when it is a number; false, *value untouched, when the key isn't there or its value is not a number. A 0 byte ends the text, as
	// in inteng. Bytes past `size` are never read.
	bool IntengOptions_Number(uint8_t const *bytes, size_t size, char const *key, int32_t *value);

	struct IntengScreenSize
	{
		int32_t width = 640;    // inteng's defaults (0x602c70 / 0x602c74)
		int32_t height = 480;
	};

	// The command line's -r, as inteng's sscanf "%dx%d" reads what follows the letter (0x58cc0d): spaces skipped, a number, 'x' right after it, spaces skipped, a number. The width is
	// taken when the first number is read, the height when both are; what isn't read keeps its value. False when not even the width could be read. At most `length` letters are looked at.
	bool IntengOptions_Resolution(char const *text, size_t length, IntengScreenSize *size);

	// IntengOptions.ini's "Screen Width" and "Screen Height" over *size, each on its own (a key not there, or not a number, keeps what *size had: the default or -r's)
	void IntengOptions_ScreenSize(uint8_t const *bytes, size_t length, IntengScreenSize *size);

	struct IntengStartWindow
	{
		int32_t width = 0;      // the window's client: what the editor draws in
		int32_t height = 0;
		bool popup = false;     // true: a window with no frame over the whole monitor (inteng's full screen, SS-8); false: a framed window whose client is width x height
	};

	// Where the editor starts (Bishop's ruling 11-16, "bounded to the desktop", SS-9): the size kept to at least 640 x 480 and at most the monitor's. When the framed window wouldn't fit the
	// monitor's work area (frameWidth / frameHeight: what the frame and title add), it is inteng's full screen: a popup over the whole monitor. Ours never changes the display mode and never
	// takes the screen exclusively (SS-10). framed (the command line's -w): always a framed window, its client made smaller until the framed window fits the work area.
	IntengStartWindow IntengOptions_StartWindow(IntengScreenSize wanted, int32_t monitorWidth, int32_t monitorHeight, int32_t workWidth, int32_t workHeight, int32_t frameWidth,
		int32_t frameHeight, bool framed);
}
