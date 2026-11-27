// stock_beside_me.h: where a test finds the game's files that the team's runner copied beside its program (tests/<name>.runargs: {FASTFILES} for the whole
// fastfile folder, {FASTFILE:Name} for one file; AvP-docs tools/hicks/run-editor-tests.sh does the copying). Written 2026-11-27 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY. A test that opened a path built from its command line was a Path Traversal to Snyk Code (cpp/PT/test, 2026-11-27): whoever runs the program says
// what it reads. Here the test takes no path at all. Its own folder comes from Windows (the path of its own program), and it opens only fixed names in the
// fastfile folder beside it, as host_png_meta_test opens only a fixed name of its own (6d5b5505). The runner copies the files from its own settings
// ($STOCK_FASTFILE), never links them, so no test can change the game's own files.
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX   // Windows' min and max macros would take the standard library's
#endif
#include <windows.h>

#include <filesystem>
#include <string>

// <the folder this test's program is in>\fastfile, or an empty path when Windows can't say where the program is
inline std::filesystem::path StockFastfileBesideMe()
{
	wchar_t me[MAX_PATH + 1];
	DWORD const got = GetModuleFileNameW(nullptr, me, (DWORD)(sizeof(me) / sizeof(me[0])));

	// 0: no answer; the whole buffer: the path was cut short
	if (got == 0 || got >= sizeof(me) / sizeof(me[0]))
	{
		return std::filesystem::path();
	}

	return std::filesystem::path(std::wstring(me, got)).parent_path() / L"fastfile";
}
