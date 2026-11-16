// The test for the registry of chunk families (src/model/chunk_families.cpp) and for the chunk record over the chunks whose tables are in
// the shape of a ChunkKind (the base table's and the acoustics file's). Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no file. It holds a table of every chunk the editor has a table for, by name and family, and
// checks that each name is claimed by its family and by no other, that every name in a family's own tables is in that table (so a chunk
// added to a family without being said here fails), and that each chunk of a family whose chunks are ChunkKinds builds a record from a
// right-sized payload (the panel's own check passes, and every field the old editor showed no label for is skipped) and refuses a payload
// one byte short. A check that would crash if a guard were lost runs under Survives (crash_guard.h), so it FAILS by name instead of
// stopping the program. The program gives back a non-zero result if any check failed.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "acoustics_fields.h"
#include "ai_fields.h"
#include "chunk_payloads.h"
#include "chunk_record.h"
#include "crash_guard.h"
#include "field_model.h"
#include "field_panel_host.h"

using namespace model;
using host::BuildChunkRecord;
using host::FieldRecord;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Every chunk the editor has a table for, built by the families so far: its 8 bytes (a name of 7 letters has its zero), and its family.
// A family added later says its chunks here too, or the check of its own tables below fails.

struct ChunkSaid
{
	char const *id;
	ChunkFamily family;
};

static ChunkSaid const Chunks[] =
{
	{ "STDLIGHT", ChunkFamily::Base }, { "PLOBJLIT", ChunkFamily::Base }, { "SOUNDOB2", ChunkFamily::Base }, { "TRAKSOUN", ChunkFamily::Base },
	{ "AVPGENER", ChunkFamily::Base }, { "GENEXSET", ChunkFamily::Base }, { "PARGENDA", ChunkFamily::Base }, { "AVPSTART", ChunkFamily::Base },
	{ "AVPENVIR", ChunkFamily::Base }, { "GLOGENDC", ChunkFamily::Base }, { "ENVSDSCL", ChunkFamily::Base }, { "MODFLAGS", ChunkFamily::Base },
	{ "FRGTYPDC", ChunkFamily::Base }, { "FRGTYPSC", ChunkFamily::Base }, { "FRGSOUND", ChunkFamily::Base }, { "RANTEXID", ChunkFamily::Base },
	{ "GMODHEAD", ChunkFamily::Base }, { "SOUNDDIR", ChunkFamily::Base },
	{ "OBJHEAD1", ChunkFamily::Base },
	{ "MODACOUS", ChunkFamily::Acoustics }, { "ENVACOUS", ChunkFamily::Acoustics }, { "AMBIENCE", ChunkFamily::Acoustics },
	{ "LTSETHDR", ChunkFamily::Acoustics }, { "LITSCALE", ChunkFamily::Acoustics },
	{ "OBJTRAK2", ChunkFamily::Tracks }, { "ALTLOCAT", ChunkFamily::AlternateLocations },
	{ "AVPPATH2", ChunkFamily::Ai }, { "WAYPOINT", ChunkFamily::Ai }, { "AIMODMAS", ChunkFamily::Ai }, { "AIMODSLA", ChunkFamily::Ai },
	{ "ADJMDLEP", ChunkFamily::Ai }, { "VMDARRAY", ChunkFamily::Ai }, { "MODZONE", ChunkFamily::Ai },
};

static size_t const ChunkCount = sizeof(Chunks) / sizeof(Chunks[0]);

static ChunkFamily const Families[] = { ChunkFamily::Base, ChunkFamily::Acoustics, ChunkFamily::Tracks, ChunkFamily::AlternateLocations,
                                        ChunkFamily::Ai };

static size_t const FamilyCount = sizeof(Families) / sizeof(Families[0]);

static char const *FamilyWords(ChunkFamily family)
{
	switch (family)
	{
	case ChunkFamily::Base: return "Base";
	case ChunkFamily::Acoustics: return "Acoustics";
	case ChunkFamily::Tracks: return "Tracks";
	case ChunkFamily::AlternateLocations: return "AlternateLocations";
	case ChunkFamily::Ai: return "Ai";
	case ChunkFamily::None: return "None";
	}

	return "?";
}

// A chunk's name as text, whatever it holds after its letters
static std::string Words(char const *id)
{
	return std::string(id, strnlen(id, 8));
}

// ---- Helpers, as host_chunk_record_test.cpp has them

static void PutInt(std::vector<uint8_t> &bytes, size_t at, int32_t value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));

	for (int i = 0; i < 4; i++)
	{
		bytes.at(at + i) = (uint8_t)((bits >> (8 * i)) & 0xFF);
	}
}

// A payload the game's reader accepts, with every number 0 (chunk_payloads.h)
static std::vector<uint8_t> Valid(ChunkKind const &kind)
{
	return ValidChunkPayload(kind);
}

