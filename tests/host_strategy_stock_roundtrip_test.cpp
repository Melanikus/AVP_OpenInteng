// The stock round trip of every strategy (strategy-regen-suite-design.md, 1A, with the skeptic's corrections): every strategy holder of every stock level read, its payload checked, written back
// unchanged with the editor's own writers, the level saved as a copy and read again, and every holder found as it was, in the same order, byte for byte. Then, on a fresh open, Strategy Data's Save
// before its write: the repair pass and the Links rebuilt (strategy_screen.cpp StrategySave: StrategyRepair_Run, then StrategyLinksMakeAgain), and what those two change listed in
// {OUT}\repair-changes.tsv, so the regeneration scenarios know which stock strategies that Save changes. Written 2026-11-17 by Melanikus (Bishop) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_strategy_stock_roundtrip_test.exe <folder it may write into> <level> [<level> ...] (tests/host_strategy_stock_roundtrip_test.runargs: {OUT} {LEVELS}). The levels are only read, and
// opened so that other tests may read them at the same time (avp_CreateFile below). The copies go into the folder; a copy that came back the same is deleted again, one that did not is kept.
// Each level runs in a child process of its own (this program with --one <folder> <its list> <level>), a quarter of the processors at once, and the lines come back in the levels' order (main).
//
// THE HOLDERS are the strategy objects in inteng's records order (strategy_lookup.h ListStrategyRecords): the level's objects (modules and placed objects), then its virtual objects, its enemies and
// generators, and its placed hierarchies. A holder with no strategy is listed (it must stay without one) but nothing is written to it. A strategy's bytes are the FILE's (the store, as the editor's
// StrategyOfObject reads them); where the game's chunk class writes the same strategy with other bytes, the strategy is a row of {OUT}\class-bytes-differ.tsv (RoundTrip says why).
//
// THE WRITERS are the ones the editor's own edits use: an object's or a virtual object's through SetStrategyBytes, then VirtualObject_SetStrategyBytes when SetStrategyBytes answers that no object has
// the ID (the order of strategy_screen.cpp WriteStrategyBytes); an enemy's or a generator's through GeneratorStrategy_Put; a placed hierarchy's through PlacedHierarchyStrategy_Put (as TrackWrite).
//
// The level's bytes are hostile even here: every payload comes through the lookup, which takes nothing the game's loader did not accept, and every field is read through the field model, which checks
// the field lies inside the payload before it reads a byte.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <array>
#include <chrono>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_store.h"
#include "field_model.h"
#include "generator_strategy.h"
#include "object_edit.h"
#include "placed_hierarchy_strategy.h"
#include "strategy_edit.h"
#include "strategy_lookup.h"
#include "strategy_repair.h"
#include "strategy_views.h"
#include "virtual_object.h"

using namespace host;

static int checks = 0;
static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checks++;

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls (as host_switch_links_test). The game's loader opens a level with no sharing (mishchnk.cpp: share 0), so two tests reading one
// stock level at the same time would refuse each other; an open for reading only is shared for reading here, which changes nothing the loader reads.

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (!(access & (GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE)))
	{
		share |= FILE_SHARE_READ;
	}

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

// ---- The holders

enum class HolderClass
{
	Object,       // a placed object (an RBOBJECT without a MODULEDT)
	Module,       // a module (an RBOBJECT with a MODULEDT)
	Virtual,      // a VIOBJECT
	Generator,    // an AVPGENER of type 0
	Enemy,        // an AVPGENER of any other type
	Hierarchy,    // a PLACHIER
};

static char const *ClassWords(HolderClass c)
{
	switch (c)
	{
	case HolderClass::Object: return "object";
	case HolderClass::Module: return "module";
	case HolderClass::Virtual: return "virtual";
	case HolderClass::Generator: return "generator";
	case HolderClass::Enemy: return "enemy";
	case HolderClass::Hierarchy: return "placed hierarchy";
	}

	return "?";
}

// A holder's strategy as the STORE holds it now (strategy_edit.h LookUpStrategyBytes, as the editor's StrategyOfObject reads it): after the repair pass's field edits, the chunk's class still holds
// the bytes from before, so only the store says what a save would write. kind 0 and no bytes when it has none.
static void StoreBytes(OpenLevelFile &level, std::array<uint8_t, 8> const &id, int32_t *kind, std::vector<uint8_t> *payload)
{
	LevelStore store(&level);
	ObjectStrategy now;

	*kind = 0;
	payload->clear();

	if (LookUpStrategyBytes(level.Level(), store, id.data(), &now) == StrategyFound::Found && now.payload.size() >= 12)
	{
		memcpy(kind, now.payload.data() + 8, 4);
		*payload = now.payload;
	}
}

struct Holder
{
	HolderClass what = HolderClass::Object;
	std::string name;
	std::array<uint8_t, 8> id = {};
	int32_t kind = 0;               // 0: no strategy
	std::vector<uint8_t> payload;   // the AVPSTRAT's bytes after its 12-byte heading, as the file holds them (the store: what the editor's views read and its writes start from)
	std::vector<uint8_t> classBytes;   // the same strategy as the game's chunk class writes it again (ListStrategyRecords: FindObjectStrategy asks the class for its bytes)
};

