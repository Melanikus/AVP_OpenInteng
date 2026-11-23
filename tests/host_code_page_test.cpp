// The test for the system's own letters on a PC whose code page is UTF-8 (Windows' "Use Unicode UTF-8 for worldwide language support"). Written 2026-11-23 by
// Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// WHY (Vasquez's lead on 5f8c0b28, Bishop 2's order 11-23: "a unit only, no reboot"): the editor turns wide names into the system's letters with
// WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, ..., &usedDefault) (the level path from the command line, the first launch, the shape extractor, the
// picker's folder list, the preview's player path). Windows documents that for UTF-8 the flags must be 0 or WC_ERR_INVALID_CHARS and lpUsedDefaultChar NULL,
// or the call fails. With the UTF-8 switch on, CP_ACP IS 65001, so the editor's call is this test's call with 65001 named. MEASURED 2026-11-23 on Windows 11
// build 26200 (nothing switched), against the documentation: the editor's form WORKS at 65001, and still says when a letter was stood in for. So this is a
// GUARD of what the editor relies on: a later Windows that starts refusing the editor's form at 65001 turns it red (each call made before its Check):
//   (1) the editor's form at 65001 (WC_NO_BEST_FIT_CHARS + lpUsedDefaultChar): said whole
//   (2) the flag alone, (3) lpUsedDefaultChar alone: said whole
//   (4) WC_ERR_INVALID_CHARS, no lpUsedDefaultChar: said whole
//   (5) the editor's form at this PC's own code page (GetACP), as it runs today: said whole
//   (6) a lone surrogate (a Windows file name may hold one) at 65001, the editor's form: a stand-in made AND lpUsedDefaultChar set, so the editor refuses
//       the name (it never takes a changed name as the same); WC_ERR_INVALID_CHARS refuses it (ERROR_NO_UNICODE_TRANSLATION)
// FIRST WRITTEN to the documentation's words ((1) to (3) failing): 3 FAILED on this Windows, which is how the lead was found not to hold here. Windows 10 is
// not measured.
#include <windows.h>

#include <stdio.h>
#include <string>

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// One conversion's size asked (no output buffer), as the editor's first call asks it: the size given back and GetLastError when it is 0
static int Asked(UINT codePage, DWORD flags, bool askUsed, DWORD *error)
{
	BOOL used = FALSE;

	SetLastError(0);

	int const size = WideCharToMultiByte(codePage, flags, L"C:\\Temp\\level.rif", -1, nullptr, 0, nullptr, askUsed ? &used : nullptr);

	*error = size == 0 ? GetLastError() : 0;
	return size;
}

int main()
{
	printf("host_code_page_test: the editor's conversion to the system's letters on a UTF-8 code page (measured, nothing switched)\n");

	DWORD error = 0;
	int const both = Asked(CP_UTF8, WC_NO_BEST_FIT_CHARS, true, &error);
	DWORD const bothError = error;

	int const whole = (int)sizeof("C:\\Temp\\level.rif");

	Check(both == whole, "(1) the editor's form at 65001 (WC_NO_BEST_FIT_CHARS + lpUsedDefaultChar): said whole, size " + std::to_string(both) + ", error " +
		std::to_string(bothError));

	int const flagOnly = Asked(CP_UTF8, WC_NO_BEST_FIT_CHARS, false, &error);
	DWORD const flagError = error;

	Check(flagOnly == whole, "(2) WC_NO_BEST_FIT_CHARS alone at 65001: said whole, size " + std::to_string(flagOnly) + ", error " + std::to_string(flagError));

	int const usedOnly = Asked(CP_UTF8, 0, true, &error);
	DWORD const usedError = error;

	Check(usedOnly == whole, "(3) lpUsedDefaultChar alone at 65001: said whole, size " + std::to_string(usedOnly) + ", error " + std::to_string(usedError));

	int const taken = Asked(CP_UTF8, WC_ERR_INVALID_CHARS, false, &error);
	DWORD const takenError = error;

	Check(taken == whole, "(4) WC_ERR_INVALID_CHARS, no lpUsedDefaultChar, at 65001: said whole, size " + std::to_string(taken) + ", error " + std::to_string(takenError));

	UINT const own = GetACP();
	int const today = Asked(CP_ACP, WC_NO_BEST_FIT_CHARS, true, &error);
	DWORD const todayError = error;

	Check(today == whole, "(5) the editor's form at this PC's own code page " + std::to_string(own) + ": said whole, size " + std::to_string(today) + ", error " +
		std::to_string(todayError));

	// (6) A NAME UTF-8 CAN'T SAY WHOLE: a lone surrogate (a Windows file name may hold one). The editor asks lpUsedDefaultChar to learn that a letter was stood
	// in for: at 65001 it must still be told (so a changed name is refused, never taken as the same); WC_ERR_INVALID_CHARS refuses it; and this PC's own page
	{
		wchar_t const lone[] = { L'a', (wchar_t)0xD800, L'b', 0 };
		char out[16] = {};
		BOOL used = FALSE;

		SetLastError(0);

		int const made = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, lone, -1, out, (int)sizeof(out), nullptr, &used);
		DWORD const madeError = made == 0 ? GetLastError() : 0;
		std::string hex;

		for (int b = 0; b < made && b < 12; b++)
		{
			char two[4] = {};

			sprintf_s(two, sizeof(two), "%02x ", (unsigned char)out[b]);
			hex += two;
		}

		Check(made > 0 && used, "(6) a lone surrogate at 65001, the editor's form: a stand-in made AND said (size " + std::to_string(made) + ", used " +
			std::to_string((int)used) + ", error " + std::to_string(madeError) + ", bytes " + hex + ")");

		SetLastError(0);

		int const strict = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, lone, -1, nullptr, 0, nullptr, nullptr);
		DWORD const strictError = strict == 0 ? GetLastError() : 0;

		Check(strict == 0 && strictError == ERROR_NO_UNICODE_TRANSLATION, "(6) the same with WC_ERR_INVALID_CHARS: refused (size " + std::to_string(strict) + ", error " +
			std::to_string(strictError) + ")");

		BOOL usedHere = FALSE;
		int const here = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, lone, -1, out, (int)sizeof(out), nullptr, &usedHere);

		Check(here > 0 && usedHere, "(6) the same at this PC's own code page " + std::to_string(GetACP()) + ": a stand-in made and said (size " + std::to_string(here) +
			", used " + std::to_string((int)usedHere) + ")");
	}

	printf(checksFailed ? "host_code_page_test: %d FAILED\n" : "host_code_page_test: all passed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