static bool PanelAccepts(FieldRecord const &record)
{
	ui::FieldListProblem problem = { 0, ui::FieldListRefusal::None };
	return ui::CheckFieldList(record.list.data(), record.list.size(), &problem);
}

static bool AllReadOnly(FieldRecord const &record)
{
	for (ui::FieldDescriptor const &descriptor : record.list)
	{
		if (!descriptor.readOnly)
		{
			return false;
		}
	}

	return true;
}

static bool EveryNameHasWords(FieldRecord const &record)
{
	for (ui::FieldDescriptor const &descriptor : record.list)
	{
		if (!descriptor.name || descriptor.name[0] == 0)
		{
			return false;
		}
	}

	return true;
}

// How many fields the record of this kind has, by the record's rules (chunk_record.h), worked out here from the tables alone: nothing for
// an orientation or a field the old editor showed no label for; a flags field is one Bit for each of its bits that has words; a field of
// three numbers is three; any other field is one
static size_t ExpectedFields(ChunkKind const &kind, ChunkNames const &names)
{
	size_t count = 0;

	for (size_t f = 0; f < kind.fieldCount; f++)
	{
		Field const &field = kind.fields[f];

		if (field.storage == Storage::IntMatrix)
		{
			continue;
		}

		if (field.unit == Unit::Flags)
		{
			// names with no list of bits has none to count, whatever its count says, so a lost list FAILS a check instead of being read through
			size_t const bitCount = names.bits ? names.bitCount : 0;

			for (size_t b = 0; b < bitCount; b++)
			{
				ChunkBitName const &named = names.bits[b];
				count += (strcmp(named.chunk, kind.id) == 0 && strcmp(named.field, field.name) == 0 && named.label && named.label[0]) ? 1 : 0;
			}

			continue;
		}

		if (!field.label || !field.label[0])
		{
			continue;
		}

		count += field.storage == Storage::IntTriple ? 3 : 1;
	}

	return count;
}

static bool SameName(char const *a, char const *b)
{
	return memcmp(a, b, 8) == 0;
}

// ---- The tests

static void TheClaims()
{
	printf("Who claims which chunk:\n");

	std::string firstProblem;

	for (size_t c = 0; c < ChunkCount; c++)
	{
		size_t claimants = 0;
		ChunkFamily claimedBy = ChunkFamily::None;

		for (ChunkFamily const family : Families)
		{
			if (ChunkFamilyClaims(family, Chunks[c].id))
			{
				claimants++;
				claimedBy = family;
			}
		}

		std::string problem;

		if (claimants != 1)
		{
			problem = Words(Chunks[c].id) + " is claimed by " + std::to_string(claimants) + " families";
		}
		else if (claimedBy != Chunks[c].family)
		{
			problem = Words(Chunks[c].id) + " is claimed by " + FamilyWords(claimedBy) + ", not " + FamilyWords(Chunks[c].family);
		}
		else if (FindChunkFamily(Chunks[c].id) != Chunks[c].family)
		{
			problem = Words(Chunks[c].id) + ": FindChunkFamily says " + FamilyWords(FindChunkFamily(Chunks[c].id));
		}

		firstProblem = firstProblem.empty() ? problem : firstProblem;
	}

	Check(firstProblem.empty(), "each of the " + std::to_string(ChunkCount) + " chunks is claimed by exactly one family, the one this table says, and FindChunkFamily agrees" +
	      (firstProblem.empty() ? "" : " (first problem: " + firstProblem + ")"));

	// Every family has at least one chunk here, or this table has lost a family
	bool everyFamilyHasChunks = true;

	for (ChunkFamily const family : Families)
	{
		size_t holds = 0;

		for (size_t c = 0; c < ChunkCount; c++)
		{
			holds += Chunks[c].family == family ? 1 : 0;
		}

		everyFamilyHasChunks = everyFamilyHasChunks && holds > 0;
	}

	Check(FamilyCount == 5 && everyFamilyHasChunks, "all 5 families have chunks in this test's table");

	// Names nobody holds: none, a null name, near misses of real names (one letter different, in the last place and the first)
	char const *const nobody[] = { "NOSUCHCH", "OBJTRAK3", "ALTLOCAU", "XBJTRAK2", "AXTLOCAT", "STDLIGHU", "MODACOUT", "AVPPATH3", "WAYPOINU" };
	bool noneClaimed = true;
	bool ran = Survives([&] {
		noneClaimed = FindChunkFamily(0) == ChunkFamily::None;

		for (ChunkFamily const family : Families)
		{
			noneClaimed = noneClaimed && !ChunkFamilyClaims(family, 0);
		}
	});

	for (char const *const name : nobody)
	{
		noneClaimed = noneClaimed && FindChunkFamily(name) == ChunkFamily::None;

		for (ChunkFamily const family : Families)
		{
			noneClaimed = noneClaimed && !ChunkFamilyClaims(family, name);
		}
	}

	Check(ran && noneClaimed, "a null name, and names that are one letter off a real chunk's, are claimed by no family (and give None)");
}