// Each holder of the level in the records' order, with its class and name; false (and a FAILED line) when the lists do not add up to the records, which would make every place below wrong
static bool ListHolders(OpenLevelFile &level, std::string const &label, std::vector<Holder> *out)
{
	out->clear();

	std::vector<StrategyRecordRef> records;

	ListStrategyRecords(level.Level(), &records);

	List<Object_Chunk *> objects;

	level.Level()->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object)
		{
			continue;
		}

		Holder one;

		one.what = object->lookup_single_child("MODULEDT") ? HolderClass::Module : HolderClass::Object;
		one.name.assign(object->object_data.o_name, strnlen(object->object_data.o_name, sizeof(object->object_data.o_name)));
		memcpy(one.id.data(), &object->object_data.ID, 8);
		out->push_back(one);
	}

	std::vector<VirtualObjectRef> virtuals;
	std::vector<GeneratorRef> generators;
	std::vector<PlacedHierarchyRef> hierarchies;

	ListVirtualObjects(level.Level(), &virtuals);
	ListGenerators(level.Level(), &generators);
	ListPlacedHierarchies(level.Level(), &hierarchies);

	for (VirtualObjectRef const &v : virtuals)
	{
		Holder one;

		one.what = HolderClass::Virtual;
		one.name = v.name;
		memcpy(one.id.data(), v.id, 8);
		out->push_back(one);
	}

	for (GeneratorRef const &g : generators)
	{
		Holder one;

		one.what = g.type == 0 ? HolderClass::Generator : HolderClass::Enemy;
		one.name = g.name;
		memcpy(one.id.data(), g.id, 8);
		out->push_back(one);
	}

	for (PlacedHierarchyRef const &h : hierarchies)
	{
		Holder one;

		one.what = HolderClass::Hierarchy;
		one.name = h.name;
		memcpy(one.id.data(), h.id, 8);
		out->push_back(one);
	}

	if (records.size() != out->size())
	{
		Check(false, label + ": the records (" + std::to_string(records.size()) + ") are the objects, virtual objects, enemies, generators and placed hierarchies (" + std::to_string(out->size()) + ")");
		return false;
	}

	for (size_t i = 0; i < records.size(); i++)
	{
		if (records[i].id != (*out)[i].id)
		{
			Check(false, label + ": record " + std::to_string(i) + " is " + (*out)[i].name + " of the lists");
			return false;
		}

		// the strategy as the file holds it; the class's own bytes kept beside it, for the check that the two are the same
		Holder &h = (*out)[i];

		StoreBytes(level, h.id, &h.kind, &h.payload);
		h.classBytes = records[i].payload;

		if (h.kind != records[i].kind)
		{
			Check(false, label + ": record " + std::to_string(i) + " \"" + h.name + "\": the file's bytes say kind " + std::to_string(h.kind) + ", the class " + std::to_string(records[i].kind));
			return false;
		}
	}

	return true;
}

static std::string HolderWords(std::string const &label, size_t at, Holder const &h)
{
	return label + " #" + std::to_string(at) + " " + ClassWords(h.what) + " \"" + h.name + "\" kind " + std::to_string(h.kind);
}

// ---- The payload's checks: its size is the one the game's loader takes, and every field the field model knows for its kind reads back

static std::string FieldsRead(Holder const &h, size_t *read)
{
	model::StrategyKind const *const kind = model::FindStrategyKind(h.kind);
	uint8_t const *const bytes = h.payload.data();
	size_t const length = h.payload.size();

	auto one = [&](model::StrategyKind const &k, model::Field const &field) -> bool
	{
		char const *why = nullptr;

		switch (field.storage)
		{
		case model::Storage::IntTriple:
			for (int axis = 0; axis < 3; axis++)
			{
				int32_t value = 0;

				if (!model::ReadFieldAxis(k, field, bytes, length, axis, &value, &why))
				{
					return false;
				}
			}

			return true;

		case model::Storage::ObjectId:
			{
				uint32_t first = 0, second = 0;

				return model::ReadFieldObject(k, field, bytes, length, &first, &second);
			}

		case model::Storage::IntMatrix:
			{
				size_t place = 0;

				return model::FieldPlace(k, field, bytes, length, &place) && place <= length && length - place >= 36;
			}

		default:
			{
				double value = 0;

				return model::ReadField(k, field, bytes, length, &value, &why);
			}
		}
	};

	// a kind the editor does not know (Rebellion's other games' kinds) has only the fields every strategy has; they are read as the Door's, which has them all
	model::StrategyKind const *const reader = kind ? kind : model::FindStrategyKind(50);

	if (!reader)
	{
		return "the field model has no Door to read the common fields with";
	}

	for (size_t i = 0; i < model::StrategyBaseFieldCount; i++)
	{
		model::Field const &field = model::StrategyBaseFields[i];

		if (strcmp(field.name, "StrategyType") == 0)
		{
			continue;   // the kind's own number: the readers refuse it by rule, and it is read above as the kind
		}

		if (!one(*reader, field))
		{
			return std::string("the common field ") + field.name + " does not read";
		}

		(*read)++;
	}

	for (size_t i = 0; kind && i < kind->fieldCount; i++)
	{
		if (!one(*kind, kind->fields[i]))
		{
			return std::string("the field ") + kind->fields[i].name + " does not read";
		}

		(*read)++;
	}

	return "";
}

// ---- The writers

