// See dll_load.h. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "dll_load.h"

#include <bcrypt.h>

#include <ctype.h>
#include <string.h>
#include <wchar.h>

#include <vector>

namespace host
{
	namespace
	{
		std::string Narrow(wchar_t const *text)
		{
			std::string out;

			for (size_t i = 0; text && text[i]; i++)
			{
				out += text[i] >= L' ' && text[i] <= L'~' ? (char)text[i] : '?';
			}

			return out;
		}

		void Say(std::string *why, std::string const &words)
		{
			if (why)
			{
				*why = words;
			}
		}

		// Holds a handle, closed when it goes
		struct Held
		{
			HANDLE handle;

			explicit Held(HANDLE h) : handle(h) {}
			~Held()
			{
				if (handle != INVALID_HANDLE_VALUE)
				{
					CloseHandle(handle);
				}
			}
			Held(Held const &) = delete;
			Held &operator=(Held const &) = delete;
		};

		// The file opened as the top of dll_load.h says, or INVALID_HANDLE_VALUE with *why
		HANDLE OpenPlainFile(std::wstring const &path, std::string const &shown, std::string *why)
		{
			HANDLE const file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

			if (file == INVALID_HANDLE_VALUE)
			{
				DWORD const error = GetLastError();

				Say(why, error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? shown + " is not there"
					: (error == ERROR_SHARING_VIOLATION ? shown + " is held open for writing by another program"
					: (error == ERROR_ACCESS_DENIED ? shown + " can't be read (a folder, or no permission)"
					: shown + " can't be opened (Windows error " + std::to_string((unsigned long long)error) + ")")));
				return INVALID_HANDLE_VALUE;
			}

			BY_HANDLE_FILE_INFORMATION info;

			if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandle(file, &info))
			{
				CloseHandle(file);
				Say(why, shown + " is not a file on a disk");
				return INVALID_HANDLE_VALUE;
			}

			if (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
			{
				CloseHandle(file);
				Say(why, shown + " is a folder or a link, not a plain file");
				return INVALID_HANDLE_VALUE;
			}

			uint64_t const size = ((uint64_t)info.nFileSizeHigh << 32) | info.nFileSizeLow;

			if (size == 0 || size > kDllLoadMostBytes)
			{
				CloseHandle(file);
				Say(why, shown + (size == 0 ? " is empty" : " is larger than any DLL the editor ships"));
				return INVALID_HANDLE_VALUE;
			}

			return file;
		}

		bool SameHex(std::string const &a, char const *b)
		{
			if (!b || a.size() != strlen(b))
			{
				return false;
			}

			for (size_t i = 0; i < a.size(); i++)
			{
				char const x = (char)tolower((unsigned char)a[i]);
				char const y = (char)tolower((unsigned char)b[i]);

				if (x != y)
				{
					return false;
				}
			}

			return true;
		}
	}

	bool DllLoad_PlainName(wchar_t const *name)
	{
		if (!name)
		{
			return false;
		}

		size_t const length = wcslen(name);

		if (length <= 4 || length >= 64 || name[0] == L'.' || wcsstr(name, L"..") != nullptr || _wcsicmp(name + length - 4, L".dll") != 0)
		{
			return false;
		}

		for (size_t i = 0; i < length; i++)
		{
			wchar_t const c = name[i];
			bool const plain = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'_' || c == L'-' || c == L'.';

			if (!plain)
			{
				return false;
			}
		}

		return true;
	}

	bool DllLoad_IsSha256Hex(char const *hex)
	{
		if (!hex || strlen(hex) != 64)
		{
			return false;
		}

		for (size_t i = 0; i < 64; i++)
		{
			if (!isxdigit((unsigned char)hex[i]))
			{
				return false;
			}
		}

		return true;
	}

	bool DllLoad_HandleSha256(HANDLE file, std::string *hex, std::string *why)
	{
		if (file == INVALID_HANDLE_VALUE || !file || !hex)
		{
			Say(why, "no file to hash");
			return false;
		}

		LARGE_INTEGER start;

		start.QuadPart = 0;

		if (!SetFilePointerEx(file, start, nullptr, FILE_BEGIN))
		{
			Say(why, "the file can't be read from its start");
			return false;
		}

		BCRYPT_ALG_HANDLE algorithm = nullptr;
		BCRYPT_HASH_HANDLE hash = nullptr;

		if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
		{
			Say(why, "Windows' SHA-256 is not there");
			return false;
		}

		bool ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) == 0;
		std::vector<uint8_t> buffer(64 * 1024);
		uint64_t total = 0;