static void EveryNameInATable()
{
	printf("Every name in a family's own tables is in this test's table:\n");

	// Whether the table above has that name with that family
	auto isSaid = [](char const *id, ChunkFamily family) {
		for (size_t c = 0; c < ChunkCount; c++)
		{
			if (SameName(Chunks[c].id, id) && Chunks[c].family == family)
			{
				return true;
			}
		}

		return false;
	};

	bool baseSaid = ChunkKindCount > 0;
	bool baseOnce = true;

	for (size_t k = 0; k < ChunkKindCount; k++)
	{
		baseSaid = baseSaid && isSaid(ChunkKinds[k].id, ChunkFamily::Base);

		for (size_t j = k + 1; j < ChunkKindCount; j++)
		{
			baseOnce = baseOnce && !SameName(ChunkKinds[k].id, ChunkKinds[j].id);
		}
	}

	Check(baseSaid && baseOnce, "all " + std::to_string(ChunkKindCount) + " chunks of chunk_fields.cpp's table are said here as Base, and none is listed twice");

	bool acousticsSaid = AcousticsKindCount > 0;
	bool acousticsOnce = true;

	for (size_t k = 0; k < AcousticsKindCount; k++)
	{
		acousticsSaid = acousticsSaid && isSaid(AcousticsKinds[k].id, ChunkFamily::Acoustics);

		for (size_t j = k + 1; j < AcousticsKindCount; j++)
		{
			acousticsOnce = acousticsOnce && !SameName(AcousticsKinds[k].id, AcousticsKinds[j].id);
		}
	}

	Check(acousticsSaid && acousticsOnce, "all " + std::to_string(AcousticsKindCount) + " chunks of the acoustics file's table are said here as Acoustics, and none is listed twice");

	bool aiSaid = AiChunkCount > 0;
	bool aiOnce = true;

	for (size_t k = 0; k < AiChunkCount; k++)
	{
		aiSaid = aiSaid && isSaid(AiChunks[k].id, ChunkFamily::Ai);

		for (size_t j = k + 1; j < AiChunkCount; j++)
		{
			aiOnce = aiOnce && !SameName(AiChunks[k].id, AiChunks[j].id);
		}
	}

	Check(aiSaid && aiOnce, "all " + std::to_string(AiChunkCount) + " chunks of the AI file's table are said here as Ai, and none is listed twice");

	// And the other way: every chunk this table says is in a family's own tables, where those tables name chunks
	bool everyOneIsHeld = true;

	for (size_t c = 0; c < ChunkCount; c++)
	{
		switch (Chunks[c].family)
		{
		case ChunkFamily::Base: everyOneIsHeld = everyOneIsHeld && FindChunkKind(Chunks[c].id) != 0; break;
		case ChunkFamily::Acoustics: everyOneIsHeld = everyOneIsHeld && FindAcousticsKind(Chunks[c].id) != 0; break;
		case ChunkFamily::Ai: everyOneIsHeld = everyOneIsHeld && FindAiChunk(Chunks[c].id) != 0; break;
		default: break;   // the tracks' two chunks are named in chunk_families.cpp only
		}
	}

	Check(everyOneIsHeld, "every Base, Acoustics and Ai chunk this table says is in its family's own tables");
}

static void NamesComeWithTheKind()
{
	printf("A chunk's kind comes with the names its fields use:\n");

	ChunkNames names = { 0, 0, 0, 0 };
	ChunkKind const *kind = FindAnyChunkKind("PLOBJLIT", &names);

	Check(kind && names.bits == ChunkBits && names.bitCount == ChunkBitCount && names.choices == ChunkChoices && names.choiceCount == ChunkChoiceCount,
	      "a Base chunk comes with the base family's bits and choices");

	names = { ChunkBits, 7, ChunkChoices, 7 };
	kind = FindAnyChunkKind("MODACOUS", &names);

	Check(kind && names.bits == 0 && names.bitCount == 0 && names.choices == AcousticsChoices && names.choiceCount == AcousticsChoiceCount,
	      "an Acoustics chunk comes with the acoustics choices and no bits, whatever was in names before");

	names = { ChunkBits, 7, ChunkChoices, 7 };
	kind = FindAnyChunkKind("NOSUCHCH", &names);

	Check(!kind && names.bits == 0 && names.bitCount == 0 && names.choices == 0 && names.choiceCount == 0,
	      "a chunk no family holds gives no kind, and names with nothing in them");

	names = { ChunkBits, 7, ChunkChoices, 7 };
	bool noKind = true;

	for (size_t c = 0; c < ChunkCount; c++)
	{
		if (Chunks[c].family == ChunkFamily::Tracks || Chunks[c].family == ChunkFamily::AlternateLocations || Chunks[c].family == ChunkFamily::Ai)
		{
			noKind = noKind && FindAnyChunkKind(Chunks[c].id, &names) == 0 && names.bitCount == 0 && names.choiceCount == 0;
		}
	}

	Check(noKind, "a chunk the editor describes in parts (a track, alternate locations, an AI chunk) has no ChunkKind to give");

	Check(FindAnyChunkKind("PLOBJLIT", 0) != 0 && FindAnyChunkKind("MODACOUS", 0) != 0, "the names are optional: a null names is allowed");
}

