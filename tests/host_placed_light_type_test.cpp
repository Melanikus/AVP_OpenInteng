// The test of Object Placement's save of a placed shape and the PLACED LIGHT it makes by the Type (simple_object_edit.h, object_edit.h PutPlacedLight; inteng's FUN_004e1f90 0x4e2cd5-0x4e2dd8,
// AvP-docs\engine\inteng\reassess\light-closeout-frost.md TB-L2 and object-placement.md OP-130a). Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// USE: host_placed_light_type_test.exe <a level with the simple object med02L and no light on it (Genshd1)> <a level with placed lights (Invasion)> <a folder it may write its copy into>.
// With no arguments it says so and passes. The levels are only read; the copy is the only file written.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_light.hpp"
#include "environment_chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "object_edit.h"
#include "simple_object_edit.h"
#include "strategy_lookup.h"

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

static int32_t Int32At(std::vector<unsigned char> const &b, size_t at)
{
	int32_t v = 0;

	if (at + 4 <= b.size())
	{
		memcpy(&v, b.data() + at, 4);
	}

	return v;
}

static int32_t Int32At(std::vector<char> const &b, size_t at)
{
	int32_t v = 0;

	if (at + 4 <= b.size())
	{
		memcpy(&v, b.data() + at, 4);
	}

	return v;
}

static Object_Chunk *Named(OpenLevelFile &f, char const *name, uint8_t *id)
{
	List<Object_Chunk *> all;

	f.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && each()->object_data.o_name && strcmp(each()->object_data.o_name, name) == 0)
		{
			if (id)
			{
				memcpy(id, &each()->object_data.ID, 8);
			}

			return each();
		}
	}

	return nullptr;
}

static size_t LightsOf(OpenLevelFile &f, char const *name, std::vector<unsigned char> *payload)
{
	Object_Chunk *const object = Named(f, name, nullptr);
	List<Chunk *> lights;
	size_t count = 0;

	if (object)
	{
		object->lookup_child("PLOBJLIT", lights);
	}

	for (LIF<Chunk *> each(&lights); !each.done(); each.next())
	{
		if (count == 0 && payload)
		{
			f.PayloadOf(each(), payload);
		}

		count++;
	}

	return count;
}

// The name of the last chunk directly inside the RBOBJECT whose header holds this name, walking the level's bytes as they would be written (the file's heading, then its chunks: 8 letters and a size)
static std::string LastChildOf(OpenLevelFile &f, char const *name)
{
	std::vector<char> image;

	if (!f.BuildLevel(image))
	{
		return "(the level could not be put together)";
	}

	for (size_t at = 12; at + 12 <= image.size(); )
	{
		int32_t const size = Int32At(image, at + 8);

		if (size < 12 || at + (size_t)size > image.size())
		{
			return "(a chunk whose size does not fit)";
		}

		if (memcmp(image.data() + at, "RBOBJECT", 8) == 0)
		{
			std::string last;
			bool ours = false;

			for (size_t pos = at + 12; pos + 12 <= at + (size_t)size; )
			{
				int32_t const childSize = Int32At(image, pos + 8);

				if (childSize < 12 || pos + (size_t)childSize > at + (size_t)size)
				{
					return "(a child whose size does not fit)";
				}

				// OBJHEAD1: its name after 60 bytes of numbers, after the 12-byte heading
				if (memcmp(image.data() + pos, "OBJHEAD1", 8) == 0 && (size_t)childSize > 72 && strncmp(image.data() + pos + 72, name, (size_t)childSize - 72) == 0)
				{
					ours = true;
				}

				last.assign(image.data() + pos, 8);
				pos += (size_t)childSize;
			}

			if (ours)
			{
				return last;
			}
		}

		at += (size_t)size;
	}

	return "(no such object)";
}

// The header's flags as the store holds them (the library's class is not made again after a payload is set)
static uint32_t HeaderOf(OpenLevelFile &f, char const *name)
{
	Object_Chunk *const object = Named(f, name, nullptr);
	std::vector<unsigned char> head;

	if (!object || !object->get_header() || !f.PayloadOf(object->get_header(), &head))
	{
		return 0;
	}

	return (uint32_t)Int32At(head, 0);
}

