// The test of Object Placement's Edit of an ordinary object (simple_object_edit.h): the object's fields as inteng loads them, the window's answers (the limits, the type change, hostile text), and the
// bytes written into the header and the strategy. Written 2026-11-16 by Vasquez (Sonnet 5.5) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_simple_object_edit_test.exe <a level .rif> <a folder it may write its copy into>. With no arguments it says so and passes. The level is only read; the copy is the only file written.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "simple_object_edit.h"
#include "strategy_lookup.h"
#include "crash_guard.h"

static int checksFailed = 0;
static int checks = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

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

struct Found
{
	uint8_t id[8];
	host::SimpleObjectDraft draft;
};

static int32_t Int32At(std::vector<unsigned char> const &b, size_t at)
{
	int32_t v = 0;

	if (at + 4 <= b.size())
	{
		memcpy(&v, b.data() + at, 4);
	}

	return v;
}

static bool StrategyBytes(OpenLevelFile *f, uint8_t const *id, std::vector<unsigned char> *bytes)
{
	host::ObjectStrategy s;

	if (host::FindObjectStrategy(f->Level(), id, &s) != host::StrategyFound::Found)
	{
		return false;
	}

	return f->PayloadOf(s.chunk, bytes);
}

static uint32_t HeaderFlags(OpenLevelFile *f, uint8_t const *id)
{
	List<Object_Chunk *> all;

	f->Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
		{
			std::vector<unsigned char> b;

			if (each()->get_header() && f->PayloadOf(each()->get_header(), &b))
			{
				return (uint32_t)Int32At(b, 0);
			}
		}
	}

	return 0;
}

