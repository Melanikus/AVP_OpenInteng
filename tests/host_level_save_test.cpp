// The test of Save over the opened file with a backup (src/host/level_save.cpp; THE OWNER'S RULING, Melanikus 2026-11-16: "Overwrite, with backup"). Written 2026-11-16 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_level_save_test.exe <Invasion .rif>. The stock level is only copied: every file written is in a folder of the test's own, host_level_save_test_<process id> in the folder
// it runs in (it stops if that folder is already there), removed at the end; no path from the command line is opened or written to (Snyk Code, Path Traversal, 2026-11-17). (a COPY of the level is the "opened" file).
// THE CASES, one sitting's saves (LevelSaveSitting; Bishop's ruling B: the first save of a sitting keeps the file as .bak, later ones leave it): (1) gARAGDOR's links cut to 6, saved over
// the copy: the copy loads with 6, its .bak is the stock file byte for byte, no temporary file is left; (2) cut to 5 and saved again in the same sitting: the copy has 5 and the .bak is
// STILL the stock file byte for byte (the level as it was opened); (2b) a NEW sitting's first save of that file: its .bak is the file as that sitting found it (the 5), an older .bak replaced;
// (3) a READ-ONLY file is refused and left as it was; (4) a file that is not there yet is made, with no .bak, and saved again in the sitting still with none; (5) not a .rif: refused.
// THE FILE SAVED OVER IS ITSELF CHECKED (Vasquez's find F-a, 2026-11-22; Bishop 2's order, Tao 1), each one REFUSED with the file byte for byte as it was: (6) a file another program
// changed while the level was open; (7) one changed after the level was saved to it; (8) a file with a second name (a hard link), and the view gap's copy with one; (9) a file made a
// link (a reparse point); (10) a symbolic link to another file, where this computer can make one; (11) the save's last look before the replace (LevelFile_StillThese). RED on d2d2d909 (the
// base, with only this test changed): (6), (7), (8) and (8)'s view copy were written over, 8 checks failed; (9) was refused there only because ReplaceFileA failed on the made-up link
// kind (Windows error 1920), with no check of its own; (10) is not tried on a computer that can't make a symbolic link.
// (12) EVERY PLACED OBJECT IN THE VIEW (Vasquez 2026-11-25, the owner's "Every object, always"): LevelSave_ViewFlags' rule; LevelSave_EveryPlacedObjectIn over a made level (only the
// placed object's flags change) and over six hostile ones (each left byte for byte); the view's own copy of the stock level (11 objects' flags changed, every placed object at the
// rule's flags, pulse1 at Medium) while the level held, the opened file and the working bytes' copy keep every true flag; a level with nothing to change writes nothing; the
// harness's list of placed objects. RED: host_level_save_test does not build on the base, whose level_save.h has none of these.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include <windows.h>
#include <winioctl.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "level_save.h"
#include "module_links.h"
#include "object_edit.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// what the game's chunk code calls
HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char *message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

static std::vector<uint8_t> Bytes(std::string const &path)
{
	std::vector<uint8_t> out;
	FILE *in = fopen(path.c_str(), "rb");

	if (in)
	{
		uint8_t block[65536];
		size_t got;

		while ((got = fread(block, 1, sizeof(block), in)) > 0)
		{
			out.insert(out.end(), block, block + got);
		}

		fclose(in);
	}

	return out;
}

// Bytes written over a file, as another program would write it
static bool WriteBytes(std::string const &path, std::vector<uint8_t> const &bytes)
{
	FILE *out = fopen(path.c_str(), "wb");

	if (!out)
	{
		return false;
	}

	bool const wrote = fwrite(bytes.data(), 1, bytes.size(), out) == bytes.size();

	return fclose(out) == 0 && wrote;
}

// A file made a link of a made-up kind (tag 0x00001234), or made a plain file again. Any user may set such a reparse point: only a symbolic link needs the privilege
// (host_video_folder_links_test does the same to a folder)
static bool TagAsLink(std::string const &path, bool on)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	REPARSE_GUID_DATA_BUFFER point = {};
	DWORD returned = 0;

	point.ReparseTag = 0x00001234ul;
	point.ReparseDataLength = 0;
	point.ReparseGuid = { 0x6d0c1b5e, 0x2f4a, 0x4c7e, { 0x9a, 0x51, 0x3b, 0x8e, 0x22, 0x10, 0x7f, 0x44 } };

	BOOL const done = DeviceIoControl(file, on ? FSCTL_SET_REPARSE_POINT : FSCTL_DELETE_REPARSE_POINT, &point, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE, nullptr, 0, &returned, nullptr);

	CloseHandle(file);
	return done != 0;
}