// inteng's DAT_00607bd8 is 100 x the scale; the default light's range is 1,000,000 / that, cut toward 0 (worked out here on its own, from the library's chunk)
static int32_t DefaultRange(OpenLevelFile &f)
{
	double scale = 1.0;
	Chunk_With_Children *const envd = dynamic_cast<Chunk_With_Children *>(f.Level()->lookup_single_child("REBENVDT"));
	Environment_Scale_Chunk const *const s = envd ? dynamic_cast<Environment_Scale_Chunk const *>(envd->lookup_single_child("ENVSDSCL")) : nullptr;

	if (s && s->scale > 0.0)
	{
		scale = s->scale;
	}

	return (int32_t)(1000000.0 / (100.0 * scale));
}

static void TestTypeLight(char const *path, std::string const &folder)
{
	printf("%s\n", path);

	OpenLevelFile f;

	if (f.Open(path) != LevelOpenResult::Opened || !f.Level())
	{
		Check(false, std::string("the level opens: ") + path);
		return;
	}

	uint8_t id[8];

	Check(Named(f, "med02L", id) != nullptr, "the level holds med02L");
	Check(LightsOf(f, "med02L", nullptr) == 0, "with no placed light");

	host::SimpleObjectDraft draft;

	Check(host::SimpleObject_Read(&f, f.Level(), id, &draft) == host::SimpleObjectResult::Done && draft.strategyKind == 62, "it is a simple object (kind 62): " + std::to_string(draft.strategyKind));

	int32_t const range = DefaultRange(f);

	printf("  (the default light's range in this level: %d)\n", (int)range);

	// Edit > Type "Placed light", OK: the save makes the default light
	draft.type = 16;
	Check(host::SimpleObject_Write(&f, f.Level(), id, draft) == host::SimpleObjectResult::Done, "Type 16 (Placed light) is written");

	std::vector<unsigned char> light;

	Check(LightsOf(f, "med02L", &light) == 1, "the object has ONE placed light now (FUN_004e1f90 0x4e2ce5)");

	int32_t const wanted[16] = { 0x10000, 0, range, 0xffffff, 0, 4, 0x50, 0, 0, 0, 0, 0, 0, 0, 1, 0 };
	bool same = light.size() == 64;

	for (size_t i = 0; i < 16 && same; i++)
	{
		same = Int32At(light, 4 * i) == wanted[i];
	}

	Check(same, "holding inteng's default: brightness 1.0, spread 0, the range, white up, black down, Omni, Runtime + No PreLight, no times, Standard, Start on, no extra words");
	Check(LastChildOf(f, "med02L") == "PLOBJLIT", "as the object's last chunk (the library's Chunk puts a new child last): " + LastChildOf(f, "med02L"));

	std::vector<unsigned char> strategy;
	host::ObjectStrategy s;

	Check(host::FindObjectStrategy(f.Level(), id, &s) == host::StrategyFound::Found && f.PayloadOf(s.chunk, &strategy) && Int32At(strategy, 20) == 16, "and its strategy's Type is 16");

	// written again: the one it has is kept, no second
	Check(host::SimpleObject_Write(&f, f.Level(), id, draft) == host::SimpleObjectResult::Done && LightsOf(f, "med02L", nullptr) == 1, "a second save keeps the one light");

	// the copy, read by the game's loader
	std::string const saved = folder + "\\typelight-copy.rif";

	Check(f.SaveAs(saved.c_str()) == LevelSaveResult::Saved, "a copy is saved");

	{
		OpenLevelFile second;

		Check(second.Open(saved.c_str()) == LevelOpenResult::Opened && second.Level(), "the game's loader reads the copy");

		Object_Chunk *const object = second.Level() ? Named(second, "med02L", nullptr) : nullptr;
		Placed_Object_Light_Chunk *const loaded = object ? dynamic_cast<Placed_Object_Light_Chunk *>(object->lookup_single_child("PLOBJLIT")) : nullptr;

		Check(loaded && loaded->light.brightness == 0x10000 && loaded->light.range == range && loaded->light.up_colour == 0xffffff && loaded->light.engine_light_flags == 4 && loaded->light.local_light_flags == 0x50 &&
			loaded->light.flags == 1 && loaded->num_extra_data == 0, "in it med02L is a placed light with those numbers");
	}

	// another Type takes the light away (0x4e2dc9)
	draft.type = 4;
	Check(host::SimpleObject_Write(&f, f.Level(), id, draft) == host::SimpleObjectResult::Done && LightsOf(f, "med02L", nullptr) == 0, "Type 4 (Medikit) again: the placed light is taken off");

	// hostile and wrong calls
	unsigned char bad[64] = { 0 };
	int32_t one = 1;

	memcpy(bad + 60, &one, 4);   // a count of 1 extra word with none there
	Check(host::PutPlacedLight(&f, id, bad, 63) == host::ObjectEditResult::BadLight, "63 bytes are no placed light");
	Check(host::PutPlacedLight(&f, id, bad, 64) == host::ObjectEditResult::BadLight, "nor 64 whose count says one more word");
	Check(host::PutPlacedLight(&f, id, bad, 0x20000) == host::ObjectEditResult::BadLight, "nor 128 KB");
	Check(LightsOf(f, "med02L", nullptr) == 0, "and nothing was put in");

	List<Object_Chunk *> all;
	uint8_t module[8] = { 0 };
	bool haveModule = false;

	f.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done() && !haveModule; each.next())
	{
		if (each() && each()->get_header() && (each()->get_header()->flags & 0x400) == 0 && each()->lookup_single_child("MODULEDT"))
		{
			memcpy(module, &each()->object_data.ID, 8);
			haveModule = true;
		}
	}

	memset(bad, 0, sizeof(bad));
	Check(haveModule && host::PutPlacedLight(&f, module, bad, 64) == host::ObjectEditResult::NotPlaced, "a module is given no placed light");

	uint8_t const nobody[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

	Check(host::PutPlacedLight(&f, nobody, bad, 64) == host::ObjectEditResult::NoSuchObject, "nor an ID no object has");
}