static std::string WriteBack(OpenLevelFile &level, Holder const &h)
{
	uint8_t const *const id = h.id.data();
	uint8_t const *const bytes = h.payload.data();
	size_t const length = h.payload.size();

	switch (h.what)
	{
	case HolderClass::Object:
	case HolderClass::Module:
	case HolderClass::Virtual:
		{
			ObjectEditResult const placed = SetStrategyBytes(&level, id, bytes, length);

			if (placed != ObjectEditResult::NoSuchObject)
			{
				return placed == ObjectEditResult::Done ? "" : std::string("SetStrategyBytes: ") + ObjectEditWords(placed);
			}

			VirtualObjectResult const virt = VirtualObject_SetStrategyBytes(&level, id, bytes, length);

			return virt == VirtualObjectResult::Done ? "" : std::string("VirtualObject_SetStrategyBytes: ") + VirtualObjectWords(virt);
		}

	case HolderClass::Generator:
	case HolderClass::Enemy:
		{
			GeneratorStrategyResult const put = GeneratorStrategy_Put(&level, id, bytes, length);

			return put == GeneratorStrategyResult::Done ? "" : std::string("GeneratorStrategy_Put: ") + GeneratorStrategyWords(put);
		}

	case HolderClass::Hierarchy:
		{
			PlacedHierarchyStrategyResult const put = PlacedHierarchyStrategy_Put(&level, id, bytes, length);

			return put == PlacedHierarchyStrategyResult::Done ? "" : std::string("PlacedHierarchyStrategy_Put: ") + PlacedHierarchyStrategyWords(put);
		}
	}

	return "no writer for the holder";
}

// The field (a common one, or one of the kind's own) whose bytes hold this offset of the holder's payload, by the field model's places; "past the fixed fields" for a list's items or bytes the
// model names no field for
static std::string FieldAtWords(Holder const &h, size_t offset)
{
	model::StrategyKind const *const kind = model::FindStrategyKind(h.kind);
	model::StrategyKind const *const reader = kind ? kind : model::FindStrategyKind(50);
	uint8_t const *const bytes = h.payload.data();
	size_t const length = h.payload.size();

	auto covers = [&](model::StrategyKind const &k, model::Field const &field) -> bool
	{
		size_t place = 0;
		size_t const width = field.storage == model::Storage::IntTriple ? 12 : field.storage == model::Storage::IntMatrix ? 36 : model::StorageBytes(field.storage);

		return model::FieldPlace(k, field, bytes, length, &place) && offset >= place && offset - place < width;
	};

	for (size_t i = 0; reader && i < model::StrategyBaseFieldCount; i++)
	{
		if (covers(*reader, model::StrategyBaseFields[i]))
		{
			return model::StrategyBaseFields[i].name;
		}
	}

	for (size_t i = 0; kind && i < kind->fieldCount; i++)
	{
		if (covers(*kind, kind->fields[i]))
		{
			return kind->fields[i].name;
		}
	}

	return "past the fixed fields";
}

// Where two payloads differ, in words for a report: their sizes and the ranges of bytes that differ (the first eight ranges), or the first difference when the sizes are not the same
static std::string DifferenceWords(std::vector<uint8_t> const &a, std::vector<uint8_t> const &b)
{
	if (a.size() != b.size())
	{
		size_t first = 0;

		while (first < a.size() && first < b.size() && a[first] == b[first])
		{
			first++;
		}

		return "size " + std::to_string(a.size()) + " -> " + std::to_string(b.size()) + ", first difference at " + std::to_string(first);
	}

	std::string ranges;
	size_t shown = 0, more = 0;

	for (size_t i = 0; i < a.size();)
	{
		if (a[i] == b[i])
		{
			i++;
			continue;
		}

		size_t end = i;

		while (end < a.size() && a[end] != b[end])
		{
			end++;
		}

		if (shown < 8)
		{
			ranges += (ranges.empty() ? "" : " ") + std::to_string(i) + (end - i > 1 ? "-" + std::to_string(end - 1) : "");
			shown++;
		}
		else
		{
			more++;
		}

		i = end;
	}

	return "size " + std::to_string(a.size()) + ", bytes " + ranges + (more ? " and " + std::to_string(more) + " more ranges" : "");
}

static double SecondsSince(std::chrono::steady_clock::time_point start)
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

// The level's file name without its folder, for the labels and the copy's name; only letters, digits, '.', '-' and '_' are kept in the copy's name
static std::string BaseName(char const *path)
{
	std::string const whole = path;
	size_t const slash = whole.find_last_of("\\/");

	return slash == std::string::npos ? whole : whole.substr(slash + 1);
}

static std::string SafeName(std::string const &name)
{
	std::string safe;

	for (char c : name)
	{
		safe += (isalnum((unsigned char)c) || c == '.' || c == '-' || c == '_') ? c : '_';
	}

	return safe.empty() ? "level" : safe;
}

// The chunks that hold other chunks on the way down to a strategy (the level, its environment data and special objects, an object, its project data, a virtual object, an enemy's or generator's
// extra data, a placed hierarchy): only these are looked into. (Not every chunk with a plain name: Sulaco holds a top-level chunk whose 8-byte name is not text at all, and a strategy's own bytes
// could read as chunks by chance.)
static bool HoldsChunks(char const *name)
{
	static char const *const holders[] = { "REBINFF2", "REBENVDT", "SPECLOBJ", "RBOBJECT", "OBJPRJDT", "VIOBJECT", "AVPGENEX", "PLACHIER" };

	for (char const *one : holders)
	{
		if (memcmp(name, one, 8) == 0)
		{
			return true;
		}
	}

	return false;
}

// One chunk of the image: where it starts and how many bytes it takes, its heading included
struct ChunkSpan
{
	size_t at;
	size_t size;
};

