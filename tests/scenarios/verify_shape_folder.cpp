// verify_shape_folder: takes a shard's shape_rifs away, as a player who deletes it, and counts the shape files in one (acceptance-shapes-gone-1 and -2).
// Written 2026-11-23 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_shape_folder.exe away <a shard's data copy>               its shape_rifs is renamed shape_rifs-away: never deleted
//      verify_shape_folder.exe count <a shard's data copy> <at least>   its shape_rifs holds at least that many shape files
//      verify_shape_folder.exe none <a shard's data copy>               it has no shape_rifs at all
// A SHAPE FILE is a plain file (never a folder or a link) whose name ends in ".rif" in any capitals, as the editor counts one (shapes_start.h).
// SAFE BY CONSTRUCTION: the folder given must be a shard's data copy, the {datadir} run-shards gives a scenario: its path holds "\shards\" and its own name
// ends in "-data". Its shape_rifs must be a plain folder (never a link or a junction), the new name must be free, and nothing is ever deleted; run-shards
// makes the copy afresh after the scenario's group ("# changes-data"). Named verify_ so run-shards builds it from the tree it runs.
// Gives back 0 only when what was asked was done, or holds.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <windows.h>

namespace
{
	int Failed(std::string const &why)
	{
		printf("FAILED verify_shape_folder: %s\n", why.c_str());
		return 1;
	}

	std::wstring Wide(std::string const &narrow)
	{
		int const length = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, narrow.c_str(), -1, nullptr, 0);

		if (length <= 0)
		{
			return std::wstring();
		}

		std::wstring wide((size_t)length - 1, L'\0');
		MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, narrow.c_str(), -1, &wide[0], length);
		return wide;
	}

	// the folder given, its slashes Windows' own and no last one; "" when it isn't a shard's data copy (or names a "." or ".." part)
	std::string ShardDataCopy(std::string given)
	{
		for (char &c : given)
		{
			c = c == '/' ? '\\' : c;
		}

		while (!given.empty() && given.back() == '\\')
		{
			given.pop_back();
		}

		std::string lower = given;

		for (char &c : lower)
		{
			c = (char)tolower((unsigned char)c);
		}

		bool const dots = lower.find("\\.\\") != std::string::npos || lower.find("\\..\\") != std::string::npos ||
			(lower.size() >= 2 && lower.compare(lower.size() - 2, 2, "\\.") == 0) || (lower.size() >= 3 && lower.compare(lower.size() - 3, 3, "\\..") == 0);
		bool const shard = lower.find("\\shards\\") != std::string::npos && lower.size() > 5 && lower.compare(lower.size() - 5, 5, "-data") == 0;

		return !dots && shard ? given : std::string();
	}

	// "folder", "link", "missing" or "file": what the path is
	char const *KindOf(std::wstring const &path)
	{
		DWORD const attributes = GetFileAttributesW(path.c_str());

		if (attributes == INVALID_FILE_ATTRIBUTES)
		{
			return "missing";
		}

		if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
		{
			return "link";
		}

		return (attributes & FILE_ATTRIBUTE_DIRECTORY) ? "folder" : "file";
	}

	// the shape files in a folder, as the editor counts them; -1 when it can't be listed
	long long ShapeFiles(std::wstring const &folder)
	{
		WIN32_FIND_DATAW found;
		HANDLE const search = FindFirstFileExW((folder + L"\\*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, 0);

		if (search == INVALID_HANDLE_VALUE)
		{
			return -1;
		}

		long long count = 0;

		do
		{
			size_t const length = wcslen(found.cFileName);
			bool const plain = !(found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));

			count += plain && length > 4 && _wcsicmp(found.cFileName + length - 4, L".rif") == 0 ? 1 : 0;
		} while (FindNextFileW(search, &found));

		FindClose(search);
		return count;
	}
}

int main(int argc, char **argv)
{
	std::string const verb = argc >= 3 ? argv[1] : "";
	std::string const data = argc >= 3 ? ShardDataCopy(argv[2]) : "";

	if ((verb != "away" && verb != "count" && verb != "none") || (verb == "count" && argc != 4) || (verb != "count" && argc != 3))
	{
		return Failed("use: away <a shard's data copy> | count <a shard's data copy> <at least> | none <a shard's data copy>");
	}

	if (data.empty())
	{
		return Failed("\"" + std::string(argv[2]) + "\" isn't a shard's data copy (a path holding \\shards\\ and named ...-data), so nothing is done");
	}

	std::wstring const shapes = Wide(data + "\\shape_rifs");

	if (shapes.empty())
	{
		return Failed("the folder's name can't be said in Windows' code page");
	}

	std::string const kind = KindOf(shapes);

	if (verb == "none")
	{
		if (kind != "missing")
		{
			return Failed(data + "\\shape_rifs is there (a " + kind + ")");
		}

		printf("ok verify_shape_folder: %s holds no shape_rifs\n", data.c_str());
		return 0;
	}

	if (kind != "folder")
	{
		return Failed(data + "\\shape_rifs isn't a plain folder (it is " + kind + ")");
	}

	if (verb == "count")
	{
		char *end = nullptr;
		long long const most = 1000000;
		long long const atLeast = strtoll(argv[3], &end, 10);

		if (!end || *end != '\0' || end == argv[3] || atLeast < 0 || atLeast > most)
		{
			return Failed("\"" + std::string(argv[3]) + "\" isn't a count from 0 to 1000000");
		}

		long long const count = ShapeFiles(shapes);

		if (count < 0)
		{
			return Failed(data + "\\shape_rifs can't be listed (Windows error " + std::to_string((unsigned long)GetLastError()) + ")");
		}

		if (count < atLeast)
		{
			return Failed(data + "\\shape_rifs holds " + std::to_string(count) + " shape files, fewer than " + std::to_string(atLeast));
		}

		printf("ok verify_shape_folder: %s\\shape_rifs holds %lld shape files (at least %lld)\n", data.c_str(), count, atLeast);
		return 0;
	}

	// away: renamed, never deleted, and only onto a name that is free
	std::wstring const away = Wide(data + "\\shape_rifs-away");

	if (std::string(KindOf(away)) != "missing")
	{
		return Failed(data + "\\shape_rifs-away is there already, so nothing is moved");
	}

	if (!MoveFileExW(shapes.c_str(), away.c_str(), 0))
	{
		return Failed("shape_rifs couldn't be renamed (Windows error " + std::to_string((unsigned long)GetLastError()) + ")");
	}

	printf("ok verify_shape_folder: %s\\shape_rifs is now shape_rifs-away, as a player who deleted it\n", data.c_str());
	return 0;
}