// The .bak and the temporary file of a save over path are not there
static bool NoBackupNorTemporary(std::string const &path)
{
	return GetFileAttributesA((path + ".bak").c_str()) == INVALID_FILE_ATTRIBUTES && GetFileAttributesA((path + ".saving").c_str()) == INVALID_FILE_ATTRIBUTES;
}

// gARAGDOR's link count in a level file, -1 when it does not load
static int LinksIn(std::string const &path, int32_t garage)
{
	OpenLevelFile level;
	std::vector<host::LinkItem> items;

	if (level.Open(path.c_str()) != LevelOpenResult::Opened || !level.Level() || !host::ReadModuleLinks(level.Level(), garage, &items))
	{
		return -1;
	}

	return (int)items.size();
}

// gARAGDOR's links cut to their first `keep` items (no branch reaches past them), through the editor's own SetModuleLinks
static bool CutLinks(OpenLevelFile *open, int32_t garage, size_t keep)
{
	std::vector<host::LinkItem> items;
	uint8_t id[8];

	if (!host::ReadModuleLinks(open->Level(), garage, &items) || items.size() < keep || !host::ModuleIdOfIndex(open->Level(), garage, id))
	{
		return false;
	}

	std::vector<host::ModuleLinkItem> kept;

	for (size_t i = 0; i < keep; i++)
	{
		host::ModuleLinkItem const one = { items[i].objectIndex, items[i].branchNo > (int32_t)keep ? (int32_t)keep : items[i].branchNo, items[i].flags, items[i].spare };

		kept.push_back(one);
	}

	return host::SetModuleLinks(open, id, kept.data(), kept.size()) == host::ObjectEditResult::Done;
}

// Every file in the test's own folder deleted (read-only ones too), then the folder: the names come from the folder itself
static void RemoveTheFolder(std::string const &folder)
{
	WIN32_FIND_DATAA found;
	HANDLE const walk = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (walk != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				std::string const file = folder + "\\" + found.cFileName;
				SetFileAttributesA(file.c_str(), FILE_ATTRIBUTE_NORMAL);
				DeleteFileA(file.c_str());
			}
		} while (FindNextFileA(walk, &found));

		FindClose(walk);
	}

	RemoveDirectoryA(folder.c_str());
}

static void RunTheCases(std::string const &stock, std::string const &folder);
static void EveryPlacedObjectCases(std::string const &stock, std::string const &folder);

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("  FAILED  usage: host_level_save_test <Invasion .rif>\n");
		return 1;
	}

	// the test's own folder, in the folder it runs in, with a name it makes itself
	std::string const folder = "host_level_save_test_" + std::to_string(GetCurrentProcessId());

	if (GetFileAttributesA(folder.c_str()) != INVALID_FILE_ATTRIBUTES || !CreateDirectoryA(folder.c_str(), nullptr))
	{
		printf("  FAILED  the test's folder %s is already there, or can't be made\n", folder.c_str());
		return 1;
	}

	RunTheCases(argv[1], folder);   // the level it opened is closed when this returns, so the folder can go
	RemoveTheFolder(folder);
	Check(GetFileAttributesA(folder.c_str()) == INVALID_FILE_ATTRIBUTES, "the test's folder is gone at the end");

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}