// The chunks from the level's top down to the innermost one that holds this offset: a chunk of HoldsChunks' is looked into, its children walked from its first (any 8-byte name, then a size of at
// least 12 that stays inside it) up to the one that holds the offset; a child that does not read so ends the walk there. Every size is checked against the bytes before it is used. Empty when the
// offset is outside the level's own chunk.
static std::vector<ChunkSpan> ChunksAt(std::vector<char> const &image, size_t offset)
{
	std::vector<ChunkSpan> spans;
	size_t at = 0, end = image.size();

	for (int depth = 0; depth < 32 && end - at >= 12 && offset >= at && offset < end; depth++)
	{
		uint32_t size = 0;

		memcpy(&size, image.data() + at + 8, 4);

		if (size < 12 || size > end - at || offset >= at + size)
		{
			break;
		}

		spans.push_back({ at, size });

		size_t const payload = at + 12, stop = at + size;

		if (offset < payload || !HoldsChunks(image.data() + at))
		{
			break;
		}

		// the child that holds the offset
		size_t child = payload;
		bool found = false;

		while (!found && stop - child >= 12)
		{
			uint32_t childSize = 0;

			memcpy(&childSize, image.data() + child + 8, 4);

			if (childSize < 12 || childSize > stop - child)
			{
				break;
			}

			if (offset < child + childSize)
			{
				found = true;
			}
			else
			{
				child += childSize;
			}
		}

		if (!found)
		{
			break;
		}

		at = child;
		end = stop;
	}

	return spans;
}

// The same, in words: "REBINFF2 > REBENVDT > SPECLOBJ > VIOBJECT > AVPSTRAT (its heading +0)", or "... +20" for a place in the last one's payload
static std::string ChunkPathAt(std::vector<char> const &image, size_t offset)
{
	std::vector<ChunkSpan> const spans = ChunksAt(image, offset);
	std::string path;

	for (ChunkSpan const &s : spans)
	{
		path += (path.empty() ? "" : " > ") + std::string(image.data() + s.at, 8);
	}

	if (spans.empty())
	{
		return "(outside every chunk)";
	}

	ChunkSpan const last = spans.back();

	return path + (offset < last.at + 12 ? " (its heading +" + std::to_string(offset - last.at) + ")" : " +" + std::to_string(offset - last.at - 12));
}

// THE ONE CHANGE A WRITE BACK IS KNOWN TO MAKE (FOUND 11-17 by this test): VirtualObject_SetStrategyBytes and GeneratorStrategy_Put make the holder (a VIOBJECT, an AVPGENEX) again with its other
// children first and the AVPSTRAT LAST, so a stock holder whose AVPSTRAT stood before another child (a VOBJPROP, an AVPGENNM, a GENEXSET) has its chunks in another order after the write, though
// the game reads it the same (by name). inteng keeps the chunk where it stands (its Assign deletes only the strategy inside it, FUN_00539d60; its views edit the strategy in place). True when
// `now` is `last` with exactly that move made at `offset` (the first byte that differs): the chunk holding it is an AVPSTRAT directly inside a VIOBJECT or AVPGENEX, and `now` is `last` with that
// AVPSTRAT taken out of its holder and put back at the holder's end; *holderName is the holder's chunk name.
static bool IsStrategyMovedLast(std::vector<char> const &last, std::vector<char> const &now, size_t offset, std::string *holderName)
{
	std::vector<ChunkSpan> const spans = ChunksAt(last, offset);

	if (spans.size() < 2 || last.size() != now.size() || memcmp(last.data() + spans.back().at, "AVPSTRAT", 8) != 0)
	{
		return false;
	}

	ChunkSpan const strategy = spans.back(), holder = spans[spans.size() - 2];

	holderName->assign(last.data() + holder.at, 8);

	if (*holderName != "VIOBJECT" && *holderName != "AVPGENEX")
	{
		return false;
	}

	std::vector<char> expected(last.begin(), last.begin() + (std::ptrdiff_t)strategy.at);

	expected.insert(expected.end(), last.begin() + (std::ptrdiff_t)(strategy.at + strategy.size), last.begin() + (std::ptrdiff_t)(holder.at + holder.size));
	expected.insert(expected.end(), last.begin() + (std::ptrdiff_t)strategy.at, last.begin() + (std::ptrdiff_t)(strategy.at + strategy.size));
	expected.insert(expected.end(), last.begin() + (std::ptrdiff_t)(holder.at + holder.size), last.end());
	return expected == now;
}

// The rule above on made bytes, before any level: a level holding one VIOBJECT of an AVPSTRAT then a VOBJPROP; the same with the two the other way round is the known move; one byte changed is not;
// a chunk whose size runs past the end is not looked into (hostile sizes)
static void TestMoveRule()
{
	puts("the one known change of a write back (IsStrategyMovedLast), on made bytes:");

	auto chunk = [](char const *name, std::vector<char> const &payload)
	{
		std::vector<char> c(name, name + 8);
		uint32_t const size = (uint32_t)(12 + payload.size());

		c.insert(c.end(), (char const *)&size, (char const *)&size + 4);
		c.insert(c.end(), payload.begin(), payload.end());
		return c;
	};
	auto join = [](std::vector<char> a, std::vector<char> const &b)
	{
		a.insert(a.end(), b.begin(), b.end());
		return a;
	};

	std::vector<char> const strategy = chunk("AVPSTRAT", std::vector<char>(20, 7)), prop = chunk("VOBJPROP", std::vector<char>(16, 3));
	std::vector<char> const was = chunk("REBINFF2", chunk("REBENVDT", chunk("SPECLOBJ", chunk("VIOBJECT", join(strategy, prop)))));
	std::vector<char> const moved = chunk("REBINFF2", chunk("REBENVDT", chunk("SPECLOBJ", chunk("VIOBJECT", join(prop, strategy)))));
	std::vector<char> otherwise = moved;
	std::string holder;
	size_t const at = 12 * 4;   // the AVPSTRAT's heading: after the four headings above it, first in the VIOBJECT

	otherwise[otherwise.size() - 1] ^= 1;
	Check(IsStrategyMovedLast(was, moved, at, &holder) && holder == "VIOBJECT", "an AVPSTRAT put after the VOBJPROP that followed it is the known move (in a VIOBJECT)");
	Check(!IsStrategyMovedLast(was, otherwise, at, &holder), "the same with one byte of the strategy changed is not");
	Check(ChunkPathAt(was, at + 12) == "REBINFF2 > REBENVDT > SPECLOBJ > VIOBJECT > AVPSTRAT +0", "the chunks down to a byte are named: " + ChunkPathAt(was, at + 12));

	std::vector<char> hostile = was;
	uint32_t const huge = 0x7fffffff;

	memcpy(hostile.data() + 12 * 3 + 8, &huge, 4);   // the VIOBJECT's size past the end
	Check(ChunksAt(hostile, at).size() == 3 && !IsStrategyMovedLast(hostile, moved, at, &holder), "a chunk whose size runs past its holder is not looked into, and is no known move");
}

