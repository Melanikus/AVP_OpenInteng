// The check of Hierarchy mode's data commands (Copy Hierarchy, Create / Rename / Voice Index / Delete Collection, Add to / Remove from Collection, Set speed and angle),
// made by someone who built none of it (the non-builder's check for the Hierarchy MVP). It runs on a REAL level (the program arguments after the folder: the first
// level that holds a hierarchy is used): each command is done on a freshly opened level through hierarchy_edit.h (what hierarchy_mode.h calls), the level is SAVED AS A
// COPY and loaded again with the game's loader, and the result is compared with what the command promises, counted from the level's own bytes (the chunk names are counted
// in the unpacked image, not through the code under test). Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// USE: host_hierarchy_check_test.exe <a folder it may write its copies into> <a level .rif> [more levels]. With fewer arguments it says so and passes.
// The levels are only read; the copies are the only files written.
//
// AFTER EVERY COMMAND: the game's loader takes the copy; the opened file is byte for byte what it was; the commands that change nothing outside their chunks leave
// every other chunk count as it was (the objects, the shapes).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <functional>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "hierarchy_edit.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls: files are read by Windows' own

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

static bool ReadWholeFile(std::string const &path, std::vector<char> *out)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	LARGE_INTEGER size;
	bool good = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1LL << 31);

	if (good)
	{
		out->assign((size_t)size.QuadPart, 0);
		DWORD got = 0;
		good = ReadFile(file, out->data(), (DWORD)out->size(), &got, 0) && got == out->size();
	}

	CloseHandle(file);
	return good;
}

static std::string CopyPathIn(std::string const &folder, char const *name)
{
	return folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + name;
}

// How many times a chunk's 8-letter name stands in the level's unpacked image (as the chunk library's own name field, counted from the bytes)
static size_t CountName(std::vector<char> const &image, char const *name)
{
	size_t count = 0;

	for (size_t at = 0; at + 8 <= image.size(); at++)
	{
		if (image[at] == name[0] && memcmp(image.data() + at, name, 8) == 0)
		{
			count++;
		}
	}

	return count;
}

struct Counts
{
	size_t hierarchies, collections, platforms, sequences, objects, shapes;
};

static Counts CountsOf(OpenLevelFile &level)
{
	std::vector<char> image;
	Counts c = {};

	if (level.BuildLevel(image))
	{
		c.hierarchies = CountName(image, "OBJCHIER");
		c.collections = CountName(image, "HSETCOLL");
		c.platforms = CountName(image, "OBASEQSP");
		c.sequences = CountName(image, "OBANSEQC");
		c.objects = CountName(image, "RBOBJECT");
		c.shapes = CountName(image, "REBSHAPE");
	}

	return c;
}

struct Context
{
	std::string levelPath, folder;
	std::vector<char> original;
	Counts before;
};

