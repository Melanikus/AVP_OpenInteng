// The test for "Open with" (src/host/open_with.h): the copy into the data folder's avp_rifs\OpenedByEditor, and the user's own file as it was taken.
// (Vasquez's find F-d, and Frost's review of F-a, SHOULDs 1 and 2, 2026-11-22; Tao 1.)
//   (1) a level file taken: its copy holds its bytes, and OpenWith_StillAsTaken says yes for it (and for any other file: not its business)
//   (2) the user's file changed by another program after the take: no (so its first save doesn't write over that change)
//   (3) taken again: yes again, with the new bytes
//   (4) under that name in OpenedByEditor, a hard link's second name to another file: refused, and that other file left byte for byte
//   (5) OpenedByEditor itself a junction to another folder (mklink /J needs no privilege): refused, and nothing written through it
//   (6) a take that is refused leaves nothing taken: OpenWith_StillAsTaken never compares with an earlier take
// Folders are made in the temp folder and taken away again. Written 2026-11-22 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "open_with.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static std::wstring Wide(std::string const &narrow)
{
	return std::wstring(narrow.begin(), narrow.end());   // the temp folder's path, plain letters
}

static bool Write(std::string const &path, std::string const &text)
{
	FILE *out = fopen(path.c_str(), "wb");

	if (!out)
	{
		return false;
	}

	bool const wrote = fwrite(text.data(), 1, text.size(), out) == text.size();

	return fclose(out) == 0 && wrote;
}

static std::string Read(std::string const &path)
{
	std::string text;
	FILE *in = fopen(path.c_str(), "rb");

	if (in)
	{
		char block[4096];
		size_t got;

		while ((got = fread(block, 1, sizeof(block), in)) > 0)
		{
			text.append(block, got);
		}

		fclose(in);
	}

	return text;
}

static bool Take(std::string const &file, std::string const &data, std::string *why)
{
	char levelName[MAX_PATH] = "";
	char said[512] = "";
	bool const taken = OpenWith_TakeLevel(Wide(file).c_str(), data.c_str(), levelName, sizeof(levelName), said, sizeof(said));

	*why = taken ? std::string(levelName) : std::string(said);
	return taken;
}

int main()
{
	char temp[MAX_PATH];
	DWORD const length = GetTempPathA(MAX_PATH, temp);

	if (length == 0 || length >= MAX_PATH)
	{
		printf("  FAILED  no temp folder: nothing was tested\n");
		return 1;
	}

	std::string const root = std::string(temp) + "open_with_test_" + std::to_string(GetCurrentProcessId());
	std::string const data = root + "\\data", rifs = data + "\\avp_rifs", opened = rifs + "\\OpenedByEditor";
	std::string const mine = root + "\\mine", elsewhere = root + "\\elsewhere";
	std::string const level = mine + "\\Lev.rif", other = mine + "\\Other.rif", place = opened + "\\Lev.rif", victim = elsewhere + "\\victim.rif";
	bool const made = CreateDirectoryA(root.c_str(), NULL) && CreateDirectoryA(data.c_str(), NULL) && CreateDirectoryA(rifs.c_str(), NULL) &&
		CreateDirectoryA(mine.c_str(), NULL) && CreateDirectoryA(elsewhere.c_str(), NULL) && Write(level, "the level as the mapper keeps it") &&
		Write(other, "another level") && Write(victim, "somebody else's file");

	Check(made, "the test's folders and files are made in the temp folder: " + root);

	std::string why;

	// (1)
	bool const first = Take(level, data, &why);

	Check(first && why == "OpenedByEditor/Lev", "(1) the file is taken, the game's name for it OpenedByEditor/Lev (" + why + ")");
	Check(Read(place) == "the level as the mapper keeps it", "(1) its copy in OpenedByEditor holds its bytes");
	Check(OpenWith_StillAsTaken(Wide(level).c_str()), "(1) the file still holds what was taken: yes");
	Check(OpenWith_StillAsTaken(Wide(other).c_str()), "(1) another file: not this rule's to judge, yes");

	// (2)
	Check(Write(level, "the level as ANOTHER program left it"), "(2) another program writes the mapper's file");
	Check(!OpenWith_StillAsTaken(Wide(level).c_str()), "(2) the file is no longer what was taken: no, so its first save doesn't write over that change");

	// (3)
	Check(Take(level, data, &why) && OpenWith_StillAsTaken(Wide(level).c_str()), "(3) taken again: yes, with its new bytes");

	// (4) the editor's own copy's name is a hard link's second name to somebody else's file
	DeleteFileA(place.c_str());
	Check(CreateHardLinkA(place.c_str(), victim.c_str(), NULL) != 0, "(4) under Lev.rif in OpenedByEditor, a second name of another file (CreateHardLinkA)");
	bool const throughLink = Take(level, data, &why);
	Check(!throughLink && why.find("plain file") != std::string::npos, "(4) the take is refused, and says why: " + why);
	Check(Read(victim) == "somebody else's file", "(4) the other file is byte for byte as it was");
	Check(OpenWith_StillAsTaken(Wide(level).c_str()), "(6) a refused take leaves nothing taken: not compared with (3)'s bytes");
	DeleteFileA(place.c_str());

	// (5) OpenedByEditor a junction to another folder
	std::string const copies = elsewhere + "\\copies";
	bool const folderGone = RemoveDirectoryA(opened.c_str()) != 0;
	bool const copiesMade = CreateDirectoryA(copies.c_str(), NULL) != 0;
	std::wstring const command = L"mklink /J \"" + Wide(opened) + L"\" \"" + Wide(copies) + L"\" >nul";
	bool const joined = folderGone && copiesMade && _wsystem(command.c_str()) == 0 && (GetFileAttributesA(opened.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

	Check(joined, "(5) OpenedByEditor is made a junction to another folder (mklink /J)");
	bool const throughJunction = Take(level, data, &why);
	Check(!throughJunction && why.find("OpenedByEditor folder can't be used") != std::string::npos, "(5) the take is refused, and says why: " + why);
	Check(GetFileAttributesA((copies + "\\Lev.rif").c_str()) == INVALID_FILE_ATTRIBUTES, "(5) nothing was written through the junction");

	// taken away again (RemoveDirectory on a junction takes the junction away, never what it points at)
	RemoveDirectoryA(opened.c_str());
	RemoveDirectoryA(copies.c_str());
	DeleteFileA(victim.c_str());
	DeleteFileA(level.c_str());
	DeleteFileA(other.c_str());
	RemoveDirectoryA(elsewhere.c_str());
	RemoveDirectoryA(mine.c_str());
	RemoveDirectoryA(rifs.c_str());
	RemoveDirectoryA(data.c_str());
	Check(RemoveDirectoryA(root.c_str()) != 0, "and the test's folders are taken away");

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