// THE ROUND TRIP of one level
static void RoundTrip(char const *path, std::string const &folder, size_t *strategiesSeen, FILE *classTsv, size_t *classDifferSeen, size_t *movedSeen)
{
	std::string const label = BaseName(path);
	auto const started = std::chrono::steady_clock::now();
	OpenLevelFile level;

	if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, label + ": the game's loader takes the level");
		return;
	}

	std::vector<Holder> holders;

	if (!ListHolders(level, label, &holders))
	{
		return;
	}

	// 1. every payload's size and fields
	size_t strategies = 0, fields = 0, badPayloads = 0;

	for (size_t i = 0; i < holders.size(); i++)
	{
		Holder const &h = holders[i];

		if (h.kind == 0)
		{
			continue;
		}

		strategies++;

		size_t expected = 0;
		model::SizeCheck const size = model::CheckStrategySize(h.payload.data(), h.payload.size(), &expected);
		std::string const unread = size == model::SizeCheck::Right ? FieldsRead(h, &fields) : "";

		if (size != model::SizeCheck::Right || !unread.empty())
		{
			badPayloads++;
			Check(false, HolderWords(label, i, h) + ": " + (size != model::SizeCheck::Right ? std::string(model::SizeCheckWords(size)) + " (" + std::to_string(h.payload.size()) + " bytes, " +
				std::to_string(expected) + " wanted)" : unread));
		}
	}

	// a level may hold no strategy at all (a multiplayer level); main checks that the levels together held some
	*strategiesSeen += strategies;
	Check(badPayloads == 0, label + ": " + std::to_string(holders.size()) + " holders, " + std::to_string(strategies) + " strategies: every size is the one the game's loader takes, and " +
		std::to_string(fields) + " fields read back");

	// The class's bytes beside the file's: FindObjectStrategy, ListStrategyRecords and the verifiers read a strategy as the game's chunk class writes it again, the editor's views and writes as the
	// file holds it (the store). Where the two differ the strategy is a row of {OUT}\class-bytes-differ.tsv, with the field where it first differs (a note, not a failure: none on the stock levels,
	// 11-17).
	size_t classDiffer = 0;

	for (size_t i = 0; i < holders.size(); i++)
	{
		Holder const &h = holders[i];

		if (h.kind == 0 || h.classBytes == h.payload)
		{
			continue;
		}

		classDiffer++;

		if (classTsv)
		{
			size_t first = 0;

			while (first < h.payload.size() && first < h.classBytes.size() && h.payload[first] == h.classBytes[first])
			{
				first++;
			}

			fprintf(classTsv, "%s\t#%zu %s\t%s\t%d\t%s\t%s\n", label.c_str(), i, ClassWords(h.what), h.name.c_str(), (int)h.kind, DifferenceWords(h.payload, h.classBytes).c_str(),
				FieldAtWords(h, first).c_str());
		}
	}

	*classDifferSeen += classDiffer;
	printf("  note  %s: the game's classes write %zu of its %zu strategies with other bytes than the file holds (class-bytes-differ.tsv)\n", label.c_str(), classDiffer, strategies);

	// 2. every strategy written back unchanged, with the editor's own writers
	std::vector<char> before, after, last, now;
	size_t written = 0, refused = 0, moved = 0, otherChanges = 0;

	level.BuildLevel(before);
	last = before;

	for (size_t i = 0; i < holders.size(); i++)
	{
		Holder const &h = holders[i];

		if (h.kind == 0)
		{
			continue;
		}

		std::string const why = WriteBack(level, h);

		if (!why.empty())
		{
			refused++;
			Check(false, HolderWords(label, i, h) + ": the write back is refused: " + why);
			continue;
		}

		written++;

		// what the write did to the level's bytes: nothing, or the one known change (IsStrategyMovedLast, named with its holder); anything else is a failure, named with the chunks down to the
		// first byte that differs
		level.BuildLevel(now);

		if (now != last)
		{
			size_t at = 0;
			std::string holderName;

			while (at < last.size() && at < now.size() && last[at] == now[at])
			{
				at++;
			}

			if (IsStrategyMovedLast(last, now, at, &holderName))
			{
				// inteng keeps the AVPSTRAT where it stands (its Assign deletes only the strategy inside the holder, FUN_00539d60; its views edit the strategy in place): a move is a failure
				// since 11-17 (Drake, Bishop's item: the writers keep it in place); named as the move it is
				moved++;
				Check(false, HolderWords(label, i, h) + ": its write moved the AVPSTRAT to the end of its " + holderName + " (the stock file has it before another child; inteng keeps it there, FUN_00539d60)");
			}
			else
			{
				otherChanges++;
				Check(false, HolderWords(label, i, h) + ": its write changed the level's bytes (" + std::to_string(last.size()) + " -> " + std::to_string(now.size()) + "), first at " +
					std::to_string(at) + ": " + ChunkPathAt(last, at));
			}
		}

		last.swap(now);
	}

	Check(refused == 0, label + ": every one of the " + std::to_string(strategies) + " strategies is written back by the editor's own writer (" + std::to_string(written) + " written)");

	// the level's whole bytes after the writes: the very bytes from before, but for the AVPSTRATs the writers moved (each named above); every other change failed above
	level.BuildLevel(after);
	Check(!before.empty() && otherChanges == 0 && moved == 0 && before == after, label + ": the level's bytes after the writes are the very bytes from before" +
		(moved ? " (" + std::to_string(moved) + " AVPSTRAT chunks moved to the end of their holders)" : std::string("")) + " (" + std::to_string(before.size()) + " bytes)");
	*movedSeen += moved;

	// 3. the level saved as a copy, read again, and every holder the same, in the same order, byte for byte
	std::string const copyPath = folder + "\\roundtrip-" + SafeName(label);

	DeleteFileA(copyPath.c_str());

	LevelSaveResult const saved = level.SaveAs(copyPath.c_str());

	Check(saved == LevelSaveResult::Saved, label + ": the level is saved as a copy (" + copyPath + ")");

	if (saved != LevelSaveResult::Saved)
	{
		return;
	}

	OpenLevelFile copy;
	std::vector<Holder> again;

	if (copy.Open(copyPath.c_str()) != LevelOpenResult::Opened || !copy.Level())
	{
		Check(false, label + ": the game's loader takes the copy");
		return;
	}

	if (!ListHolders(copy, label + " (copy)", &again))
	{
		return;
	}

	size_t differ = 0;

	if (again.size() != holders.size())
	{
		differ++;
		Check(false, label + ": the copy has " + std::to_string(again.size()) + " holders, the level " + std::to_string(holders.size()));
	}

	for (size_t i = 0; i < holders.size() && i < again.size(); i++)
	{
		Holder const &was = holders[i];
		Holder const &now = again[i];

		if (was.what == now.what && was.id == now.id && was.name == now.name && was.kind == now.kind && was.payload == now.payload && was.classBytes == now.classBytes)
		{
			continue;
		}

		differ++;

		if (differ <= 20)
		{
			Check(false, HolderWords(label, i, was) + ": in the copy it is " + ClassWords(now.what) + " \"" + now.name + "\" kind " + std::to_string(now.kind) +
				(was.id != now.id ? ", another ID" : "") + (was.payload != now.payload ? ", its strategy " + DifferenceWords(was.payload, now.payload) : "") +
				(was.classBytes != now.classBytes ? ", its class's bytes " + DifferenceWords(was.classBytes, now.classBytes) : ""));
		}
	}

	Check(differ == 0, label + ": the copy holds the " + std::to_string(holders.size()) + " holders in the same order, each strategy byte for byte" +
		(differ > 20 ? " (" + std::to_string(differ) + " differ, the first 20 above)" : ""));

	// a copy that came back whole is not kept: the folder would hold every stock level otherwise
	if (differ == 0)
	{
		copy.Open("");
		DeleteFileA(copyPath.c_str());
	}

	printf("  time  %s: %.1f s\n", label.c_str(), SecondsSince(started));
}