		while (ok)
		{
			DWORD got = 0;

			if (!ReadFile(file, buffer.data(), (DWORD)buffer.size(), &got, nullptr))
			{
				ok = false;
				break;
			}

			if (got == 0)
			{
				break;
			}

			total += got;
			ok = total <= kDllLoadMostBytes && BCryptHashData(hash, buffer.data(), got, 0) == 0;
		}

		uint8_t digest[32];

		ok = ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0;

		if (hash)
		{
			BCryptDestroyHash(hash);
		}

		BCryptCloseAlgorithmProvider(algorithm, 0);

		if (!ok)
		{
			Say(why, "the file could not be read to its end and hashed");
			return false;
		}

		static char const digits[] = "0123456789abcdef";

		hex->clear();

		for (uint8_t const byte : digest)
		{
			*hex += digits[byte >> 4];
			*hex += digits[byte & 15];
		}

		return true;
	}

	bool DllLoad_FileSha256(wchar_t const *path, std::string *hex, std::string *why)
	{
		if (!path || !path[0])
		{
			Say(why, "no file named");
			return false;
		}

		wchar_t const *const slash = wcsrchr(path, L'\\');
		Held file(OpenPlainFile(path, Narrow(slash ? slash + 1 : path), why));

		return file.handle != INVALID_HANDLE_VALUE && DllLoad_HandleSha256(file.handle, hex, why);
	}

	HMODULE DllLoad_PinnedIn(std::wstring const &folder, wchar_t const *fileName, char const *sha256Hex, std::string *why)
	{
		std::string const shown = Narrow(fileName);

		if (!DllLoad_PlainName(fileName))
		{
			Say(why, "\"" + shown + "\" is not a plain DLL name");
			return nullptr;
		}

		if (!DllLoad_IsSha256Hex(sha256Hex))
		{
			Say(why, shown + " is not loaded: there is no SHA-256 to check it against");
			return nullptr;
		}

		std::wstring root = folder;

		while (!root.empty() && (root.back() == L'\\' || root.back() == L'/'))
		{
			root.pop_back();
		}

		std::wstring const path = root + L"\\" + fileName;

		// a full path the loader can take (LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR wants one): a drive's or a share's
		bool const full = path.size() >= 3 && ((path[1] == L':' && path[2] == L'\\') || (path[0] == L'\\' && path[1] == L'\\'));

		if (root.empty() || !full || path.size() >= MAX_PATH)
		{
			Say(why, shown + " is not loaded: the editor's folder is not a full path it can use");
			return nullptr;
		}

		Held file(OpenPlainFile(path, shown, why));

		if (file.handle == INVALID_HANDLE_VALUE)
		{
			return nullptr;
		}

		std::string hex;

		if (!DllLoad_HandleSha256(file.handle, &hex, why))
		{
			return nullptr;
		}

		if (!SameHex(hex, sha256Hex))
		{
			Say(why, shown + " is not the one this editor was built with (its SHA-256 differs)");
			return nullptr;
		}

		// still held against writers: the file hashed is the file loaded
		HMODULE const module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);

		if (!module)
		{
			Say(why, shown + " would not load (Windows error " + std::to_string((unsigned long long)GetLastError()) + ")");
			return nullptr;
		}

		return module;
	}

	std::wstring DllLoad_OwnFolder()
	{
		std::vector<wchar_t> buffer(MAX_PATH);

		for (int attempt = 0; attempt < 4; attempt++)
		{
			DWORD const got = GetModuleFileNameW(nullptr, buffer.data(), (DWORD)buffer.size());

			if (got == 0)
			{
				return std::wstring();
			}

			if (got < buffer.size())
			{
				std::wstring path(buffer.data(), got);
				size_t const slash = path.find_last_of(L'\\');

				return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
			}

			buffer.resize(buffer.size() * 4);
		}

		return std::wstring();
	}

	HMODULE DllLoad_Pinned(wchar_t const *fileName, char const *sha256Hex, std::string *why)
	{
		return DllLoad_PinnedIn(DllLoad_OwnFolder(), fileName, sha256Hex, why);
	}
}