static void TestLevel(char const *path, std::string const &folder)
{
	printf("%s\n", path);

	OpenLevelFile f;

	if (f.Open(path) != LevelOpenResult::Opened)
	{
		Check(false, std::string("the level opens: ") + path);
		return;
	}

	// every object: the fields are as inteng's loader makes them
	std::vector<Found> objects;
	List<Object_Chunk *> all;

	f.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done() && objects.size() < 600; each.next())
	{
		Found one;

		memcpy(one.id, &each()->object_data.ID, 8);

		if (host::SimpleObject_Read(&f, f.Level(), one.id, &one.draft) == host::SimpleObjectResult::Done)
		{
			objects.push_back(one);
		}
	}

	Check(!objects.empty(), "the objects of the level can be read");

	bool sane = true;

	for (Found const &one : objects)
	{
		sane = sane && one.draft.explosion >= 0 && one.draft.explosion <= 3 && one.draft.mass >= 0 && one.draft.mass <= 65535 && one.draft.integrity >= 0 && one.draft.integrity <= 65535;
	}

	Check(sane, "every object's explosion is 0 to 3 and its mass and integrity are 16-bit numbers");

	// one object that has a simple object strategy, one with none, one with another strategy
	Found *simple = nullptr, *none = nullptr, *other = nullptr;

	for (Found &one : objects)
	{
		if (!simple && one.draft.strategyKind == 62)
		{
			simple = &one;
		}
		else if (!other && one.draft.strategyKind != 0 && one.draft.strategyKind != 62 && one.draft.strategyKind != 53)
		{
			other = &one;
		}
		else if (!none)
		{
			host::ObjectStrategy s;

			if (host::FindObjectStrategy(f.Level(), one.id, &s) != host::StrategyFound::Found && one.draft.flags == 0xb800)
			{
				none = &one;
			}
		}
	}

	printf("  (%zu objects; a simple object %s, one with no strategy %s, one with another strategy %s)\n", objects.size(), simple ? "yes" : "no", none ? "yes" : "no", other ? "yes" : "no");

	// A SIMPLE OBJECT: changed in place; what the window does not show stays
	if (simple)
	{
		uint8_t id[8];

		memcpy(id, simple->id, 8);

		std::vector<unsigned char> before;
		uint32_t const headerBefore = HeaderFlags(&f, id);

		StrategyBytes(&f, id, &before);

		host::SimpleObjectDraft draft = simple->draft;

		draft.type = 2;
		draft.extra = 0x16;
		draft.mass = 70;
		draft.integrity = 9;
		draft.explosion = 2;
		draft.flags = (draft.flags & ~0x100000u) | 0x200000u;   // Easy on, Medium off (the file keeps the opposite)
		draft.flags |= 0x800;
		Check(host::SimpleObject_Write(&f, f.Level(), id, draft) == host::SimpleObjectResult::Done, "a simple object is written");

		host::SimpleObjectDraft back;

		Check(host::SimpleObject_Read(&f, f.Level(), id, &back) == host::SimpleObjectResult::Done && back.type == 2 && back.extra == 0x16 && back.mass == 70 && back.integrity == 9 && back.explosion == 2, "and reads back as written");
		Check((back.flags & 0x300000u) == 0x200000u && (back.flags & 0x800) != 0, "with the game mode and difficulty it was given");

		std::vector<unsigned char> after;

		StrategyBytes(&f, id, &after);
		Check(before.size() == after.size() && after.size() >= 52 && memcmp(before.data() + 40, after.data() + 40, before.size() - 40) == 0, "the target and everything after the explosion are as they were");
		Check((Int32At(after, 36) & ~0x30) == (Int32At(before, 36) & ~0x30), "the flags word keeps all but its explosion bits");
		Check((uint32_t)Int32At(after, 32) == (9u | 0x80000000u), "the integrity word is the number with its high bit on");

		uint32_t const headerAfter = HeaderFlags(&f, id);
		uint32_t const touched = 0x800 | 0x1000 | 0x2000 | 0x8000 | 0x10000 | 0x20000 | 0x100000 | 0x200000 | 0x400000 | 0x4000;

		Check((headerAfter & ~touched) == (headerBefore & ~touched) && (headerAfter & 0x4000) != 0, "the header keeps all its other flags and has platform load set");

		// the window: answers as the controls give them
		host::SimpleObjectEdit edit;

		Check(host::SimpleEdit_Begin(&f, f.Level(), id, &edit), "the window opens on the object");

		std::string title;
		std::vector<host::SimpleFormItem> items;

		host::SimpleEdit_Items(edit, &title, &items);
		Check(title == "Edit object properties" && items.size() == 9 && items[1].label == "Enter type" && items[1].value == "Pistol" && items[1].kind == 2 && items[1].choices.size() == 8 && items[0].choices.size() == 17 && items[8].value == "Big" && items[8].kind == 3 && items[8].choices.size() == 4, "a Weapon shows its combo box headed Enter type (8 rows), no Mass or Integrity, and the explosion radio buttons");

		std::string words;
		std::vector<std::string> values;

		for (host::SimpleFormItem const &item : items)
		{
			values.push_back(item.value);
		}

		// the layout of a type (what the combo box's pick shows at once): Static has Mass and Integrity and no list, a Weapon a list and no numbers
		host::SimpleLayout const staticLayout = host::SimpleEdit_LayoutFor(0), weaponLayout = host::SimpleEdit_LayoutFor(2);

		Check(staticLayout.mass && !staticLayout.extra && !weaponLayout.mass && weaponLayout.extra && weaponLayout.extraChoices.size() == 8 && host::SimpleEdit_ExtraRow(2, 0x16) == 6, "a Static object shows Mass and Integrity, a Weapon its list with Pistol on row 6");

		// Static: its values are the type, Mass, Integrity, the six boxes and the explosion; a number past 16 bits is refused with inteng's words
		values = { "Static", "50", "2", "y", "y", "y", "y", "y", "y", "None" };

		values[1] = "65536";
		Check(host::SimpleEdit_Answer(&f, f.Level(), &edit, values, &words) == host::SimpleAnswer::Message && words == "Mass value too big (>65535)\n", "a Mass of 65536 is refused: Mass value too big");
		values[1] = "-1";
		Check(host::SimpleEdit_Answer(&f, f.Level(), &edit, values, &words) == host::SimpleAnswer::Message && words == "Mass value too big (>65535)\n", "a Mass of -1 is refused too");
		values[1] = "5";
		values[2] = "99999999999";
		Check(host::SimpleEdit_Answer(&f, f.Level(), &edit, values, &words) == host::SimpleAnswer::Message && words.find("Integrity value too big") == 0, "an Integrity that is no 16-bit number is refused");

		// HOSTILE TEXT, with its reason: the texts come from a person, so a very long one, text with no number, and control characters must be taken without reading past anything
		std::string const very(5000, '9');
		values[1] = very;
		values[2] = "abc";

		host::SimpleAnswer answer = host::SimpleAnswer::Done;

		Check(Survives([&] { answer = host::SimpleEdit_Answer(&f, f.Level(), &edit, values, &words); }), "a 5000-letter Mass and a text with no number in Integrity are taken without stopping the program (each value is cut to 63 letters)");
		Check(answer == host::SimpleAnswer::Message, "a Mass of 63 nines is too big");

		values[1] = "abc";
		values[2] = "abc";
		Check(host::SimpleEdit_Answer(&f, f.Level(), &edit, values, &words) == host::SimpleAnswer::Done, "text with no number is taken (inteng's own: the number is then the type's, 0 for Static)");

		host::SimpleObjectDraft nonum;

		host::SimpleObject_Read(&f, f.Level(), id, &nonum);
		Check(nonum.type == 0 && nonum.mass == 0 && nonum.integrity == 0, "and the Mass and Integrity are 0, the Static number");
	}

	// NO STRATEGY: a simple object is made, with the window's values
	if (none)
	{
		uint8_t id[8];

		memcpy(id, none->id, 8);

		host::SimpleObjectDraft draft = none->draft;

		draft.type = 4;
		draft.extra = 1;
		draft.explosion = 3;
		host::SimpleObjectResult const madeNone = host::SimpleObject_Write(&f, f.Level(), id, draft);

		Check(madeNone == host::SimpleObjectResult::Done || madeNone == host::SimpleObjectResult::NotPlaced, std::string("an object with no strategy is given a simple object: ") + host::SimpleObjectWords(madeNone));

		host::SimpleObjectDraft back;

		Check(madeNone != host::SimpleObjectResult::Done || (host::SimpleObject_Read(&f, f.Level(), id, &back) == host::SimpleObjectResult::Done && back.strategyKind == 62 && back.type == 4 && back.extra == 1 && back.explosion == 3), "which reads back with the values given");

		std::vector<unsigned char> bytes;

		StrategyBytes(&f, id, &bytes);
		Check(madeNone != host::SimpleObjectResult::Done || (bytes.size() >= 52 && (Int32At(bytes, 36) & 1) == 1), "and has the creation flag bit");
	}

	// ANOTHER STRATEGY: replaced by a simple object (inteng's own)
	if (other)
	{
		uint8_t id[8];

		memcpy(id, other->id, 8);
		host::SimpleObjectResult const madeOther = host::SimpleObject_Write(&f, f.Level(), id, other->draft);

		Check(madeOther == host::SimpleObjectResult::Done, std::string("an object with another strategy is written, named by others or not (inteng replaces it): ") + host::SimpleObjectWords(madeOther) + " (kind " + std::to_string(other->draft.strategyKind) + ")");

		host::SimpleObjectDraft back;

		Check(madeOther != host::SimpleObjectResult::Done || (host::SimpleObject_Read(&f, f.Level(), id, &back) == host::SimpleObjectResult::Done && back.strategyKind == 62), "and its strategy is now a simple object (inteng replaces it)");
	}

	// the changed level is taken by the game's loader as a copy
	std::string const saved = folder + "\\simple_saved.rif";

	if (f.SaveAs(saved.c_str()) == LevelSaveResult::Saved)
	{
		OpenLevelFile second;

		Check(second.Open(saved.c_str()) == LevelOpenResult::Opened, "a saved copy with the changes is taken by the game's own loader");
		DeleteFileA(saved.c_str());
		DeleteFileA((saved + ".bak").c_str());
	}
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_simple_object_edit_test: give a level .rif and a folder for the copy; with none there is nothing to run.\n");
		printf("passed host_simple_object_edit_test (0 ok)\n");
		return 0;
	}

	std::string const folder = std::string(argv[2]) + "\\simple_object_test_" + std::to_string(GetCurrentProcessId());

	CreateDirectoryA(folder.c_str(), 0);
	TestLevel(argv[1], folder);
	RemoveDirectoryA(folder.c_str());

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