// THE REPAIR PASS AND THE LINKS on one level, freshly opened, as Strategy Data's Save runs them before it writes (strategy_screen.cpp StrategySave); every holder they change is a row of the list
static void RepairChanges(char const *path, FILE *tsv, size_t *changedSeen)
{
	std::string const label = BaseName(path);
	OpenLevelFile level;

	if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, label + ": the game's loader takes the level (the repair pass)");
		return;
	}

	std::vector<Holder> holders;

	if (!ListHolders(level, label, &holders))
	{
		return;
	}

	size_t const repaired = StrategyRepair_Run(&level);

	// the strategies as the store holds them after the repair pass
	std::vector<Holder> afterRepair = holders;

	for (Holder &h : afterRepair)
	{
		StoreBytes(level, h.id, &h.kind, &h.payload);
	}

	// the Links made again (strategy_screen.cpp StrategyLinksMakeAgain): every record's strategy as the store holds it, every switch's Links from those, written where they differ
	std::vector<StrategyRecordRef> records;

	ListStrategyRecords(level.Level(), &records);

	for (StrategyRecordRef &one : records)
	{
		int32_t kind = 0;
		std::vector<uint8_t> payload;

		StoreBytes(level, one.id, &kind, &payload);

		if (kind != 0)
		{
			one.kind = kind;
			one.payload = payload;
		}
	}

	size_t linksWritten = 0, linksRefused = 0;

	for (StrategyRecordRef const &one : records)
	{
		std::vector<std::array<uint8_t, 8>> stored;

		if ((one.kind != 61 && one.kind != 66 && one.kind != 68) || !StrategyView_StoredLinks(one.kind, one.payload, &stored))
		{
			continue;
		}

		std::vector<std::array<uint8_t, 8>> const links = StrategyView_SwitchLinks(one.kind, one.id.data(), records);
		std::vector<uint8_t> bytes;

		if (links != stored && StrategyView_WithLinks(one.kind, one.payload, links, &bytes))
		{
			Holder h;

			h.what = HolderClass::Object;   // WriteStrategyBytes' order: SetStrategyBytes, then the virtual object's writer
			h.id = one.id;
			h.kind = one.kind;
			h.payload = bytes;

			std::string const why = WriteBack(level, h);

			if (why.empty())
			{
				linksWritten++;
			}
			else
			{
				linksRefused++;
				Check(false, label + ": a switch's Links made again could not be written: " + why);
			}
		}
	}

	std::vector<Holder> afterLinks = holders;

	for (Holder &h : afterLinks)
	{
		StoreBytes(level, h.id, &h.kind, &h.payload);
	}

	size_t rows = 0;

	for (size_t i = 0; i < holders.size(); i++)
	{
		Holder const &was = holders[i];
		Holder const &now = afterLinks[i];

		if (was.kind == now.kind && was.payload == now.payload)
		{
			continue;
		}

		bool const byRepair = afterRepair[i].payload != was.payload;
		bool const byLinks = afterRepair[i].payload != now.payload;

		rows++;
		fprintf(tsv, "%s\t#%zu %s\t%s\t%d\t%s\t%s\n", label.c_str(), i, ClassWords(was.what), was.name.c_str(), (int)was.kind, DifferenceWords(was.payload, now.payload).c_str(),
			byRepair && byLinks ? "repair+links" : byRepair ? "repair" : "links");
	}

	*changedSeen += rows;
	Check(linksRefused == 0, label + ": the repair pass changed " + std::to_string(repaired) + " strategies, the Links were written again for " + std::to_string(linksWritten) + " switches; " +
		std::to_string(rows) + " holders differ from the file (listed)");
}