// Does one command on a fresh open, saves a COPY, loads it again and gives the reloaded level (opened into *after) and its counts
static bool Run(Context const &c, std::string const &name, std::function<bool(OpenLevelFile &)> const &edit, OpenLevelFile *after, Counts *counts)
{
	printf("%s\n", name.c_str());

	OpenLevelFile level;

	if (level.Open(c.levelPath.c_str()) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "the game's loader takes the level");
		return false;
	}

	Check(edit(level), "the command says it is done");

	std::string const copyPath = CopyPathIn(c.folder, "hierarchy-check-copy.rif");

	CreateDirectoryA(c.folder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());

	if (level.SaveAs(copyPath.c_str()) != LevelSaveResult::Saved)
	{
		Check(false, "the level is saved as a COPY");
		return false;
	}

	std::vector<char> originalAfter;

	Check(ReadWholeFile(c.levelPath, &originalAfter) && originalAfter == c.original, "the opened file is byte for byte what it was");

	if (after->Open(copyPath.c_str()) != LevelOpenResult::Opened || !after->Level())
	{
		Check(false, "the game's loader takes the copy");
		DeleteFileA(copyPath.c_str());
		return false;
	}

	Check(true, "the game's loader takes the copy");
	*counts = CountsOf(*after);
	DeleteFileA(copyPath.c_str());
	Check(counts->objects == c.before.objects && counts->shapes == c.before.shapes, "the objects and the shapes are as they were: " + std::to_string(counts->objects) + " objects, " + std::to_string(counts->shapes) + " shapes");
	return true;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_hierarchy_check_test <a folder for its copies> <a level .rif> [more levels]: no level was named, so nothing was tested here\n");
		printf("All checks passed\n");
		return 0;
	}

	Context c;

	c.folder = argv[1];

	std::vector<std::string> names;
	OpenLevelFile pristine;

	for (int at = 2; at < argc; at++)
	{
		OpenLevelFile level;

		if (level.Open(argv[at]) == LevelOpenResult::Opened && level.Level() && host::HierarchyEdit_Names(&level, &names) == host::HierarchyEditResult::Done && !names.empty())
		{
			c.levelPath = argv[at];
			break;
		}

		names.clear();
	}

	Check(!c.levelPath.empty(), "a level that holds a hierarchy was found among the arguments");

	if (c.levelPath.empty())
	{
		return 1;
	}

	Check(ReadWholeFile(c.levelPath, &c.original), "the level is read (it is only read, never written): " + c.levelPath);

	if (pristine.Open(c.levelPath.c_str()) != LevelOpenResult::Opened)
	{
		return 1;
	}

	c.before = CountsOf(pristine);

	std::vector<host::SequenceInfo> sequences;
	std::string const first = names[0];

	Check(host::HierarchyEdit_Sequences(&pristine, first, &sequences) == host::HierarchyEditResult::Done, "the first hierarchy's sequences are read");
	printf("  (%zu hierarchies named, %zu OBJCHIER in the bytes, %zu collections, %zu sequences in the first: \"%s\")\n", names.size(), c.before.hierarchies, c.before.collections, sequences.size(), first.c_str());
	Check(names.size() <= c.before.hierarchies, "the names found are no more than the OBJCHIER chunks counted from the bytes");

	// 1. Copy Hierarchy
	{
		OpenLevelFile after;
		Counts counts = {};

		if (Run(c, "COPY HIERARCHY (the first, as \"AponeCopy\")", [&](OpenLevelFile &level)
		{
			char const *why = nullptr;

			return host::HierarchyEdit_Copy(&level, first, "AponeCopy", &why) == host::HierarchyEditResult::Done;
		}, &after, &counts))
		{
			std::vector<std::string> now;
			std::vector<host::SequenceInfo> copied, same;

			host::HierarchyEdit_Names(&after, &now);
			Check(counts.hierarchies > c.before.hierarchies, "the copy brought its OBJCHIER chunks (the hierarchy and every section of its tree carry that name): " + std::to_string(c.before.hierarchies) + " to " + std::to_string(counts.hierarchies));
			Check(now.size() == names.size() + 1 && now.back() == "AponeCopy", "the copy is named AponeCopy and is the last hierarchy");
			bool const readBoth = host::HierarchyEdit_Sequences(&after, "AponeCopy", &copied) == host::HierarchyEditResult::Done && host::HierarchyEdit_Sequences(&after, first, &same) == host::HierarchyEditResult::Done;

			Check(readBoth && copied.size() == sequences.size() && same.size() == sequences.size(), "the copy has as many sequences as the original, and the original has all of its own: " + std::to_string(copied.size()));
			Check(counts.sequences == c.before.sequences + sequences.size(), "that many more OBANSEQC chunks in the bytes: " + std::to_string(c.before.sequences) + " to " + std::to_string(counts.sequences));

			bool sameFrames = copied.size() == sequences.size();

			for (size_t i = 0; sameFrames && i < copied.size(); i++)
			{
				sameFrames = copied[i].frames == sequences[i].frames && copied[i].number == sequences[i].number && copied[i].hasPlatform == sequences[i].hasPlatform;
			}

			Check(sameFrames, "every sequence of the copy has the original's frames, number and platform");
		}
	}

	// 2. Collections: create, voice index, rename, delete
	{
		OpenLevelFile after;
		Counts counts = {};
		int32_t made = -1;

		if (Run(c, "CREATE COLLECTION (\"AponeColl\"), then its VOICE INDEX, then RENAME", [&](OpenLevelFile &level)
		{
			char const *why = nullptr;

			return host::HierarchyEdit_CreateCollection(&level, "AponeColl", &made, &why) == host::HierarchyEditResult::Done && made >= 0
				&& host::HierarchyEdit_SetCollectionVoiceIndex(&level, made, 5, &why) == host::HierarchyEditResult::Done
				&& host::HierarchyEdit_RenameCollection(&level, made, "AponeRenamed", &why) == host::HierarchyEditResult::Done;
		}, &after, &counts))
		{
			std::vector<model::ShapeSetCollection> list;
			bool found = false;

			host::HierarchyEdit_Collections(&after, &list);

			for (model::ShapeSetCollection const &item : list)
			{
				found = found || (item.number == made && item.name == "AponeRenamed" && item.voiceIndex == 5);
			}

			Check(counts.collections == c.before.collections + 1, "one more HSETCOLL chunk in the bytes: " + std::to_string(c.before.collections) + " to " + std::to_string(counts.collections));
			Check(found, "the collection is there with its new name and voice index 5, under the number it was given: " + std::to_string(made));
			Check(counts.hierarchies == c.before.hierarchies && counts.sequences == c.before.sequences, "the hierarchies and their sequences are as they were");
		}

		if (Run(c, "CREATE COLLECTION, then DELETE it again", [&](OpenLevelFile &level)
		{
			char const *why = nullptr;
			int32_t number = -1;

			return host::HierarchyEdit_CreateCollection(&level, "AponeGone", &number, &why) == host::HierarchyEditResult::Done
				&& host::HierarchyEdit_DeleteCollection(&level, number, &why) == host::HierarchyEditResult::Done;
		}, &after, &counts))
		{
			std::vector<model::ShapeSetCollection> list;

			host::HierarchyEdit_Collections(&after, &list);
			Check(counts.collections == c.before.collections && list.size() == c.before.collections, "the collections are as they were: " + std::to_string(counts.collections));
		}
	}

	// 3. Add to / Remove from a collection, when the level offers an alternate shape set
	{
		std::vector<model::AlternateSetInfo> sets;

		host::HierarchyEdit_AlternateSets(&pristine, &sets);

		if (sets.empty())
		{
			printf("  NOT TESTED  this level has no alternate shape set (OBHALTSH): Add to / Remove from collection were not tried here\n");
		}
		else
		{
			OpenLevelFile after;
			Counts counts = {};

			if (Run(c, "ADD TO COLLECTION, then REMOVE FROM COLLECTION (the level's first alternate set)", [&](OpenLevelFile &level)
			{
				char const *why = nullptr;
				int32_t number = -1;

				return host::HierarchyEdit_CreateCollection(&level, "AponeSets", &number, &why) == host::HierarchyEditResult::Done
					&& host::HierarchyEdit_AddShapeSet(&level, number, sets[0].number, &why) == host::HierarchyEditResult::Done
					&& host::HierarchyEdit_RemoveShapeSet(&level, number, sets[0].number, &why) == host::HierarchyEditResult::Done;
			}, &after, &counts))
			{
				Check(counts.collections == c.before.collections + 1, "the new collection is there: " + std::to_string(counts.collections));
			}
		}
	}

	// 4. Set speed and angle on the first sequence
	if (!sequences.empty())
	{
		OpenLevelFile after;
		Counts counts = {};
		model::SequenceSpeed written = {};

		if (Run(c, "SET SPEED AND ANGLE (the first sequence of the first hierarchy)", [&](OpenLevelFile &level)
		{
			model::PlatformState state = { 4321, 90 };

			return host::HierarchyEdit_SetPlatform(&level, first, 0, state, &written) == host::HierarchyEditResult::Done;
		}, &after, &counts))
		{
			model::SequenceSpeed read = {};
			bool present = false;

			bool const readOk = host::HierarchyEdit_ReadPlatform(&after, first, 0, &read, &present) == host::HierarchyEditResult::Done;

			Check(readOk && present && read.speedMmPerSecond == written.speedMmPerSecond && read.angleDegrees == written.angleDegrees && written.speedMmPerSecond == 4321 && written.angleDegrees == 90,
				"the sequence reads back the speed and angle that were written: " + std::to_string(read.speedMmPerSecond) + ", " + std::to_string(read.angleDegrees));
			Check(counts.platforms >= c.before.platforms && counts.platforms <= c.before.platforms + 1, "at most one more OBASEQSP chunk in the bytes: " + std::to_string(c.before.platforms) + " to " + std::to_string(counts.platforms));
			Check(counts.hierarchies == c.before.hierarchies && counts.sequences == c.before.sequences && counts.collections == c.before.collections, "hierarchies, sequences and collections are as they were");
		}
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