static void RunTheCases(std::string const &stock, std::string const &folder)
{
	std::string const target = folder + "\\Saved-over.rif", backup = target + ".bak", temporary = target + ".saving";

	Check(CopyFileA(stock.c_str(), target.c_str(), FALSE) != 0, "the 'opened' file is a copy of the stock level: " + target);

	// the level as it was opened, read from the copy (the stock file itself is only ever copied)
	std::vector<uint8_t> const asOpened = Bytes(target);

	Check(!asOpened.empty(), "the copy has the level's bytes: " + std::to_string(asOpened.size()));

	// a temporary file left by an earlier, broken save is not in the way
	{
		FILE *junk = fopen(temporary.c_str(), "wb");

		if (junk)
		{
			fputs("junk", junk);
			fclose(junk);
		}
	}

	OpenLevelFile open;

	Check(open.Open(target.c_str()) == LevelOpenResult::Opened && open.Level(), "it opens");

	int32_t garage = 54;   // gARAGDOR's object number on Invasion (host_autolink_file_test)

	Check(LinksIn(target, garage) == 7, "gARAGDOR has its 7 stock links in it");

	// ---- (1) a change, saved over the file
	std::string backupPath, why;
	host::LevelSaveSitting sitting;

	Check(!sitting.SavedBefore(target.c_str()), "a new sitting has saved nothing");
	Check(CutLinks(&open, garage, 6), "gARAGDOR's links cut to 6 in the editor's bytes");
	host::LevelSaveOver const first = sitting.Save(&open, target.c_str(), &backupPath, &why);

	Check(first == host::LevelSaveOver::Saved && backupPath == backup, "saved over the file, the one before kept as .bak: " + backupPath);
	Check(LinksIn(target, garage) == 6, "the file loads with the change: " + std::to_string(LinksIn(target, garage)) + " links");
	Check(Bytes(backup) == asOpened, "the .bak is the file as it was, byte for byte");
	Check(GetFileAttributesA(temporary.c_str()) == INVALID_FILE_ATTRIBUTES, "no temporary file is left");

	// ---- (2) another change, saved again in the same sitting: the .bak is still the level as it was opened
	bool const cut5 = CutLinks(&open, garage, 5);

	// ---- (1b) the view gap's copy of the working bytes (ShowTheWorkingBytes) between an edit and its save: written, loading with the edit, and NOT a save (the open level is as it was)
	{
		std::string const view = folder + "\\Viewed.rif";
		std::vector<uint8_t> const fileBefore = Bytes(target);

		DeleteFileA(view.c_str());
		DeleteFileA((view + ".bak").c_str());
		Check(open.HasChanges(), "before the view copy the open level has a change not saved");

		host::LevelSaveOver const copied = host::LevelSave_ViewCopy(&open, view.c_str(), &why);
		host::LevelSaveOver const again = host::LevelSave_ViewCopy(&open, view.c_str(), &why);

		Check(copied == host::LevelSaveOver::Saved && again == host::LevelSaveOver::Saved, "the view copy is written, and written over again: " + why);
		Check(LinksIn(view, garage) == 5, "the view copy loads with the edit: " + std::to_string(LinksIn(view, garage)) + " links");
		Check(GetFileAttributesA((view + ".bak").c_str()) == INVALID_FILE_ATTRIBUTES && GetFileAttributesA((view + ".saving").c_str()) == INVALID_FILE_ATTRIBUTES,
			"with no .bak and no temporary file");
		Check(open.HasChanges(), "and the open level STILL has its change not saved (a view copy is not a save)");
		Check(Bytes(target) == fileBefore, "the opened file is untouched by the view copy");
		Check(host::LevelSave_ViewCopy(&open, (folder + "\\level.txt").c_str(), &why) == host::LevelSaveOver::BadTarget, "a view copy to a path that is not a .rif is refused");
	}

	std::string const sameCapitals = folder + "\\SAVED-OVER.RIF";   // the same file, written another way
	host::LevelSaveOver const second = sitting.Save(&open, sameCapitals.c_str(), &backupPath, &why);

	Check(cut5 && second == host::LevelSaveOver::Saved && sitting.SavedBefore(target.c_str()), "saved again in the same sitting (the path written with other capitals)");
	Check(LinksIn(target, garage) == 5, "the file has the 5: " + std::to_string(LinksIn(target, garage)));
	Check(Bytes(backup) == asOpened, "and its .bak is STILL the level as it was opened, byte for byte (two saves in a sitting, one .bak)");
	Check(_stricmp(backupPath.c_str(), backup.c_str()) == 0, "the words name the .bak the sitting kept: " + backupPath);
	Check(GetFileAttributesA(temporary.c_str()) == INVALID_FILE_ATTRIBUTES, "no temporary file is left");

	// ---- (2b) a new sitting: its first save keeps the file as that sitting found it (the 5), the older .bak replaced
	host::LevelSaveSitting nextSitting;
	bool const cut4 = CutLinks(&open, garage, 4);
	host::LevelSaveOver const next = nextSitting.Save(&open, target.c_str(), &backupPath, &why);

	Check(cut4 && next == host::LevelSaveOver::Saved && backupPath == backup, "a new sitting's first save");
	Check(LinksIn(target, garage) == 4 && LinksIn(backup, garage) == 5, "the file has 4, its .bak the 5 this sitting found: " + std::to_string(LinksIn(target, garage)) + " / " +
		std::to_string(LinksIn(backup, garage)));

	// ---- (3) read-only: refused, untouched
	std::vector<uint8_t> const before = Bytes(target);

	SetFileAttributesA(target.c_str(), FILE_ATTRIBUTE_READONLY);
	bool const cut3 = CutLinks(&open, garage, 3);
	host::LevelSaveOver const third = nextSitting.Save(&open, target.c_str(), &backupPath, &why);

	Check(cut3 && third == host::LevelSaveOver::ReadOnly && backupPath.empty(), "a read-only file is refused: " + why);
	SetFileAttributesA(target.c_str(), FILE_ATTRIBUTE_NORMAL);
	Check(Bytes(target) == before && GetFileAttributesA(temporary.c_str()) == INVALID_FILE_ATTRIBUTES, "and is as it was, with no temporary file");

	// ---- (4) a file not there yet is made, with no .bak
	std::string const fresh = folder + "\\Saved-new.rif";

	DeleteFileA(fresh.c_str());
	DeleteFileA((fresh + ".bak").c_str());
	host::LevelSaveOver const fourth = nextSitting.Save(&open, fresh.c_str(), &backupPath, &why);

	Check(fourth == host::LevelSaveOver::Saved && backupPath.empty() && LinksIn(fresh, garage) == 3 && GetFileAttributesA((fresh + ".bak").c_str()) == INVALID_FILE_ATTRIBUTES,
		"a file not there yet is made, with no .bak");

	bool const cut2 = CutLinks(&open, garage, 2);
	host::LevelSaveOver const fifth = nextSitting.Save(&open, fresh.c_str(), &backupPath, &why);

	Check(cut2 && fifth == host::LevelSaveOver::Saved && backupPath.empty() && LinksIn(fresh, garage) == 2 && GetFileAttributesA((fresh + ".bak").c_str()) == INVALID_FILE_ATTRIBUTES,
		"saved again in the sitting: still no .bak (there was no level before the sitting)");

	// ---- (5) not a .rif
	Check(nextSitting.Save(&open, (folder + "\\level.txt").c_str(), &backupPath, &why) == host::LevelSaveOver::BadTarget, "a path that is not a .rif is refused");

	// ---- (6) to (10): THE FILE SAVED OVER IS ITSELF CHECKED, not only its temporary file (header). Each case is a level of its own, opened from a copy of the stock level (asOpened is
	// the stock level's bytes), changed, then saved, and each must be REFUSED with the file byte for byte as it was.

	// ---- (6) the file was changed on disk by another program while the level was open
	{
		std::string const changed = folder + "\\Changed.rif";
		OpenLevelFile level;

		Check(CopyFileA(stock.c_str(), changed.c_str(), FALSE) != 0 && level.Open(changed.c_str()) == LevelOpenResult::Opened && level.Level(), "(6) a copy of the stock level opens");

		std::vector<uint8_t> outside = asOpened;

		if (!outside.empty())
		{
			outside.back() ^= 1;
		}

		Check(WriteBytes(changed, outside), "(6) another program writes the file while it is open (its last byte changed)");

		host::LevelSaveSitting own;
		bool const cut = CutLinks(&level, garage, 6);
		host::LevelSaveOver const result = own.Save(&level, changed.c_str(), &backupPath, &why);

		Check(cut && result != host::LevelSaveOver::Saved && why.find("changed on disk") != std::string::npos, "(6) it is NOT saved over, and the words say why: " + why);
		Check(Bytes(changed) == outside && NoBackupNorTemporary(changed), "(6) the file still holds the other program's bytes, with no .bak and no temporary file");
	}

	// ---- (7) changed on disk after the level was saved to it: from that save on, the level stands for the file it was saved to (the real one, not its temporary file)
	{
		std::string const savedTo = folder + "\\Saved-then-changed.rif";
		OpenLevelFile level;
		host::LevelSaveSitting own;

		Check(CopyFileA(stock.c_str(), savedTo.c_str(), FALSE) != 0 && level.Open(savedTo.c_str()) == LevelOpenResult::Opened && level.Level(), "(7) a copy of the stock level opens");

		bool const cut6 = CutLinks(&level, garage, 6);
		host::LevelSaveOver const first7 = own.Save(&level, savedTo.c_str(), &backupPath, &why);

		Check(cut6 && first7 == host::LevelSaveOver::Saved && LinksIn(savedTo, garage) == 6, "(7) a change is saved over it: " + why);

		std::vector<uint8_t> outside = Bytes(savedTo);

		if (!outside.empty())
		{
			outside.back() ^= 1;
		}

		Check(WriteBytes(savedTo, outside), "(7) then another program writes the file (its last byte changed)");

		bool const cut5 = CutLinks(&level, garage, 5);
		host::LevelSaveOver const second7 = own.Save(&level, savedTo.c_str(), &backupPath, &why);

		Check(cut5 && second7 != host::LevelSaveOver::Saved && why.find("changed on disk") != std::string::npos, "(7) the next save is NOT made, and the words say why: " + why);
		Check(Bytes(savedTo) == outside && GetFileAttributesA((savedTo + ".saving").c_str()) == INVALID_FILE_ATTRIBUTES, "(7) the file still holds the other program's bytes");
	}

	// ---- (8) the file has a second name (a hard link): writing over one name leaves the other as it was, two files where the mapper sees one
	{
		std::string const linked = folder + "\\Linked.rif", other = folder + "\\Linked-other-name.rif";
		OpenLevelFile level;

		Check(CopyFileA(stock.c_str(), linked.c_str(), FALSE) != 0 && level.Open(linked.c_str()) == LevelOpenResult::Opened && level.Level(), "(8) a copy of the stock level opens");
		Check(CreateHardLinkA(other.c_str(), linked.c_str(), nullptr) != 0, "(8) the file is given a second name (CreateHardLinkA, which any user may do)");

		host::LevelSaveSitting own;
		bool const cut = CutLinks(&level, garage, 6);
		host::LevelSaveOver const result = own.Save(&level, linked.c_str(), &backupPath, &why);

		Check(cut && result != host::LevelSaveOver::Saved && why.find("plain file") != std::string::npos, "(8) it is NOT saved over, and the words say why: " + why);
		Check(Bytes(linked) == asOpened && Bytes(other) == asOpened && NoBackupNorTemporary(linked), "(8) both names still hold the stock bytes, with no .bak and no temporary file");

		// the view gap's copy is held to the same rule: given a second name, it is not written over either
		std::string const view = folder + "\\Viewed-linked.rif", viewOther = folder + "\\Viewed-other-name.rif";
		host::LevelSaveOver const made = host::LevelSave_ViewCopy(&level, view.c_str(), &why);
		std::vector<uint8_t> const viewBytes = Bytes(view);

		Check(made == host::LevelSaveOver::Saved && CreateHardLinkA(viewOther.c_str(), view.c_str(), nullptr) != 0, "(8) a view copy is written, then given a second name: " + why);

		bool const cutView = CutLinks(&level, garage, 5);   // (so a copy written over it would differ from the one there)
		host::LevelSaveOver const again = host::LevelSave_ViewCopy(&level, view.c_str(), &why);

		Check(cutView && again != host::LevelSaveOver::Saved && why.find("plain file") != std::string::npos, "(8) the view copy is NOT written over it, and the words say why: " + why);
		Check(Bytes(view) == viewBytes && Bytes(viewOther) == viewBytes, "(8) both of its names hold the copy as it was");
	}

	// ---- (9) the file is made a link (a reparse point) after the level was opened from it
	{
		std::string const tagged = folder + "\\Tagged.rif";
		OpenLevelFile level;

		Check(CopyFileA(stock.c_str(), tagged.c_str(), FALSE) != 0 && level.Open(tagged.c_str()) == LevelOpenResult::Opened && level.Level(), "(9) a copy of the stock level opens");

		bool const linkMade = TagAsLink(tagged, true);

		Check(linkMade && (GetFileAttributesA(tagged.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0, "(9) the file is made a link of a made-up kind (0x00001234)");

		host::LevelSaveSitting own;
		bool const cut = CutLinks(&level, garage, 6);
		host::LevelSaveOver const result = own.Save(&level, tagged.c_str(), &backupPath, &why);

		Check(cut && result != host::LevelSaveOver::Saved && why.find("plain file") != std::string::npos, "(9) it is NOT saved over, and the words say why: " + why);
		Check(TagAsLink(tagged, false) && Bytes(tagged) == asOpened && NoBackupNorTemporary(tagged), "(9) made plain again, it still holds the stock bytes, with no .bak and no temporary file");
	}

	// ---- (10) the file to save over is a symbolic link to another file. Windows makes one only with the privilege (an administrator, or Developer Mode): where it can't be made, the
	// case says so and isn't counted (GitHub's runner makes it)
	{
		std::string const real = folder + "\\Pointed-at.rif", pointer = folder + "\\Pointer.rif";
		OpenLevelFile level;

		Check(CopyFileA(stock.c_str(), real.c_str(), FALSE) != 0 && level.Open(real.c_str()) == LevelOpenResult::Opened && level.Level(), "(10) a copy of the stock level opens");

		if (!CreateSymbolicLinkA(pointer.c_str(), real.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
		{
			printf("  --  (10) not tried here: this computer makes no symbolic link without the privilege (Windows error %lu)\n", (unsigned long)GetLastError());
		}
		else
		{
			host::LevelSaveSitting own;
			bool const cut = CutLinks(&level, garage, 6);
			host::LevelSaveOver const result = own.Save(&level, pointer.c_str(), &backupPath, &why);

			Check(cut && result != host::LevelSaveOver::Saved && why.find("plain file") != std::string::npos, "(10) it is NOT saved over, and the words say why: " + why);
			Check(Bytes(real) == asOpened && (GetFileAttributesA(pointer.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && NoBackupNorTemporary(pointer),
				"(10) the file it points at still holds the stock bytes, the link is still a link, with no .bak and no temporary file");
			DeleteFileA(pointer.c_str());
		}
	}

	// ---- (11) the last look before the replace (LevelFile_StillThese): only the very bytes, in a plain file with one name, pass
	{
		std::string const looked = folder + "\\Looked-at.rif", lookedOther = folder + "\\Looked-at-other-name.rif";
		std::vector<char> const asChars(asOpened.begin(), asOpened.end());
		std::vector<char> changedChars = asChars;

		if (!changedChars.empty())
		{
			changedChars.back() ^= 1;
		}

		bool const copied = CopyFileA(stock.c_str(), looked.c_str(), FALSE) != 0;

		Check(copied && LevelFile_StillThese(looked.c_str(), asChars), "(11) a plain file holding the bytes looked for passes");
		Check(!LevelFile_StillThese(looked.c_str(), changedChars), "(11) other bytes do not");
		Check(!LevelFile_StillThese((folder + "\\Not-there.rif").c_str(), std::vector<char>()), "(11) nothing there does not");
		Check(CreateHardLinkA(lookedOther.c_str(), looked.c_str(), nullptr) != 0 && !LevelFile_StillThese(looked.c_str(), asChars),
			"(11) the same bytes under a second name do not");
	}

	EveryPlacedObjectCases(stock, folder);
}

// ---- (12) EVERY PLACED OBJECT IN THE VIEW (Vasquez 2026-11-25; the owner, holding alpha.7: "Every object, always"; level_save.h, LevelSave_ViewFlags): the rule, the walk over
// hostile bytes, and the view's own copy of the stock level, which must change only those flags, and only in that copy. Invasion has 229 placed objects, 2 of them not at Medium
// (pulse1, pulse8), and 11 whose flags the rule changes (AvP-docs/tools/vasquez/difficulty-survey.js; the others are Not Hard only, or have a species not ticked).

// The header flags of the level's object of that name, or ~0 when it has none
static uint32_t FlagsOfObject(OpenLevelFile *level, char const *name)
{
	List<Object_Chunk *> objects;

	if (level && level->Level())
	{
		level->Level()->list_objects(&objects);
	}

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		if (each() && each()->get_header() && strcmp(each()->object_data.o_name, name) == 0)
		{
			return (uint32_t)each()->get_header()->flags;
		}
	}

	return ~0u;
}

// The flags of every top-level object of the level, in its order (placed or not)
static std::vector<uint32_t> FlagsOfEveryObject(OpenLevelFile *level)
{
	std::vector<uint32_t> flags;
	List<Object_Chunk *> objects;

	if (level && level->Level())
	{
		level->Level()->list_objects(&objects);
	}

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		flags.push_back(each() && each()->get_header() ? (uint32_t)each()->get_header()->flags : ~0u);
	}

	return flags;
}

static std::vector<char> WordBytes(uint32_t word)
{
	return { (char)(word & 0xff), (char)((word >> 8) & 0xff), (char)((word >> 16) & 0xff), (char)(word >> 24) };
}

// A chunk: its 8 letters, its whole size (12 + the payload) and the payload; sizeOverride, when not 0, is written as its size instead (a hostile chunk)
static std::vector<char> ChunkOf(char const *letters, std::vector<char> const &payload, uint32_t sizeOverride = 0)
{
	std::vector<char> chunk(letters, letters + 8);
	std::vector<char> const size = WordBytes(sizeOverride ? sizeOverride : (uint32_t)(12 + payload.size()));

	chunk.insert(chunk.end(), size.begin(), size.end());
	chunk.insert(chunk.end(), payload.begin(), payload.end());
	return chunk;
}

static std::vector<char> Joined(std::vector<std::vector<char>> const &parts)
{
	std::vector<char> all;

	for (std::vector<char> const &part : parts)
	{
		all.insert(all.end(), part.begin(), part.end());
	}

	return all;
}

// An object with a header of headBytes payload bytes, its flags first (a header of fewer than 4 bytes holds only part of them)
static std::vector<char> ObjectOf(uint32_t flags, size_t headBytes)
{
	std::vector<char> head = WordBytes(flags);

	head.resize(headBytes, 0);
	return ChunkOf("RBOBJECT", ChunkOf("OBJHEAD1", head));
}

static void EveryPlacedObjectCases(std::string const &stock, std::string const &folder)
{
	// the rule
	Check(host::LevelSave_ViewFlags(0x0063fc00u) == 0x0003fc00u, "(12) the rule: Derelict's medbox013 (Easy only, every species ticked) is at every difficulty in the view's copy");
	Check(host::LevelSave_ViewFlags(0x00000400u) == 0x0000bc00u, "(12) the rule: a placed object with no species and no PC load ticked gets PC load and the three species");
	Check(host::LevelSave_ViewFlags(0x80040401u) == 0x8004bc01u, "(12) the rule: every other bit is as it was");

	// the walk: one placed object to change, one not placed, one placed whose header is too short to hold its flags
	std::vector<char> const good = ChunkOf("REBINFF2", Joined({ ChunkOf("RIFVERIN", WordBytes(1)), ObjectOf(0x0063fc00u, 64), ObjectOf(0x0063f800u, 64), ObjectOf(0x0063fc00u, 2) }));
	std::vector<char> walked = good;
	size_t const changedInGood = host::LevelSave_EveryPlacedObjectIn(walked);
	std::vector<char> expected = good;
	size_t const firstFlags = 12 + 16 + 12 + 12;   // the file's head, RIFVERIN (12 + 4), the object's head, its header's head
	std::vector<char> const shown = WordBytes(0x0003fc00u);

	std::copy(shown.begin(), shown.end(), expected.begin() + firstFlags);
	Check(changedInGood == 1 && walked == expected, "(12) the walk changes the placed object's flags and not one other byte (not the unplaced one, not the short header): " +
		std::to_string(changedInGood));

	// hostile bytes: each left exactly as it was, nothing changed
	struct Hostile
	{
		char const *what;
		std::vector<char> bytes;
	};

	std::vector<char> notALevel = good;
	std::vector<char> tooLong = good;
	std::vector<char> const pastItsObject = ChunkOf("REBINFF2", Joined({ ObjectOf(0x0063fc00u, 64),
		ChunkOf("RBOBJECT", ChunkOf("OBJHEAD1", std::vector<char>(64, 0), 12 + 65)) }));   // its header says one byte more than its object holds
	std::vector<char> const sizeZero = ChunkOf("REBINFF2", Joined({ ObjectOf(0x0063fc00u, 64), ChunkOf("RBOBJECT", std::vector<char>(4, 0), 0) }));

	memcpy(notALevel.data(), "REBCRIF1", 8);
	tooLong.resize(tooLong.size() - 1);   // the file's size now says one byte more than there is

	std::vector<Hostile> const hostile = {
		{ "bytes that are not an unpacked level", notALevel },
		{ "a level whose size says more bytes than there are", tooLong },
		{ "a header reaching past its object (after an object it would have changed)", pastItsObject },
		{ "a chunk whose size is 0 (after an object it would have changed)", sizeZero },
		{ "no bytes at all", std::vector<char>() },
		{ "fewer bytes than a chunk's head", std::vector<char>(good.begin(), good.begin() + 11) },
	};

	for (Hostile const &one : hostile)
	{
		std::vector<char> bytes = one.bytes;
		size_t const changed = host::LevelSave_EveryPlacedObjectIn(bytes);

		Check(changed == 0 && bytes == one.bytes, std::string("(12) hostile: ") + one.what + ": nothing changed, the bytes exactly as they were");
	}

	// the stock level: the view's own copy changes those flags, in that copy only
	std::string const opened = folder + "\\Every-object.rif", viewPath = folder + "\\Every-object-view.rif", plainPath = folder + "\\Every-object-plain.rif",
		againPath = folder + "\\Every-object-view-again.rif";
	std::string why;

	Check(CopyFileA(stock.c_str(), opened.c_str(), FALSE) != 0, "(12) a copy of the stock level to open: " + opened);

	std::vector<uint8_t> const asCopied = Bytes(opened);
	OpenLevelFile level;

	Check(level.Open(opened.c_str()) == LevelOpenResult::Opened && level.Level(), "(12) it opens");

	uint32_t const pulse1 = FlagsOfObject(&level, "pulse1");
	std::vector<uint32_t> const before = FlagsOfEveryObject(&level);

	Check(pulse1 != ~0u && (pulse1 & OBJECT_FLAG_PLACED_OBJECT) && (pulse1 & OBJECT_FLAG_NOTDIFFICULTY2), "(12) Invasion's pulse1 is a placed object not at Medium");
	Check(host::LevelSave_ViewNeedsItsOwnCopy(&level), "(12) so the open level needs the view's own copy");

	size_t changed = 0;
	host::LevelSaveOver const written = host::LevelSave_ViewEveryObjectCopy(&level, viewPath.c_str(), &changed, &why);

	Check(written == host::LevelSaveOver::Saved && changed == 11, "(12) the view's own copy is written, with 11 objects' flags changed: " + std::to_string(changed) + " " + why);
	Check(GetFileAttributesA((viewPath + ".bak").c_str()) == INVALID_FILE_ATTRIBUTES && GetFileAttributesA((viewPath + ".saving").c_str()) == INVALID_FILE_ATTRIBUTES,
		"(12) with no .bak and no temporary file");

	{
		OpenLevelFile viewed;

		Check(viewed.Open(viewPath.c_str()) == LevelOpenResult::Opened && viewed.Level(), "(12) the view's copy loads through the game's loader");

		std::vector<uint32_t> const inView = FlagsOfEveryObject(&viewed);
		bool eachAsTheRuleSays = inView.size() == before.size() && !before.empty();

		for (size_t i = 0; eachAsTheRuleSays && i < before.size(); i++)
		{
			eachAsTheRuleSays = inView[i] == ((before[i] & OBJECT_FLAG_PLACED_OBJECT) ? host::LevelSave_ViewFlags(before[i]) : before[i]);
		}

		Check(eachAsTheRuleSays, "(12) in it every placed object has the rule's flags and every other object its own (" + std::to_string(inView.size()) + " objects)");
		Check(!(FlagsOfObject(&viewed, "pulse1") & OBJECT_FLAG_NOTDIFFICULTY2), "(12) pulse1 is at Medium in it");
		Check(!host::LevelSave_ViewNeedsItsOwnCopy(&viewed), "(12) the view's copy itself needs no copy of its own");
		Check(host::LevelSave_ViewEveryObjectCopy(&viewed, againPath.c_str(), &changed, &why) == host::LevelSaveOver::Saved && changed == 0 &&
			GetFileAttributesA(againPath.c_str()) == INVALID_FILE_ATTRIBUTES, "(12) a level with nothing to change: nothing is written");
	}

	// the level held, the file it was opened from and the working bytes' copy (the OpenedByEditor one a reopen opens again) all keep the true flags
	Check(FlagsOfObject(&level, "pulse1") == pulse1 && FlagsOfEveryObject(&level) == before, "(12) the level held keeps every object's true flags");
	Check(Bytes(opened) == asCopied, "(12) the file it was opened from is untouched");
	Check(host::LevelSave_ViewCopy(&level, plainPath.c_str(), &why) == host::LevelSaveOver::Saved, "(12) the working bytes' copy is written: " + why);

	{
		OpenLevelFile plain;

		Check(plain.Open(plainPath.c_str()) == LevelOpenResult::Opened && FlagsOfEveryObject(&plain) == before, "(12) and in it every object has its true flags (pulse1 still not at Medium)");
	}

	// the harness's list: every placed object, pulse1 with its own ID and place
	std::vector<host::ViewPlacedObject> placed;
	bool pulse1Listed = false;

	host::LevelSave_ViewsPlacedObjects(&level, &placed);

	{
		List<Object_Chunk *> objects;

		level.Level()->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			if (strcmp(each()->object_data.o_name, "pulse1") != 0)
			{
				continue;
			}

			for (host::ViewPlacedObject const &one : placed)
			{
				pulse1Listed = pulse1Listed || (one.name == "pulse1" && memcmp(one.id, &each()->object_data.ID, 8) == 0 && one.place[0] == each()->object_data.location.x &&
					one.place[1] == each()->object_data.location.y && one.place[2] == each()->object_data.location.z);
			}
		}
	}

	Check(placed.size() == 229 && pulse1Listed, "(12) the placed objects listed: " + std::to_string(placed.size()) + " of Invasion's 229, pulse1 with its ID and place");
}
