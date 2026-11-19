// host_dll_load_test.cpp: the pinned DLL loader (src/host/dll_load.h) on a folder of its own: the plain-name and hash rules, SHA-256 on known bytes, and a real load of a renamed
// copy of a small Windows DLL (msimg32.dll: it exports AlphaBlend) only when its hash is the expected one; refused when the hash differs, the file is missing, a folder, empty, or
// held open by a writer. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include <string>

#include "dll_load.h"
#include "dll_pins.h"

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

static bool WriteBytes(std::wstring const &path, char const *bytes, size_t size)
{
	HANDLE const file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD wrote = 0;
	bool const ok = size == 0 || (WriteFile(file, bytes, (DWORD)size, &wrote, nullptr) && wrote == size);

	CloseHandle(file);
	return ok;
}

int main()
{
	printf("THE RULES\n");

	Check(host::DllLoad_PlainName(L"libsmackerdec.dll") && host::DllLoad_PlainName(L"libsmackerenc.DLL") && host::DllLoad_PlainName(L"a-b_c.1.dll"), "plain names are taken");

	for (wchar_t const *bad : { L"", L".dll", L"x.exe", L"..\\x.dll", L"a\\b.dll", L"a/b.dll", L"c:x.dll", L"x.dll:stream", L"x..dll", L".hidden.dll", L"x y.dll", L"x.dll.", L"\\\\?\\c:\\x.dll" })
	{
		std::string shown;

		for (wchar_t const *c = bad; *c; c++)
		{
			shown += (char)*c;
		}

		Check(!host::DllLoad_PlainName(bad), "\"" + shown + "\" is not a plain DLL name");
	}

	Check(!host::DllLoad_PlainName(nullptr), "no name is not a plain name");
	Check(!host::DllLoad_PlainName((std::wstring(60, L'a') + L".dll").c_str()), "a name of 64 letters is refused");

	char const *const abcHash = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

	Check(host::DllLoad_IsSha256Hex(abcHash) && host::DllLoad_IsSha256Hex("BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"), "64 hex digits, any capitals");
	Check(!host::DllLoad_IsSha256Hex("") && !host::DllLoad_IsSha256Hex(nullptr) && !host::DllLoad_IsSha256Hex(std::string(abcHash).substr(1).c_str()) &&
		!host::DllLoad_IsSha256Hex((std::string(abcHash) + "0").c_str()) && !host::DllLoad_IsSha256Hex((std::string(abcHash).substr(1) + "g").c_str()), "63, 65, a g, \"\" and none are refused");
	Check(strcmp(host::kDllPinSmackerDecoder, "") == 0 && strcmp(host::kDllPinSmackerEncoder, "") == 0, "a build given no pins pins none: \"\" for both");

	// a pin as the .vcxproj passes it, an x then the bare hex: made the same string whether the hex starts with a letter or a digit
#define TEST_PIN_LETTER xba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
#define TEST_PIN_DIGIT x0a7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
#define TEST_PIN_NONE x
	char const *const fromLetter = DLL_PIN_STRING(TEST_PIN_LETTER) + 1;
	char const *const fromDigit = DLL_PIN_STRING(TEST_PIN_DIGIT) + 1;

	Check(strcmp(DLL_PIN_STRING(TEST_PIN_NONE) + 1, "") == 0, "x alone (the build was given no hash): \"\"");

	Check(strcmp(fromLetter, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0, "a pin starting with a letter, as dll_pins.h makes it: " + std::string(fromLetter));
	Check(strcmp(fromDigit, "0a7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0 && host::DllLoad_IsSha256Hex(fromDigit), "and one starting with a digit: " + std::string(fromDigit));

	printf("SHA-256 AND THE LOAD\n");

	wchar_t temp[MAX_PATH];
	DWORD const tempLength = GetTempPathW(MAX_PATH, temp);

	if (tempLength == 0 || tempLength >= MAX_PATH)
	{
		printf("FAILED host_dll_load_test: no temporary folder\n");
		return 1;
	}

	std::wstring const folder = std::wstring(temp) + L"host_dll_load_test_" + std::to_wstring((unsigned long long)GetCurrentProcessId());

	CreateDirectoryW(folder.c_str(), nullptr);

	std::string hex, why;

	Check(WriteBytes(folder + L"\\abc.bin", "abc", 3) && host::DllLoad_FileSha256((folder + L"\\abc.bin").c_str(), &hex, &why) && hex == abcHash, "SHA-256 of \"abc\" (FIPS 180-2's example): " + hex);

	// a small Windows DLL, copied under a name of its own (System32 is the 32-bit one for this 32-bit test, as Windows redirects it)
	wchar_t system[MAX_PATH];
	UINT const systemLength = GetSystemDirectoryW(system, MAX_PATH);
	std::wstring const dll = folder + L"\\pinned_test.dll";
	bool const copied = systemLength > 0 && systemLength < MAX_PATH && CopyFileW((std::wstring(system) + L"\\msimg32.dll").c_str(), dll.c_str(), FALSE);

	Check(copied, "a copy of msimg32.dll made as pinned_test.dll");

	std::string dllHash;

	Check(host::DllLoad_FileSha256(dll.c_str(), &dllHash, &why) && host::DllLoad_IsSha256Hex(dllHash.c_str()), "its SHA-256: " + dllHash);

	std::string wrong = dllHash;

	wrong[0] = wrong[0] == '0' ? '1' : '0';

	HMODULE module = host::DllLoad_PinnedIn(folder, L"pinned_test.dll", wrong.c_str(), &why);

	Check(!module && why == "pinned_test.dll is not the one this editor was built with (its SHA-256 differs)", "a hash one digit off: NOT loaded (" + why + ")");
	Check(GetModuleHandleW(L"pinned_test.dll") == nullptr, "and nothing of it is in the process");

	module = host::DllLoad_PinnedIn(folder, L"pinned_test.dll", "", &why);
	Check(!module && why == "pinned_test.dll is not loaded: there is no SHA-256 to check it against", "no pin: NOT loaded (" + why + ")");

	// held open by a writer: refused, never loaded
	HANDLE const writer = CreateFileW(dll.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	module = host::DllLoad_PinnedIn(folder, L"pinned_test.dll", dllHash.c_str(), &why);
	Check(writer != INVALID_HANDLE_VALUE && !module && why == "pinned_test.dll is held open for writing by another program", "a file held by a writer: NOT loaded (" + why + ")");

	if (writer != INVALID_HANDLE_VALUE)
	{
		CloseHandle(writer);
	}

	// the right hash, any capitals: loaded, from that folder, and it works
	std::string upper = dllHash;

	for (char &c : upper)
	{
		c = (char)toupper((unsigned char)c);
	}

	module = host::DllLoad_PinnedIn(folder + L"\\", L"pinned_test.dll", upper.c_str(), &why);
	Check(module != nullptr, "the right hash (in capitals): loaded" + (module ? std::string() : " (" + why + ")"));

	if (module)
	{
		wchar_t loadedFrom[MAX_PATH];
		DWORD const got = GetModuleFileNameW(module, loadedFrom, MAX_PATH);

		Check(got > 0 && _wcsicmp(loadedFrom, dll.c_str()) == 0, "from exactly that file");
		Check(GetProcAddress(module, "AlphaBlend") != nullptr, "and its AlphaBlend is there");

		// while it is loaded the file can't be written over
		HANDLE const over = CreateFileW(dll.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		Check(over == INVALID_HANDLE_VALUE, "a loaded DLL's file can't be opened for writing (Windows holds it)");

		if (over != INVALID_HANDLE_VALUE)
		{
			CloseHandle(over);
		}

		FreeLibrary(module);
	}

	// missing, a folder, empty, a name that isn't plain
	module = host::DllLoad_PinnedIn(folder, L"missing.dll", dllHash.c_str(), &why);
	Check(!module && why == "missing.dll is not there", "a missing file: " + why);

	CreateDirectoryW((folder + L"\\folder.dll").c_str(), nullptr);
	module = host::DllLoad_PinnedIn(folder, L"folder.dll", dllHash.c_str(), &why);
	Check(!module && why == "folder.dll can't be read (a folder, or no permission)", "a folder named folder.dll: " + why);

	WriteBytes(folder + L"\\empty.dll", "", 0);
	module = host::DllLoad_PinnedIn(folder, L"empty.dll", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", &why);
	Check(!module && why == "empty.dll is empty", "an empty file, even with the empty file's hash: " + why);

	module = host::DllLoad_PinnedIn(folder, L"..\\pinned_test.dll", dllHash.c_str(), &why);
	Check(!module && why == "\"..\\pinned_test.dll\" is not a plain DLL name", "a name with ..: " + why);

	module = host::DllLoad_PinnedIn(L"relative", L"pinned_test.dll", dllHash.c_str(), &why);
	Check(!module && why == "pinned_test.dll is not loaded: the editor's folder is not a full path it can use", "a folder that is not a full path: " + why);

	Check(!host::DllLoad_OwnFolder().empty(), "the test's own folder is known");

	// the test folder goes
	for (wchar_t const *name : { L"abc.bin", L"pinned_test.dll", L"empty.dll" })
	{
		DeleteFileW((folder + L"\\" + name).c_str());
	}

	RemoveDirectoryW((folder + L"\\folder.dll").c_str());
	RemoveDirectoryW(folder.c_str());

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed != 0)
	{
		printf("FAILED host_dll_load_test\n");
		return 1;
	}

	printf("passed host_dll_load_test\n");
	return 0;
}