static void TestSaveAll(char const *path)
{
	printf("%s\n", path);

	OpenLevelFile f;

	if (f.Open(path) != LevelOpenResult::Opened || !f.Level())
	{
		Check(false, std::string("the level opens: ") + path);
		return;
	}

	size_t lampsBefore = 0;
	List<Object_Chunk *> all;

	f.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		lampsBefore += each() && each()->lookup_single_child("PLOBJLIT") ? 1 : 0;
	}

	size_t saved = 0, changed = 0;
	uint32_t eggBefore[2] = { 0, 0 };
	char const *const eggs[2] = { "openegg", "egg2" };

	for (int e = 0; e < 2; e++)
	{
		eggBefore[e] = HeaderOf(f, eggs[e]);
	}

	Check(host::SimpleObject_SaveAll(&f, &saved, &changed) == host::SimpleObjectResult::Done, "Object Placement's Exit saves every placed shape (OP-130a)");
	Check(saved > 100, "of the level's placed shapes: " + std::to_string(saved));

	// On Invasion inteng's save changes two headers and nothing else: openegg and egg2 are simple objects of Type 0 and ExtraData 0 whose shape has an outside file, so the load ORs the three
	// platform bits in (FUN_004e05d0 0x4e0c3a -> FUN_004e44c0 0x4e4755, flags |= 0x38000) and the save writes them: 0xfc00 becomes 0x3fc00 (PSX and Saturn load put in; the game reads only PC load)
	bool eggsRight = true;

	for (int e = 0; e < 2; e++)
	{
		uint32_t const now = HeaderOf(f, eggs[e]);

		eggsRight = eggsRight && eggBefore[e] == 0xfc00 && now == 0x3fc00;
	}

	Check(changed == 2 && eggsRight, "on this stock level exactly two objects change, as inteng's save changes them: openegg and egg2 0xfc00 -> 0x3fc00 (" + std::to_string(changed) + " changed)");

	if (changed != 2 || !eggsRight)
	{
		// which ones, and how: each placed shape saved alone on a fresh open, its header and strategy before and after
		OpenLevelFile g;

		if (g.Open(path) == LevelOpenResult::Opened && g.Level())
		{
			List<Object_Chunk *> objects;
			std::vector<std::pair<std::string, std::vector<uint8_t>>> ids;

			g.Level()->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				if (each() && each()->get_header() && (each()->get_header()->flags & 0x400) && each()->object_data.o_name)
				{
					ids.push_back(std::make_pair(std::string(each()->object_data.o_name), std::vector<uint8_t>((uint8_t const *)&each()->object_data.ID, (uint8_t const *)&each()->object_data.ID + 8)));
				}
			}

			for (auto const &one : ids)
			{
				OpenLevelFile h;

				if (h.Open(path) != LevelOpenResult::Opened)
				{
					continue;
				}

				host::SimpleObjectDraft d;
				Object_Chunk *const o = Named(h, one.first.c_str(), nullptr);
				uint32_t const headBefore = o && o->get_header() ? (uint32_t)o->get_header()->flags : 0;
				host::ObjectStrategy st;
				std::vector<unsigned char> before, after;

				if (host::FindObjectStrategy(h.Level(), one.second.data(), &st) == host::StrategyFound::Found)
				{
					h.PayloadOf(st.chunk, &before);
				}

				if (host::SimpleObject_Read(&h, h.Level(), one.second.data(), &d) != host::SimpleObjectResult::Done)
				{
					continue;
				}

				host::SimpleObject_Write(&h, h.Level(), one.second.data(), d);

				if (h.HasChanges())
				{
					Object_Chunk *const o2 = Named(h, one.first.c_str(), nullptr);
					std::vector<unsigned char> head;

					if (o2 && o2->get_header())
					{
						h.PayloadOf(o2->get_header(), &head);
					}

					if (host::FindObjectStrategy(h.Level(), one.second.data(), &st) == host::StrategyFound::Found)
					{
						h.PayloadOf(st.chunk, &after);
					}

					printf("  changed: %s header 0x%x -> 0x%x, kind %d Type %d -> %d, extra %d -> %d, mass %d -> %d, integrity 0x%x -> 0x%x, flags 0x%x -> 0x%x, light %d\n", one.first.c_str(), headBefore,
						(uint32_t)Int32At(head, 0), Int32At(before, 8), Int32At(before, 20), Int32At(after, 20), Int32At(before, 24), Int32At(after, 24), Int32At(before, 28), Int32At(after, 28),
						(uint32_t)Int32At(before, 32), (uint32_t)Int32At(after, 32), (uint32_t)Int32At(before, 36), (uint32_t)Int32At(after, 36), (int)LightsOf(h, one.first.c_str(), nullptr));
				}
			}
		}
	}

	size_t lampsAfter = 0;
	List<Object_Chunk *> again;

	f.Level()->list_objects(&again);

	for (LIF<Object_Chunk *> each(&again); !each.done(); each.next())
	{
		lampsAfter += each() && each()->lookup_single_child("PLOBJLIT") ? 1 : 0;
	}

	Check(lampsBefore > 0 && lampsAfter == lampsBefore, "its " + std::to_string(lampsBefore) + " placed lights are all there");
	Check(host::SimpleObject_SaveAll(nullptr, &saved, &changed) == host::SimpleObjectResult::BadCall && saved == 0 && changed == 0, "no level: refused, nothing counted");
}

int main(int argc, char **argv)
{
	if (argc < 4)
	{
		printf("host_placed_light_type_test: no levels given (<Genshd1> <Invasion> <folder>), nothing to test: passed\n");
		return 0;
	}

	TestTypeLight(argv[1], argv[3]);
	TestSaveAll(argv[2]);

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed)
	{
		printf("FAILED host_placed_light_type_test\n");
		return 1;
	}

	printf("passed host_placed_light_type_test\n");
	return 0;
}