static void ChunksWithItems()
{
	printf("A chunk that has items isn't built whole, and says why:\n");

	std::vector<uint8_t> const bytes(64, 0);
	bool allSayItems = true;
	size_t tried = 0;

	for (size_t c = 0; c < ChunkCount; c++)
	{
		if (Chunks[c].family != ChunkFamily::Tracks && Chunks[c].family != ChunkFamily::AlternateLocations && Chunks[c].family != ChunkFamily::Ai)
		{
			continue;
		}

		FieldRecord record;
		char const *why = 0;
		bool built = true;
		bool const ran = Survives([&] { built = BuildChunkRecord(Chunks[c].id, bytes.data(), bytes.size(), &record, &why); });

		allSayItems = allSayItems && ran && !built && why && strstr(why, "items") != 0 && record.list.empty();
		tried++;
	}

	Check(tried == 9 && allSayItems, "each of the " + std::to_string(tried) + " chunks of items is refused, with words that say it has items, and an empty record");

	FieldRecord record;
	char const *why = 0;
	bool built = true;
	bool ran = Survives([&] { built = BuildChunkRecord("NOSUCHCH", bytes.data(), bytes.size(), &record, &why); });

	Check(ran && !built && why && strstr(why, "doesn't know") != 0 && strstr(why, "items") == 0, "a chunk no family holds is refused as one the editor doesn't know, not as one with items");
}

static void TheChunksOfAFamily(ChunkFamily family)
{
	printf("The chunks of the %s family, each from a right-sized payload:\n", FamilyWords(family));

	for (size_t c = 0; c < ChunkCount; c++)
	{
		if (Chunks[c].family != family)
		{
			continue;
		}

		std::string const name = Words(Chunks[c].id);
		ChunkNames names = { 0, 0, 0, 0 };
		ChunkKind const *const kind = FindAnyChunkKind(Chunks[c].id, &names);

		if (!kind)
		{
			Check(false, name + ": the chunk has no kind to build a record from");
			continue;
		}

		std::vector<uint8_t> const right = Valid(*kind);
		FieldRecord record;
		char const *why = 0;
		bool built = false;
		bool const ran = Survives([&] { built = BuildChunkRecord(Chunks[c].id, right.data(), right.size(), &record, &why); });

		Check(ran && built && PanelAccepts(record) && AllReadOnly(record) && record.list.size() == record.values.size() &&
		      record.list.size() <= host::kFieldPanelHostMostFields,
		      name + ": a right-sized payload of " + std::to_string(right.size()) + " bytes builds a record that is whole, read-only and accepted by the panel's own check" +
		      (built ? "" : std::string(" (refused: ") + (why ? why : "no reason") + ")"));

		size_t const expected = ExpectedFields(*kind, names);

		Check(built && record.list.size() == expected && EveryNameHasWords(record),
		      name + ": every field the old editor showed no label for is skipped: the record has the " + std::to_string(expected) + " fields the tables name, each with words");

		std::vector<uint8_t> const shorter(right.begin(), right.end() - (right.empty() ? 0 : 1));
		FieldRecord refused;
		char const *refusedWhy = 0;
		bool shortBuilt = true;
		bool const shortRan = Survives([&] { shortBuilt = BuildChunkRecord(Chunks[c].id, shorter.data(), shorter.size(), &refused, &refusedWhy); });

		Check(shortRan && !right.empty() && !shortBuilt && refusedWhy && refused.list.empty() && refused.title.empty(),
		      name + ": a payload one byte short is refused, with a reason, and the record is empty");
	}
}

int main()
{
	TheClaims();
	EveryNameInATable();
	NamesComeWithTheKind();
	ChunksWithItems();
	TheChunksOfAFamily(ChunkFamily::Base);
	TheChunksOfAFamily(ChunkFamily::Acoustics);

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks passed");
	return checksFailed ? 1 : 0;
}