// ---- One level, in a process of its own (--one): its round trip, then its repair pass and Links, its rows into its own two lists (<list> and <list>.class); last a line
// "  sum  <strategies> <rows> <class rows>" for the parent's totals

static int OneLevel(std::string const &folder, std::string const &tsvPath, char const *level)
{
	size_t strategies = 0, changed = 0, classDiffer = 0, moved = 0;
	std::string const classPath = tsvPath + ".class";
	FILE *const classTsv = fopen(classPath.c_str(), "w");

	Check(classTsv != nullptr, BaseName(level) + ": its list of the class's other bytes can be written: " + classPath);
	RoundTrip(level, folder, &strategies, classTsv, &classDiffer, &moved);

	if (classTsv)
	{
		fclose(classTsv);
	}

	FILE *const tsv = fopen(tsvPath.c_str(), "w");

	Check(tsv != nullptr, BaseName(level) + ": its list can be written: " + tsvPath);

	if (tsv)
	{
		RepairChanges(level, tsv, &changed);
		fclose(tsv);
	}

	printf("  sum  %zu %zu %zu %zu\n", strategies, changed, classDiffer, moved);
	return checksFailed ? 1 : 0;
}

// ---- Every level, each in a child process of its own, several at once. Measured 11-17: one process took about 22 minutes for the 63 stock levels (every write makes the level again
// through the game's loader, 0.05-0.15 s each), past the test's 900 s; the levels share nothing, so they run side by side. The children are in a job that ends with this program, so a
// stopped run (the time limit) leaves none behind. Their lines are given here in the levels' order, and counted as this program's own.

struct Child
{
	size_t level;
	HANDLE process;
};

// The whole of a small file the child wrote (its lines, its list rows); empty when it can't be read. Nothing past 16 MB is read: a child's output is a few KB.
static std::string FileText(std::string const &path)
{
	std::string text;
	FILE *const file = fopen(path.c_str(), "rb");

	if (!file)
	{
		return text;
	}

	char block[65536];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0 && text.size() < (16u << 20))
	{
		text.append(block, got);
	}

	fclose(file);
	return text;
}

static bool StartChild(std::string const &exe, std::string const &folder, std::string const &level, std::string const &outPath, std::string const &tsvPath, HANDLE job, HANDLE *process)
{
	SECURITY_ATTRIBUTES inherit = { sizeof(inherit), nullptr, TRUE };
	HANDLE const out = CreateFileA(outPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inherit, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (out == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	// every argument in quotes: none of these paths can hold a quote (Windows refuses one in a file name)
	std::string command = "\"" + exe + "\" --one \"" + folder + "\" \"" + tsvPath + "\" \"" + level + "\"";
	std::vector<char> line(command.begin(), command.end());

	line.push_back(0);

	STARTUPINFOA start = {};
	PROCESS_INFORMATION made = {};

	start.cb = sizeof(start);
	start.dwFlags = STARTF_USESTDHANDLES;
	start.hStdInput = nullptr;
	start.hStdOutput = out;
	start.hStdError = out;

	BOOL const started = CreateProcessA(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &start, &made);

	CloseHandle(out);

	if (!started)
	{
		return false;
	}

	// in the job before it runs a single instruction, so it can never outlive this program
	if (job && !AssignProcessToJobObject(job, made.hProcess))
	{
		TerminateProcess(made.hProcess, 1);
		CloseHandle(made.hThread);
		CloseHandle(made.hProcess);
		return false;
	}

	ResumeThread(made.hThread);
	CloseHandle(made.hThread);
	*process = made.hProcess;
	return true;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc == 5 && strcmp(argv[1], "--one") == 0)
	{
		return OneLevel(argv[2], argv[3], argv[4]);
	}

	if (argc < 3)
	{
		printf("  FAILED  usage: host_strategy_stock_roundtrip_test <folder for the copies> <level> [<level> ...] (tests/host_strategy_stock_roundtrip_test.runargs)\n");
		return 1;
	}

	std::string const folder = argv[1];

	CreateDirectoryA(folder.c_str(), nullptr);

	char exe[MAX_PATH * 2] = { 0 };
	DWORD const exeLength = GetModuleFileNameA(nullptr, exe, sizeof(exe));

	if (exeLength == 0 || exeLength >= sizeof(exe))
	{
		printf("  FAILED  the program's own path could not be had, to start a child for each level\n");
		return 1;
	}

	HANDLE const job = CreateJobObjectA(nullptr, nullptr);
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};

	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

	if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
	{
		printf("  FAILED  no job for the children (they could outlive a stopped run)\n");
		return 1;
	}

	// a quarter of the processors, 1 to 8 (run-units may run other tests beside this one)
	char const *const processors = getenv("NUMBER_OF_PROCESSORS");
	long const cores = processors ? strtol(processors, nullptr, 10) : 4;
	size_t const most = (size_t)(cores / 4 < 1 ? 1 : cores / 4 > 8 ? 8 : cores / 4);

	auto const started = std::chrono::steady_clock::now();
	size_t const levels = (size_t)(argc - 2);
	std::vector<std::string> outPaths(levels), tsvPaths(levels);
	std::vector<DWORD> codes(levels, 0xffffffff);
	std::vector<Child> running;
	size_t next = 0;

	for (size_t i = 0; i < levels; i++)
	{
		std::string const stem = folder + "\\level-" + std::to_string(i) + "-" + SafeName(BaseName(argv[i + 2]));

		outPaths[i] = stem + ".out";
		tsvPaths[i] = stem + ".tsv";
	}

	TestMoveRule();
	printf("the round trip (read, written back, saved, read again), then the repair pass and the Links as Strategy Data's Save runs them; %zu levels, %zu at a time:\n", levels, most);

	while (next < levels || !running.empty())
	{
		while (next < levels && running.size() < most)
		{
			HANDLE process = nullptr;

			if (StartChild(exe, folder, argv[next + 2], outPaths[next], tsvPaths[next], job, &process))
			{
				running.push_back({ next, process });
			}

			next++;
		}

		if (running.empty())
		{
			continue;
		}

		std::vector<HANDLE> handles;

		for (Child const &c : running)
		{
			handles.push_back(c.process);
		}

		DWORD const woke = WaitForMultipleObjects((DWORD)handles.size(), handles.data(), FALSE, INFINITE);

		if (woke >= WAIT_OBJECT_0 + handles.size())
		{
			printf("  FAILED  waiting for the children failed\n");
			return 1;
		}

		Child const done = running[woke - WAIT_OBJECT_0];

		GetExitCodeProcess(done.process, &codes[done.level]);
		CloseHandle(done.process);
		running.erase(running.begin() + (std::ptrdiff_t)(woke - WAIT_OBJECT_0));
	}

	// each level's lines in order, counted; its rows into the two lists
	std::string const tsvPath = folder + "\\repair-changes.tsv";
	std::string const classPath = folder + "\\class-bytes-differ.tsv";
	FILE *const tsv = fopen(tsvPath.c_str(), "w");
	FILE *const classTsv = fopen(classPath.c_str(), "w");
	size_t strategies = 0, changed = 0, classDiffer = 0, moved = 0;

	Check(tsv != nullptr && classTsv != nullptr, "the lists can be written: " + tsvPath + ", " + classPath);

	if (tsv)
	{
		fprintf(tsv, "level\tholder\tname\tkind\tbytes\tby\n");
	}

	if (classTsv)
	{
		fprintf(classTsv, "level\tholder\tname\tkind\tbytes (the file's, then the class's)\tfirst field\n");
	}

	for (size_t i = 0; i < levels; i++)
	{
		std::string const text = FileText(outPaths[i]);
		size_t failedHere = 0, at = 0;

		while (at < text.size())
		{
			size_t end = text.find('\n', at);

			if (end == std::string::npos)
			{
				end = text.size();
			}

			std::string line = text.substr(at, end - at);

			at = end + 1;

			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}

			size_t a = 0, b = 0, c = 0, d = 0;

			if (sscanf(line.c_str(), "  sum  %zu %zu %zu %zu", &a, &b, &c, &d) == 4)
			{
				strategies += a;
				changed += b;
				classDiffer += c;
				moved += d;
				continue;
			}

			printf("%s\n", line.c_str());

			if (line.compare(0, 6, "  ok  ") == 0)
			{
				checks++;
			}
			else if (line.compare(0, 10, "  FAILED  ") == 0)
			{
				checks++;
				checksFailed++;
				failedHere++;
			}
		}

		// a child that stopped without saying why (it crashed, or never started) is a failure of its level
		if (codes[i] != 0 && failedHere == 0)
		{
			Check(false, BaseName(argv[i + 2]) + ": its process ended with " + std::to_string((unsigned long)codes[i]) + " and no FAILED line (it stopped, or could not start)");
		}

		std::string const rows = FileText(tsvPaths[i]), classRows = FileText(tsvPaths[i] + ".class");

		if (tsv)
		{
			fwrite(rows.data(), 1, rows.size(), tsv);
		}

		if (classTsv)
		{
			fwrite(classRows.data(), 1, classRows.size(), classTsv);
		}

		DeleteFileA(outPaths[i].c_str());
		DeleteFileA(tsvPaths[i].c_str());
		DeleteFileA((tsvPaths[i] + ".class").c_str());
	}

	if (tsv)
	{
		fclose(tsv);
	}

	if (classTsv)
	{
		fclose(classTsv);
	}

	Check(strategies > 0, "the levels hold strategies to test (" + std::to_string(strategies) + ")");
	printf("  note  %zu levels, %zu strategies round-tripped; the writers moved %zu AVPSTRAT chunks to the end of their holders; Strategy Data's Save changes %zu of them (%s); the game's classes "
		"write %zu with other bytes than the file holds (%s); %.1f s\n", levels, strategies, moved, changed, tsvPath.c_str(), classDiffer, classPath.c_str(), SecondsSince(started));
	printf("host_strategy_stock_roundtrip_test: %d checks, %d failed\n", checks, checksFailed);
	CloseHandle(job);
	return checksFailed ? 1 : 0;
}
