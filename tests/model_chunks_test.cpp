// The test for src/model's chunk tables and rules (the chunks edited directly: lights so far). Written 2026-11-14 by Melanikus for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no file. It builds payloads in memory, some of them hostile, and checks what the model says
// about them. The program gives back a non-zero result if any check failed. Each check's words say what it proves.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <set>
#include <string>
#include <vector>

#include "crash_guard.h"
#include "field_model.h"

using namespace model;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static void PutInt(std::vector<uint8_t> &bytes, size_t at, int32_t value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));

	for (int i = 0; i < 4; i++)
	{
		bytes.at(at + i) = (uint8_t)((bits >> (8 * i)) & 0xFF);
	}
}

static uint32_t GetUInt(std::vector<uint8_t> const &bytes, size_t at)
{
	uint32_t bits = 0;

	for (int i = 0; i < 4; i++)
	{
		bits |= (uint32_t)bytes.at(at + i) << (8 * i);
	}

	return bits;
}

static bool NamesAFileAndLine(char const *where)
{
	char const *const colon = where ? strstr(where, ":") : 0;

	return colon && colon > where && colon[1] >= '0' && colon[1] <= '9';
}

// Whether a value is allowed in a field, and the reason when it isn't ("" when it is).
static char const *Verdict(Field const &field, double value)
{
	char const *why = 0;
	return ValueIsAllowed(field, value, &why) ? "" : (why ? why : "refused with no reason");
}

static void TablesHoldTogether()
{
	printf("The tables hold together:\n");

	std::set<std::string> ids;
	bool unique = true, readers = true, sizes = true, inside = true, starts = true, masks = true, limits = true, whys = true;
	bool fieldNames = true, choices = true, creations = true, trailers = true, nameLists = true;

	for (size_t k = 0; k < ChunkKindCount; k++)
	{
		ChunkKind const &kind = ChunkKinds[k];

		unique = ids.insert(kind.id).second && unique;
		readers = strlen(kind.id) == 8 && NamesAFileAndLine(kind.reader) && readers;

		// A kind that starts with a name has the numbers the game reads after it, counted from the name's end, and may have a list among
		// them with its count first, as any list's: the game's registration gives it no smallest size (it reads the name, then each
		// number one at a time), and it has no block that holds its size, or names after them.
		if (kind.nameFirst)
		{
			sizes = kind.leastBytes == 0 && kind.fixedBytes > 0 && kind.sizeFieldAt < 0 && kind.namesRequired + kind.namesOptional == 0 &&
			        (kind.listCountAt < 0 ? kind.listItemBytes == 0
			                              : kind.fixedBytes == (unsigned)kind.listCountAt + 4 && kind.listItemBytes > 0) && sizes;
		}
		// A kind with a list has its count straight after its fields; one without has every byte the game reads in its fixed part.
		else if (kind.listCountAt >= 0)
		{
			// A kind ends with a list or with names, never both.
			sizes = kind.fixedBytes == (unsigned)kind.listCountAt + 4 && kind.listItemBytes > 0 && kind.leastBytes <= kind.fixedBytes &&
			        kind.namesRequired + kind.namesOptional == 0 && kind.sizeFieldAt < 0 && sizes;
		}
		else if (kind.sizeFieldAt >= 0)
		{
			// A block holding its own size: the game needs the payload to reach just past that number, and the editor writes more.
			sizes = kind.leastBytes == (unsigned)kind.sizeFieldAt + 4 && kind.fixedBytes > kind.leastBytes && kind.listItemBytes == 0 &&
			        kind.namesRequired + kind.namesOptional == 0 && sizes;
		}
		else
		{
			sizes = kind.fixedBytes == kind.leastBytes && kind.listItemBytes == 0 && sizes;
		}

		// The numbers the game reads after a list's last item belong to a kind that has a list
		trailers = (kind.trailerBytes == 0 || kind.listCountAt >= 0) && trailers;

		// A list of names is the kind's last part: it follows the required names, and no optional name could come before it
		nameLists = (!kind.namesToEmpty || (kind.namesOptional == 0 && kind.listCountAt < 0 && kind.sizeFieldAt < 0 && !kind.nameFirst)) && nameLists;

		std::set<std::string> names;

		for (size_t f = 0; f < kind.fieldCount; f++)
		{
			Field const &field = kind.fields[f];
			double lowest = 0, highest = 0;
			StorageRange(field.storage, &lowest, &highest);

			fieldNames = names.insert(field.name).second && fieldNames;
			bool const afterList = field.anchor == Anchor::AfterFirstList;
			starts = (field.anchor == Anchor::Start || (afterList && kind.listCountAt >= 0)) && starts;
			inside = field.offset + StorageBytes(field.storage) <=
			             (afterList ? kind.trailerBytes : (kind.listCountAt >= 0 ? (unsigned)kind.listCountAt : kind.fixedBytes)) &&
			         inside;
			masks = (field.mask == 0 || field.storage == Storage::UInt32) && (field.unit != Unit::Flags || field.mask != 0) && masks;
			whys = (field.limit == Limit::None) == (field.why[0] == 0) && field.source[0] != 0 && whys;
			// A light's starting values in the old editor aren't known yet; any value a table does claim is one its field allows.
			bool const isLight = strcmp(kind.id, "STDLIGHT") == 0 || strcmp(kind.id, "PLOBJLIT") == 0;
			creations = (!isLight || !field.hasCreation) && (!field.hasCreation || ValueIsAllowed(field, field.creation, 0)) && creations;

			if (field.limit != Limit::None)
			{
				limits = field.lowest <= field.highest && field.lowest >= lowest && field.highest <= highest && limits;
			}

			if (field.unit == Unit::Choice)
			{
				// Every value the limit allows has a name, and every name is a value the limit allows.
				bool named = field.limit != Limit::None;

				for (double v = field.lowest; named && v <= field.highest; v++)
				{
					named = ChunkChoiceIsNamed(kind.id, field.name, (int32_t)v);
				}

				choices = named && choices;
			}
		}
	}

	bool bitsBelong = true;

	for (size_t i = 0; i < ChunkBitCount; i++)
	{
		ChunkKind const *const kind = FindChunkKind(ChunkBits[i].chunk);
		Field const *const field = kind ? FindChunkField(*kind, ChunkBits[i].field) : 0;
		uint32_t const bit = ChunkBits[i].bit;

		bitsBelong = field && field->unit == Unit::Flags && bit != 0 && (bit & (bit - 1)) == 0 && (bit & field->mask) == bit && bitsBelong;
	}

	bool choicesBelong = true;

	for (size_t i = 0; i < ChunkChoiceCount; i++)
	{
		ChunkKind const *const kind = FindChunkKind(ChunkChoices[i].chunk);
		Field const *const field = kind ? FindChunkField(*kind, ChunkChoices[i].field) : 0;

		choicesBelong = field && field->unit == Unit::Choice && ChunkChoices[i].value >= field->lowest &&
		                ChunkChoices[i].value <= field->highest && choicesBelong;
	}

	Check(ChunkKindCount > 0 && unique, "each chunk kind is in the table once");
	Check(readers, "every kind is an 8-letter chunk name, and names the game's reader as a file and line");
	Check(sizes, "a kind's fixed part is its registered size, or its fields and a list's count straight after them, or a block that "
	             "holds its own size, or the numbers after its leading name; a kind ends with only one of a list, names or such a block");
	Check(fieldNames, "no kind has two fields of one name");
	Check(starts, "every chunk field is counted from the start of its payload, or from the end of its leading name, or, after a list, from "
	              "the list's end");
	Check(inside, "every field lies inside its kind's fixed part, before any list's count, or inside the numbers after its list");
	Check(trailers, "only a kind with a list has numbers after it");
	Check(nameLists, "a list of names ends a kind that has no optional name, list, block that holds its size or leading name");
	Check(masks, "a masked field is stored unsigned, and every flags field says which bits a mapper may change");
	Check(limits, "every limit is a range its storage can hold");
	Check(whys, "a field with a limit says why, and only those do; every field says where its facts come from");
	Check(choices, "every value a choice field allows has a name");
	Check(bitsBelong, "every named bit is one bit of a flags field, among the bits a mapper may change");
	Check(choicesBelong, "every named choice belongs to a choice field, and is a value its limit allows");
	Check(creations, "no light field claims a starting value (the old editor's aren't known yet), and every value claimed is allowed");
}

static void SizesAreTheGames()
{
	printf("Sizes are checked as the game's reader checks them:\n");

	ChunkKind const *const light = FindChunkKind("STDLIGHT");
	ChunkKind const *const placed = FindChunkKind("PLOBJLIT");

	if (!light || !placed)
	{
		Check(false, "the kinds these checks use are in the table");
		return;
	}

	std::vector<uint8_t> bytes(100, 0);

	Check(CheckChunkSize(*light, bytes.data(), 84) == SizeCheck::Right, "a light of 84 bytes is the right size");
	Check(CheckChunkSize(*light, bytes.data(), 83) == SizeCheck::TooShort, "a light of 83 bytes is refused as too short");
	Check(CheckChunkSize(*light, bytes.data(), 100) == SizeCheck::Right, "a longer light is read, and the rest ignored, as the game does");

	Check(CheckChunkSize(*placed, bytes.data(), 59) == SizeCheck::TooShort, "a placed light under its registered 60 bytes is refused as too short");
	Check(CheckChunkSize(*placed, bytes.data(), 60) == SizeCheck::CountCutOff,
	      "a placed light of 60 bytes has no room for its count, which the game refuses");
	Check(CheckChunkSize(*placed, bytes.data(), 64) == SizeCheck::Right, "a placed light with a count of 0 is the right size");

	PutInt(bytes, 60, -1);
	Check(CheckChunkSize(*placed, bytes.data(), 64) == SizeCheck::CountNegative, "a negative count is refused");

	PutInt(bytes, 60, 2);
	Check(CheckChunkSize(*placed, bytes.data(), 68) == SizeCheck::CountTooBig, "a count of 2 with room for 1 is refused as too big");
	Check(CheckChunkSize(*placed, bytes.data(), 72) == SizeCheck::Right, "a count of 2 with room for 2 is the right size");
	Check(CheckChunkSize(*placed, bytes.data(), 80) == SizeCheck::Right, "bytes after the items are ignored, as the game does");
}

static void ValuesAreChecked()
{
	printf("Values are checked:\n");

	ChunkKind const *const light = FindChunkKind("STDLIGHT");
	ChunkKind const *const placed = FindChunkKind("PLOBJLIT");
	Field const *const range = light ? FindChunkField(*light, "range") : 0;
	Field const *const spread = light ? FindChunkField(*light, "spread") : 0;
	Field const *const brightness = light ? FindChunkField(*light, "brightness") : 0;
	Field const *const location = light ? FindChunkField(*light, "location") : 0;
	Field const *const blue = light ? FindChunkField(*light, "colour_blue") : 0;
	Field const *const engine = light ? FindChunkField(*light, "engine_light_flags") : 0;
	Field const *const placedRange = placed ? FindChunkField(*placed, "range") : 0;
	Field const *const fadeUp = placed ? FindChunkField(*placed, "fade_up_time") : 0;
	Field const *const lightType = placed ? FindChunkField(*placed, "light_type") : 0;

	if (!range || !spread || !brightness || !location || !blue || !engine || !placedRange || !fadeUp || !lightType)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	Check(Verdict(*range, 0) == range->why, "a light's range of 0 is refused (the old editor allowed it)");
	Check(strcmp(Verdict(*range, 1), "") == 0 && strcmp(Verdict(*range, 268435456), "") == 0, "a light's range of 1 or 2^28 is allowed");
	Check(Verdict(*range, 268435457) == range->why, "a light's range past 2^28 is refused");
	Check(Verdict(*placedRange, 0) == placedRange->why, "a placed light's range of 0 is refused too");

	Check(Verdict(*spread, 0) == spread->why && Verdict(*spread, 2049) == spread->why && strcmp(Verdict(*spread, 2048), "") == 0,
	      "a light's spread is kept to 1-2048");
	Check(Verdict(*brightness, -1) == brightness->why && strcmp(Verdict(*brightness, 131072), "") == 0 &&
	      Verdict(*brightness, 131073) == brightness->why,
	      "a light's brightness is kept to 0-200%");
	Check(strcmp(Verdict(*location, 16777216), "") == 0 && Verdict(*location, 16777217) == location->why &&
	      Verdict(*location, -16777217) == location->why,
	      "a light's position is kept within 2^24 of the middle on each axis");
	Check(strcmp(Verdict(*blue, 255), "") == 0 && Verdict(*blue, 256) == DoesNotFitReason, "a colour byte takes 0-255");

	Check(strcmp(Verdict(*engine, 0x807), "") == 0 && Verdict(*engine, 0x10) == DoesNotFitReason,
	      "every engine flag inteng's light mode changes can be set (Atten, Spread atten, Omni, No specular), and No shadows (0x10), which it never changes, is refused");
	Check(Verdict(*engine, 0x100) == DoesNotFitReason, "an engine flag the game keeps for itself is refused as a value");

	Check(strcmp(Verdict(*fadeUp, 32767), "") == 0 && Verdict(*fadeUp, 32768) == fadeUp->why && Verdict(*fadeUp, -1) == fadeUp->why,
	      "a placed light's times are kept to 0-32767, as the game's reader refuses the rest");
	Check(Verdict(*lightType, 3) == lightType->why && strcmp(Verdict(*lightType, 2), "") == 0 &&
	      ChunkChoiceIsNamed("PLOBJLIT", "light_type", 2) && !ChunkChoiceIsNamed("PLOBJLIT", "light_type", 3),
	      "a light type is one of the three the game knows");
}

static void SettersWriteOnlyTheirPart()
{
	printf("Setters write only their own part:\n");

	ChunkKind const *const light = FindChunkKind("STDLIGHT");
	ChunkKind const *const placed = FindChunkKind("PLOBJLIT");
	Field const *const blue = light ? FindChunkField(*light, "colour_blue") : 0;
	Field const *const engine = light ? FindChunkField(*light, "engine_light_flags") : 0;
	Field const *const location = light ? FindChunkField(*light, "location") : 0;
	Field const *const orientation = light ? FindChunkField(*light, "orientation") : 0;
	Field const *const range = light ? FindChunkField(*light, "range") : 0;
	Field const *const placedFlags = placed ? FindChunkField(*placed, "flags") : 0;

	if (!blue || !engine || !location || !orientation || !range || !placedFlags)
	{
		Check(false, "the fields these checks use are in the tables");
		return;
	}

	char const *why = 0;
	std::vector<uint8_t> bytes(84, 0x11);

	Check(SetChunkField(*light, *blue, bytes.data(), bytes.size(), 200, &why) && bytes.at(64) == 200 && bytes.at(63) == 0x11 &&
	      bytes.at(65) == 0x11,
	      "setting a light's blue byte changes that byte alone");

	PutInt(bytes, 68, 0x300);
	Check(SetChunkField(*light, *engine, bytes.data(), bytes.size(), 0x7, &why) && GetUInt(bytes, 68) == 0x307,
	      "setting a light's engine flags keeps the bits the game keeps for itself");

	Check(SetChunkFieldAxis(*light, *location, bytes.data(), bytes.size(), 1, 5000, &why) && GetUInt(bytes, 8) == 5000 &&
	      bytes.at(4) == 0x11 && bytes.at(12) == 0x11,
	      "setting one axis of a light's position changes that axis alone");

	std::vector<uint8_t> const before = bytes;
	Check(!SetChunkField(*light, *orientation, bytes.data(), bytes.size(), 1, &why) && bytes == before,
	      "an orientation can't be set from one number");

	why = 0;
	Check(!SetChunkField(*light, *range, bytes.data(), bytes.size(), 0, &why) && why == range->why && bytes == before,
	      "a refused value changes nothing, and the reason given is the field's own");

	Check(!SetChunkField(*light, *range, bytes.data(), 60, 4000, &why) && bytes == before,
	      "a field past the end of a short payload is refused, and nothing is written");

	Check(!SetChunkField(*light, *placedFlags, bytes.data(), bytes.size(), 1, &why) && bytes == before,
	      "a field of another kind of chunk is refused, rather than written at its own offset");
}

// A payload of that many bytes, all zero, with these bytes from offset at.
static std::vector<uint8_t> Bytes(size_t size, size_t at, char const *text, size_t textLength)
{
	std::vector<uint8_t> bytes(size, 0);
	memcpy(bytes.data() + at, text, textLength);
	return bytes;
}

static SizeCheck SizeOf(ChunkKind const &kind, std::vector<uint8_t> const &bytes)
{
	return CheckChunkSize(kind, bytes.data(), bytes.size());
}

static void SoundsAreTheGames()
{
	printf("Sounds: their names end inside them, and their numbers are the game's:\n");

	ChunkKind const *const sound = FindChunkKind("SOUNDOB2");
	ChunkKind const *const track = FindChunkKind("TRAKSOUN");
	Field const *const volume = sound ? FindChunkField(*sound, "max_volume") : 0;
	Field const *const pitch = sound ? FindChunkField(*sound, "pitch") : 0;
	Field const *const flags = sound ? FindChunkField(*sound, "flags") : 0;
	Field const *const inner = sound ? FindChunkField(*sound, "inner_range") : 0;
	Field const *const index = track ? FindChunkField(*track, "index") : 0;

	if (!sound || !track || !volume || !pitch || !flags || !inner || !index)
	{
		Check(false, "the kinds and fields these checks use are in the tables");
		return;
	}

	Check(SizeOf(*sound, Bytes(39, 0, "", 0)) == SizeCheck::TooShort, "a placed sound under 40 bytes is refused as too short");
	Check(SizeOf(*sound, Bytes(40, 0, "", 0)) == SizeCheck::NameCutOff,
	      "a placed sound with no name at all is refused, as the game needs its name");
	Check(SizeOf(*sound, Bytes(44, 40, "door", 4)) == SizeCheck::NameCutOff, "a sound name with no ending zero is refused");
	Check(SizeOf(*sound, Bytes(45, 40, "door", 5)) == SizeCheck::Right, "a sound name with its zero, and no file name after, is read");
	Check(SizeOf(*sound, Bytes(50, 40, "door\0a.wav", 10)) == SizeCheck::NameCutOff,
	      "a file name after the sound name with no ending zero is refused");
	Check(SizeOf(*sound, Bytes(51, 40, "door\0a.wav", 11)) == SizeCheck::Right, "both names with their zeros are read");

	Check(SizeOf(*track, Bytes(23, 0, "", 0)) == SizeCheck::TooShort, "a track sound under 24 bytes is refused as too short");
	Check(SizeOf(*track, Bytes(24, 0, "", 0)) == SizeCheck::NameCutOff, "a track sound with no file name is refused");
	Check(SizeOf(*track, Bytes(30, 24, "a.wav", 6)) == SizeCheck::Right, "a track sound with its file name is read");
	Check(strcmp(ChunkSizeCheckWords(SizeCheck::NameCutOff), "a name at the end of the chunk has no ending zero inside it") == 0,
	      "the refusal for a cut-off name is said in words");

	Check(strcmp(Verdict(*volume, 127), "") == 0 && Verdict(*volume, 128) == volume->why && Verdict(*volume, -1) == volume->why,
	      "a sound's volume is kept to 0-127, as the game plays it");
	Check(strcmp(Verdict(*pitch, -6144), "") == 0 && Verdict(*pitch, 6145) == pitch->why && Verdict(*pitch, -6145) == pitch->why,
	      "a sound's pitch is kept within 4 octaves either way");
	Check(Verdict(*flags, 0x4) == DoesNotFitReason && strcmp(Verdict(*flags, 0x3), "") == 0, "a sound's flags take only its two bits");
	Check(Verdict(*inner, -1) == inner->why && strcmp(Verdict(*inner, 5000), "") == 0, "a sound's reach can't be negative");
	Check(Verdict(*index, 3) == index->why && strcmp(Verdict(*index, 2), "") == 0 && ChunkChoiceIsNamed("TRAKSOUN", "index", 1),
	      "a track sound is its middle, start or end sound, as the game's reader insists");
	Check(volume->hasCreation && volume->creation == 127, "a new placed sound's volume is 127, as the old editor made one");
}

static void MarkersAreTheGames()
{
	printf("Markers: generators, their settings, particle generators and the player's start:\n");

	ChunkKind const *const generator = FindChunkKind("AVPGENER");
	ChunkKind const *const settings = FindChunkKind("GENEXSET");
	ChunkKind const *const particles = FindChunkKind("PARGENDA");
	ChunkKind const *const start = FindChunkKind("AVPSTART");
	Field const *const kindField = generator ? FindChunkField(*generator, "type") : 0;
	Field const *const generatorFlags = generator ? FindChunkField(*generator, "flags") : 0;
	Field const *const pulse = settings ? FindChunkField(*settings, "PulseMarine_Wt") : 0;
	Field const *const pistol = settings ? FindChunkField(*settings, "PistolMarine_Wt") : 0;
	Field const *const particleKind = particles ? FindChunkField(*particles, "type") : 0;
	Field const *const quatX = particles ? FindChunkField(*particles, "orientation_x") : 0;
	Field const *const chance = particles ? FindChunkField(*particles, "probability") : 0;
	Field const *const startOrientation = start ? FindChunkField(*start, "orientation") : 0;

	if (!generator || !settings || !particles || !start || !kindField || !generatorFlags || !pulse || !pistol || !particleKind || !quatX ||
	    !chance || !startOrientation)
	{
		Check(false, "the kinds and fields these checks use are in the tables");
		return;
	}

	Check(generator->parent[0] && strcmp(generator->parent, "SPECLOBJ") == 0, "a generator is read only inside the special objects");
	Check(SizeOf(*generator, Bytes(27, 0, "", 0)) == SizeCheck::TooShort, "a generator under 28 bytes is refused as too short");
	Check(SizeOf(*generator, Bytes(28, 0, "", 0)) == SizeCheck::NameCutOff, "a generator with no name is refused");
	Check(SizeOf(*generator, Bytes(33, 28, "gen1", 5)) == SizeCheck::Right, "a generator with its name is read");
	Check(Verdict(*kindField, 10) == kindField->why && strcmp(Verdict(*kindField, 9), "") == 0 &&
	      ChunkChoiceIsNamed("AVPGENER", "type", 9),
	      "a generator is one of the ten kinds the game knows");
	Check(Verdict(*generatorFlags, 0x1) == DoesNotFitReason && strcmp(Verdict(*generatorFlags, 0x3800), "") == 0,
	      "a generator's always-set bit isn't a mapper's choice; its game modes are");

	// The weights block: its first number, at +20, has to be exactly the rest of the payload.
	std::vector<uint8_t> block(84, 0);
	PutInt(block, 20, 64);
	Check(SizeOf(*settings, block) == SizeCheck::Right, "generator settings with a 64-byte weights block are the right size");
	PutInt(block, 20, 60);
	Check(SizeOf(*settings, block) == SizeCheck::WrongSize, "a weights block that says 60 bytes in 64 is refused");
	PutInt(block, 20, -1);
	Check(SizeOf(*settings, block) == SizeCheck::WrongSize, "a weights block with a negative size is refused");
	std::vector<uint8_t> shortBlock(24, 0);
	PutInt(shortBlock, 20, 4);
	Check(SizeOf(*settings, shortBlock) == SizeCheck::Right, "a weights block holding only its size is read, its weights left at 0");
	Check(SizeOf(*settings, Bytes(23, 0, "", 0)) == SizeCheck::TooShort, "generator settings under 24 bytes are refused as too short");
	Check(strcmp(ChunkSizeCheckWords(SizeCheck::WrongSize), "the chunk isn't the size its block says it is") == 0,
	      "the refusal for a wrong block size is said in words");

	Check(strcmp(Verdict(*pulse, 16777216), "") == 0 && Verdict(*pulse, 16777217) == pulse->why && Verdict(*pulse, -1) == pulse->why,
	      "a weight is from 0 to 2^24, as the loader holds it, so fifteen of them can't overflow the game's sum");
	char const *why = 0;
	Check(!SetChunkField(*settings, *pistol, shortBlock.data(), shortBlock.size(), 5, &why) && GetUInt(shortBlock, 20) == 4,
	      "a weight the block doesn't hold isn't written past its end");
	Check(SetChunkField(*settings, *pistol, block.data(), block.size(), 5, &why) && GetUInt(block, 80) == 5,
	      "the last weight is written where the game reads it");

	Check(SizeOf(*particles, Bytes(68, 0, "", 0)) == SizeCheck::Right, "a particle generator may have no name");
	Check(SizeOf(*particles, Bytes(70, 68, "ab", 2)) == SizeCheck::NameCutOff, "a particle generator's name with no zero is refused");
	Check(Verdict(*particleKind, 4) == particleKind->why && strcmp(Verdict(*particleKind, 3), "") == 0,
	      "a particle generator is one of the game's four kinds");
	Check(Verdict(*quatX, 2.5) == quatX->why && Verdict(*quatX, std::numeric_limits<double>::quiet_NaN()) == NotANumberReason &&
	      strcmp(Verdict(*quatX, -1.0), "") == 0,
	      "an orientation's parts are real numbers within 2 either way");
	Check(Verdict(*chance, 101) == chance->why && strcmp(Verdict(*chance, 100), "") == 0, "a chance is 0-100");

	Check(SizeOf(*start, Bytes(56, 0, "", 0)) == SizeCheck::Right && SizeOf(*start, Bytes(55, 0, "", 0)) == SizeCheck::TooShort,
	      "the player's start is 56 bytes");
	std::vector<uint8_t> startBytes(56, 0x22);
	std::vector<uint8_t> const before = startBytes;
	Check(!SetChunkField(*start, *startOrientation, startBytes.data(), startBytes.size(), 1, &why) && startBytes == before,
	      "the start's orientation can't be set from one number");
}

// The words a bit or a choice is shown with, or "" when it has none
static std::string BitWords(char const *chunk, char const *field, uint32_t bit)
{
	for (size_t i = 0; i < ChunkBitCount; i++)
	{
		if (strcmp(ChunkBits[i].chunk, chunk) == 0 && strcmp(ChunkBits[i].field, field) == 0 && ChunkBits[i].bit == bit)
		{
			return ChunkBits[i].label;
		}
	}
	return "";
}

static std::string ChoiceWords(char const *chunk, char const *field, int32_t value)
{
	for (size_t i = 0; i < ChunkChoiceCount; i++)
	{
		if (strcmp(ChunkChoices[i].chunk, chunk) == 0 && strcmp(ChunkChoices[i].field, field) == 0 && ChunkChoices[i].value == value)
		{
			return ChunkChoices[i].label;
		}
	}
	return "";
}

// THE LEVEL'S OWN SETTINGS (M7): what cencon edited, laid out as the game reads them, in cencon's words. (Added 2026-11-14 by
// Melanikus (Vasquez).)
static void LevelSettingsAreTheGames()
{
	printf("The level's settings: environment, generators and scale:\n");

	ChunkKind const *const environment = FindChunkKind("AVPENVIR");
	ChunkKind const *const generators = FindChunkKind("GLOGENDC");
	ChunkKind const *const scale = FindChunkKind("ENVSDSCL");
	Field const *const red = environment ? FindChunkField(*environment, "sky_colour_red") : 0;
	Field const *const green = environment ? FindChunkField(*environment, "sky_colour_green") : 0;
	Field const *const blue = environment ? FindChunkField(*environment, "sky_colour_blue") : 0;
	Field const *const kit = environment ? FindChunkField(*environment, "kit") : 0;
	Field const *const spears = environment ? FindChunkField(*environment, "predator_num_spears") : 0;
	Field const *const enemy = generators ? FindChunkField(*generators, "EnemyGenerated") : 0;
	Field const *const hive = generators ? FindChunkField(*generators, "HiveStateChangeTime") : 0;
	Field const *const scaleField = scale ? FindChunkField(*scale, "scale") : 0;

	if (!environment || !generators || !scale || !red || !green || !blue || !kit || !spears || !enemy || !hive || !scaleField)
	{
		Check(false, "the kinds and fields these checks use are in the tables");
		return;
	}

	Check(strcmp(environment->parent, "REBENVDT") == 0 && strcmp(generators->parent, "REBENVDT") == 0 && strcmp(scale->parent, "REBENVDT") == 0,
	      "the three are read only inside the level's environment data (REBENVDT)");

	// AVPENVIR, at the game's offsets (avp_chunk.hpp:257-281), in cencon's words
	Check(red->offset == 4 && green->offset == 8 && blue->offset == 12 && kit->offset == 16 && spears->offset == 20,
	      "the sky's red, green and blue are at +4, +8 and +12, the kit at +16 and the spears at +20");
	Check(strcmp(red->label, "Red") == 0 && strcmp(green->label, "Green") == 0 && strcmp(blue->label, "Blue") == 0 &&
	      strcmp(spears->label, "Spears") == 0, "they're labelled as cencon labels them");
	Check(strcmp(Verdict(*red, 255), "") == 0 && Verdict(*red, 256) == red->why && Verdict(*blue, -1) == blue->why,
	      "a sky colour is 0-255, as cencon takes it");
	char const *const kitWords[] = { "Pistol", "Plasma Caster", "Disc", "Grappling hook", "Medicomp", "Jetpack", "Stars" };
	bool kitNamed = true;
	for (unsigned b = 0; b < 7; b++)
	{
		kitNamed = BitWords("AVPENVIR", "kit", 1u << b) == kitWords[b] && kitNamed;
	}
	Check(kitNamed && kit->mask == 0x7f, "the kit's seven bits, low bit first, have cencon's words, and the spare bits aren't a choice");

	// Its first number is its own size: the game chooses its defaults by that number, not by the payload's size, and then writes the
	// block's real size over it (avp_chunk.cpp:577-606)
	std::vector<uint8_t> settings(24, 0);
	PutInt(settings, 0, 24);
	Check(SizeOf(*environment, settings) == SizeCheck::Right, "environment settings of 24 bytes that say 24 are read");
	std::vector<uint8_t> older(16, 0);
	PutInt(older, 0, 16);
	Check(SizeOf(*environment, older) == SizeCheck::Right, "an older 16-byte one that says 16 is read too: the game gives it the kit");
	PutInt(settings, 0, 28);
	Check(SizeOf(*environment, settings) == SizeCheck::WrongSize,
	      "one that says it holds more than it does is refused (its first number would choose the game's defaults, and then be written over)");
	PutInt(settings, 0, -1);
	Check(SizeOf(*environment, settings) == SizeCheck::WrongSize, "and so is one that says a negative size");
	Check(SizeOf(*environment, Bytes(3, 0, "", 0)) == SizeCheck::TooShort, "one too short to hold its own size is refused");

	// GLOGENDC: 28 bytes; the enemy is aliens or marines, the loader's own refusal
	Check(SizeOf(*generators, Bytes(28, 0, "", 0)) == SizeCheck::Right && SizeOf(*generators, Bytes(27, 0, "", 0)) == SizeCheck::TooShort,
	      "the level's generator settings are 28 bytes");
	Check(ChoiceWords("GLOGENDC", "EnemyGenerated", 0) == "Aliens" && ChoiceWords("GLOGENDC", "EnemyGenerated", 1) == "Marines" &&
	      enemy->limit == Limit::Loader && Verdict(*enemy, 2) == enemy->why && strcmp(enemy->label, "Enemy Generated") == 0,
	      "\"Enemy Generated\" is Aliens or Marines, and anything else is refused as the game's loader refuses it");
	Check(hive->offset == 16 && hive->unit == Unit::Seconds && strcmp(hive->label, "Hive State Change Time (seconds)") == 0,
	      "the hive's state change time is in seconds, at +16");

	// THE RANGES THE FORMS KEEP TO (2026-11-27, Vasquez; design-vasquez-oi-envgen.md, Drake's read at the avpx pin): each refused value gives
	// its own field's reason, and each reason is a different one, so a mapper reads why THIS number can't be that
	Field const *const most = FindChunkField(*generators, "MaxNPCSOnLevel");
	Field const *const perMinute = FindChunkField(*generators, "NPCSPerMinute");
	Field const *const change = FindChunkField(*generators, "NPCAcceleration");

	if (!most || !perMinute || !change)
	{
		Check(false, "the generator fields these checks use are in the table");
		return;
	}

	Check(spears->limit == Limit::Editor && strcmp(Verdict(*spears, 0), "") == 0 && strcmp(Verdict(*spears, 32767), "") == 0 &&
	      Verdict(*spears, 32768) == spears->why && Verdict(*spears, -1) == spears->why && spears->why[0] != 0,
	      "the spears are 0-32,767: the game multiplies the count by 65,536 into a signed number (inventory.cpp:621), so more overflows");
	Check(most->limit == Limit::Editor && strcmp(Verdict(*most, 0), "") == 0 && strcmp(Verdict(*most, 255), "") == 0 &&
	      Verdict(*most, 256) == most->why && Verdict(*most, -1) == most->why && most->why[0] != 0,
	      "Max NPCs Generated is 0-255, the game's own clamp (bh_generator.cpp:496-499)");
	Check(perMinute->limit == Limit::Editor && strcmp(Verdict(*perMinute, 0), "") == 0 && strcmp(Verdict(*perMinute, 255), "") == 0 &&
	      Verdict(*perMinute, 256) == perMinute->why && Verdict(*perMinute, -1) == perMinute->why && perMinute->why[0] != 0,
	      "NPCs Generated Per Minute is 0-255, the game's own clamp (bh_generator.cpp:500-503)");
	Check(change->limit == Limit::Editor && strcmp(Verdict(*change, 0), "") == 0 && strcmp(Verdict(*change, 255), "") == 0 &&
	      Verdict(*change, 256) == change->why && Verdict(*change, -1) == change->why && change->why[0] != 0,
	      "Change in Generation Rate Per Minute is 0-255, the game's own clamp (bh_generator.cpp:504-507)");
	Check(hive->limit == Limit::Editor && strcmp(Verdict(*hive, 1), "") == 0 && strcmp(Verdict(*hive, 4681), "") == 0 &&
	      Verdict(*hive, 4682) == hive->why && Verdict(*hive, 0) == hive->why && Verdict(*hive, -5) == hive->why && hive->why[0] != 0,
	      "the hive time is 1-4,681 s: the game works it out as (s x 65,536 x 7) / 8 in a signed number (bh_generator.cpp:675), so more overflows, "
	      "and at 0 it flips every frame");
	Check(strcmp(spears->why, most->why) != 0 && strcmp(most->why, perMinute->why) != 0 && strcmp(perMinute->why, change->why) != 0 &&
	      strcmp(change->why, hive->why) != 0 && strcmp(hive->why, red->why) != 0, "each of those ranges has its own reason");

	// ENVSDSCL: one double, held to the loader's own 0.5-1 and real-number rule (environment_chunk.cpp:161-169)
	Check(SizeOf(*scale, Bytes(8, 0, "", 0)) == SizeCheck::Right && SizeOf(*scale, Bytes(7, 0, "", 0)) == SizeCheck::TooShort &&
	      scaleField->storage == Storage::Float64, "the scale is 8 bytes: one double");
	Check(strcmp(Verdict(*scaleField, 0.5), "") == 0 && strcmp(Verdict(*scaleField, 1.0), "") == 0 && Verdict(*scaleField, 0.49) == scaleField->why &&
	      Verdict(*scaleField, 1.01) == scaleField->why && Verdict(*scaleField, std::numeric_limits<double>::quiet_NaN()) == NotANumberReason,
	      "a scale is a real number from 0.5 to 1, as the loader holds it");
	Check(scaleField->limit == Limit::Loader, "and that limit is the loader's own, at the pinned commit, not a planned one");
}

// MODULE FLAGS: inteng's flag screen, laid out as the game reads MODFLAGS. (Added 2026-11-14 by Melanikus (Vasquez).)
static void ModuleFlagsAreInteng()
{
	printf("Module flags, as inteng's flag screen shows them:\n");

	ChunkKind const *const flags = FindChunkKind("MODFLAGS");
	Field const *const word = flags ? FindChunkField(*flags, "Flags") : 0;
	Field const *const message = flags ? FindChunkField(*flags, "message") : 0;

	if (!flags || !word || !message)
	{
		Check(false, "the kind and fields these checks use are in the tables");
		return;
	}

	Check(strcmp(flags->parent, "MODULEDT") == 0 && SizeOf(*flags, Bytes(8, 0, "", 0)) == SizeCheck::Right &&
	      SizeOf(*flags, Bytes(7, 0, "", 0)) == SizeCheck::TooShort, "a module's flags are 8 bytes, read only in its MODULEDT");

	struct Named { uint32_t bit; char const *words; };
	Named const named[] = { { 0x80000000, "Airduct" }, { 0x40000000, "Stair" }, { 0x20000000, "Sky Visible" }, { 0x10000000, "Fogged" },
	                        { 0x00100000, "Not Passable" }, { 0x00080000, "Not a Doorway" }, { 0x00040000, "Doorway" }, { 0x00020000, "Sneaky" },
	                        { 0x00010000, "Victory Zone" }, { 0x00000200, "Exclude From Map" } };
	bool allNamed = true;
	uint32_t all = 0;
	for (Named const &n : named)
	{
		allNamed = BitWords("MODFLAGS", "Flags", n.bit) == n.words && allNamed;
		all |= n.bit;
	}
	Check(allNamed && word->mask == all, "the ten flags inteng names have its own words, and they're the only flag bits a mapper sets");

	Check(message->offset == 0 && message->mask == 0x0FE00000 && (word->mask & message->mask) == 0,
	      "the message number is bits 21-27 of the same word, apart from the flags");
	Check(strcmp(Verdict(*message, 127u << 21), "") == 0 && strcmp(Verdict(*message, 0), "") == 0 && Verdict(*message, 0x00100000) == DoesNotFitReason,
	      "a message is 0 (none) to 127, stored as the number times 2^21, and nothing outside those bits");
	Check((message->mask & 0x08000000) != 0,
	      "the game's horizontal-door bit (0x08000000) is one of the message's bits, the clash fields-spec.md names");

	// The loader refuses a flag word with any bit outside FlagsALevelMayGiveAModule (chunk_object.cpp:833, :842-845)
	uint32_t const loaderMask = 0xFFFF0200u;
	Check((word->mask | message->mask) == loaderMask,
	      "the named flags and the message number together are exactly the bits the loader lets a level give a module");
}

// A payload that starts with a name, as the game's writer makes it (fill_data_block): the name's letters, its zero, and zeros up to a
// multiple of 4 bytes, then these numbers, then extra zero bytes after them, which the game ignores.
static std::vector<uint8_t> NameThen(char const *name, std::vector<int32_t> const &numbers, size_t extra = 0)
{
	size_t const letters = strlen(name);
	size_t const padded = (letters + 4) & ~(size_t)3;
	std::vector<uint8_t> bytes(padded + 4 * numbers.size() + extra, 0);
	memcpy(bytes.data(), name, letters);

	for (size_t i = 0; i < numbers.size(); i++)
	{
		PutInt(bytes, padded + 4 * i, numbers[i]);
	}

	return bytes;
}

// THE FRAGMENT TYPES (L1): a type's name, the shapes it breaks into, and the sound it makes, laid out as the game reads them
// (chunk_fragments.cpp), with cencon's word for the quantity. (Added 2026-11-14 by Melanikus (Vasquez).)
static void FragmentsAreTheGames()
{
	printf("Fragment types: a name first, then the numbers counted from its end:\n");

	ChunkKind const *const type = FindChunkKind("FRGTYPDC");
	ChunkKind const *const shape = FindChunkKind("FRGTYPSC");
	ChunkKind const *const sound = FindChunkKind("FRGSOUND");
	Field const *const typePad1 = type ? FindChunkField(*type, "pad1") : 0;
	Field const *const quantity = shape ? FindChunkField(*shape, "num_fragments") : 0;
	Field const *const location = shape ? FindChunkField(*shape, "location") : 0;
	Field const *const shapePad3 = shape ? FindChunkField(*shape, "pad3") : 0;
	Field const *const inner = sound ? FindChunkField(*sound, "inner_range") : 0;
	Field const *const outer = sound ? FindChunkField(*sound, "outer_range") : 0;
	Field const *const volume = sound ? FindChunkField(*sound, "max_volume") : 0;
	Field const *const pitch = sound ? FindChunkField(*sound, "pitch") : 0;
	Field const *const soundPad = sound ? FindChunkField(*sound, "pad") : 0;

	if (!type || !shape || !sound || !typePad1 || !quantity || !location || !shapePad3 || !inner || !outer || !volume || !pitch || !soundPad)
	{
		Check(false, "the kinds and fields these checks use are in the tables");
		return;
	}

	Check(strcmp(type->parent, "FRAGTYPE") == 0 && strcmp(shape->parent, "FRAGTYPE") == 0 && sound->parent[0] == 0,
	      "a type's data and its shapes are read only inside the type; a fragment sound is read inside a type and inside a shape, so under no one parent");
	Check(type->nameFirst && shape->nameFirst && !sound->nameFirst && type->fixedBytes == 12 && shape->fixedBytes == 28 &&
	      sound->fixedBytes == 20,
	      "a type's data and a shape start with a name and hold 12 and 28 bytes of numbers after it; a sound holds 20 before its name");

	// The type's data: its name, padded, then three spare numbers
	Check(SizeOf(*type, NameThen("office", { 0, 0, 0 })) == SizeCheck::Right && SizeOf(*type, NameThen("", { 0, 0, 0 })) == SizeCheck::Right,
	      "a type's data, as the game writes it, is the right size, even with no name");
	Check(SizeOf(*type, NameThen("abc", { 0, 0, 0 }, 8)) == SizeCheck::Right, "bytes after its numbers are ignored, as the game ignores them");
	Check(SizeOf(*type, std::vector<uint8_t>(10, 'a')) == SizeCheck::NameCutOff && SizeOf(*type, std::vector<uint8_t>()) == SizeCheck::NameCutOff,
	      "a name with no ending zero inside the chunk is refused, as the game refuses it");
	std::vector<uint8_t> shortType = NameThen("abc", { 0, 0, 0 });
	shortType.pop_back();
	Check(SizeOf(*type, shortType) == SizeCheck::TooShort, "a type's data with its last number cut short is refused");

	// "The name's length plus 4, rounded down to a multiple of 4": a name of 4 letters takes 8 bytes, one of 3 letters takes 4
	std::vector<uint8_t> unpadded(5 + 12, 0);
	memcpy(unpadded.data(), "abcd", 4);
	Check(SizeOf(*type, unpadded) == SizeCheck::TooShort && SizeOf(*type, NameThen("abcd", { 0, 0, 0 })) == SizeCheck::Right,
	      "the name's padding is stepped over: numbers straight after a 4-letter name and its zero are 3 bytes short");
	Check(SizeOf(*type, Bytes(3, 0, "ab", 2)) == SizeCheck::TooShort, "a name whose padding runs off the chunk leaves no room for the numbers");

	// Where the numbers are
	std::vector<uint8_t> three = NameThen("abc", { 11, 22, 33, 44, 55, 66, 77 });
	std::vector<uint8_t> four = NameThen("abcd", { 11, 22, 33, 44, 55, 66, 77 });
	size_t place = 0;
	Check(ChunkFieldPlace(*shape, *quantity, three.data(), three.size(), &place) && place == 4 &&
	      ChunkFieldPlace(*shape, *quantity, four.data(), four.size(), &place) && place == 8,
	      "a shape's quantity is straight after a 3-letter name's 4 bytes, and after a 4-letter name's 8");
	Check(ChunkFieldPlace(*shape, *location, four.data(), four.size(), &place) && place == 12 &&
	      ChunkFieldPlace(*shape, *shapePad3, four.data(), four.size(), &place) && place == 32,
	      "its location is 4 bytes and its last spare 24 bytes after that place");
	place = 12345;
	Check(!ChunkFieldPlace(*shape, *quantity, std::vector<uint8_t>(10, 'a').data(), 10, &place) && place == 12345,
	      "a field of a chunk whose name has no ending zero has no place, and the place is left alone");
	Check(!ChunkFieldPlace(*shape, *quantity, four.data(), 11, &place) && place == 12345,
	      "a field that would fall past the end of the chunk has no place");
	Check(!ChunkFieldPlace(*shape, *typePad1, four.data(), four.size(), &place), "a field of another kind has no place in this one");

	// A shape: its name, then the quantity, the location and three spare numbers
	Check(SizeOf(*shape, three) == SizeCheck::Right && SizeOf(*shape, NameThen("Nost03", { 4, 0, 0, 0, 0, 0 })) == SizeCheck::TooShort &&
	      SizeOf(*shape, NameThen("Nost03", { 4, 0, 0, 0, 0, 0, 0 })) == SizeCheck::Right,
	      "a shape's numbers are 28 bytes after its name, and 24 are too few");

	// Setting them
	char const *why = 0;
	std::vector<uint8_t> const beforeThree = three;
	Check(SetChunkField(*shape, *quantity, four.data(), four.size(), 12, &why) && GetUInt(four, 8) == 12 && GetUInt(four, 4) == 0 &&
	      GetUInt(four, 12) == 22,
	      "setting a shape's quantity writes it after the name's padding and changes nothing else");
	Check(SetChunkFieldAxis(*shape, *location, four.data(), four.size(), 2, 1000, &why) && GetUInt(four, 20) == 1000 && GetUInt(four, 16) == 33,
	      "setting one axis of its location writes that axis alone");
	std::vector<uint8_t> const beforeBad = four;
	std::vector<uint8_t> noZero(40, 'a');
	std::vector<uint8_t> const beforeNoZero = noZero;
	Check(!SetChunkField(*shape, *quantity, noZero.data(), noZero.size(), 1, &why) && noZero == beforeNoZero,
	      "a number isn't written into a chunk whose name has no ending zero");
	Check(!SetChunkField(*shape, *typePad1, three.data(), three.size(), 1, &why) && three == beforeThree,
	      "another kind's field isn't written into a shape, at its own offset or any other");
	why = 0;
	Check(!SetChunkField(*shape, *quantity, four.data(), four.size(), 751, &why) && why == quantity->why && four == beforeBad,
	      "a refused value changes nothing, and the reason is the field's own");

	// The numbers' limits
	Check(strcmp(quantity->label, "Quantity") == 0 && quantity->limit == Limit::GameMeaning && strcmp(Verdict(*quantity, 0), "") == 0 &&
	      strcmp(Verdict(*quantity, 750), "") == 0 && Verdict(*quantity, 751) == quantity->why && Verdict(*quantity, -1) == quantity->why,
	      "a quantity is cencon's \"Quantity\", from 0 to the 750 pieces a level has room for");
	Check(strcmp(Verdict(*quantity, 30), "") == 0, "and the most any of the game's own levels uses, 30, is allowed");
	Check(location->limit == Limit::Loader && strcmp(Verdict(*location, 16777216), "") == 0 && Verdict(*location, 16777217) == location->why &&
	      Verdict(*location, -16777217) == location->why,
	      "a shape's location is held to the loader's own 2^24 on each axis, though the game never uses it");
	Check(typePad1->limit == Limit::None && shapePad3->limit == Limit::None && typePad1->label[0] == 0 && shapePad3->label[0] == 0,
	      "the spare numbers take any value and have no label, so the editor keeps them as they are");

	// The sound: 20 bytes of numbers, then its wav file's name
	Check(SizeOf(*sound, Bytes(19, 0, "", 0)) == SizeCheck::TooShort && SizeOf(*sound, Bytes(20, 0, "", 0)) == SizeCheck::NameCutOff &&
	      SizeOf(*sound, Bytes(21, 0, "", 0)) == SizeCheck::Right,
	      "a fragment sound under 20 bytes is refused as too short, and one with no room for its name's zero is refused");
	Check(SizeOf(*sound, Bytes(23, 20, "abc", 3)) == SizeCheck::NameCutOff && SizeOf(*sound, Bytes(24, 20, "abc", 3)) == SizeCheck::Right,
	      "its wav name needs its ending zero inside the chunk, and nothing more");
	Check(inner->offset == 0 && outer->offset == 4 && volume->offset == 8 && pitch->offset == 12 && soundPad->offset == 16 &&
	      inner->storage == Storage::UInt32 && outer->storage == Storage::UInt32,
	      "a sound's two reaches are unsigned numbers at +0 and +4, then its volume, pitch and a spare");
	Check(strcmp(Verdict(*volume, 127), "") == 0 && Verdict(*volume, 128) == volume->why && Verdict(*volume, -1) == volume->why &&
	      strcmp(Verdict(*pitch, -6144), "") == 0 && Verdict(*pitch, 6145) == pitch->why,
	      "its volume and pitch are kept to what the game plays, as a placed sound's are");
	Check(strcmp(Verdict(*outer, 100000), "") == 0 && strcmp(Verdict(*inner, 0), "") == 0 && Verdict(*inner, -1) == DoesNotFitReason,
	      "its reach takes the game's own 100000 and can't be negative");
	Check(inner->hasCreation && inner->creation == 5000 && outer->hasCreation && outer->creation == 40000 && volume->hasCreation &&
	      volume->creation == 127 && pitch->hasCreation && pitch->creation == 0 && soundPad->hasCreation && soundPad->creation == 0,
	      "a new fragment sound starts as the chunk library makes it: 5000, 40000, volume 127, no pitch");
	std::vector<uint8_t> soundBytes(24, 0x11);
	Check(SetChunkField(*sound, *volume, soundBytes.data(), soundBytes.size(), 100, &why) && GetUInt(soundBytes, 8) == 100 &&
	      GetUInt(soundBytes, 4) == 0x11111111 && GetUInt(soundBytes, 12) == 0x11111111,
	      "setting a sound's volume writes it at +8 and changes nothing else");
}

// A set of random textures as the game's writer makes it (fill_data_block): the name padded, the count, the items, the two spares.
static std::vector<uint8_t> RandomTextures(char const *name, std::vector<int32_t> const &items, int32_t spare1, int32_t spare2, size_t extra = 0)
{
	std::vector<int32_t> numbers;
	numbers.push_back((int32_t)items.size());
	numbers.insert(numbers.end(), items.begin(), items.end());
	numbers.push_back(spare1);
	numbers.push_back(spare2);
	return NameThen(name, numbers, extra);
}

// THE RANDOM TEXTURES (L1): a set's name, a list of numbers, and two spares, laid out as the game reads them (environment_chunk.cpp:276-298).
// (Added 2026-11-14 by Melanikus (Vasquez).)
static void RandomTexturesAreTheGames()
{
	printf("Random textures: a name, a list counted from its end, and two numbers after the list:\n");

	ChunkKind const *const set = FindChunkKind("RANTEXID");
	Field const *const spare1 = set ? FindChunkField(*set, "spare1") : 0;
	Field const *const spare2 = set ? FindChunkField(*set, "spare2") : 0;

	if (!set || !spare1 || !spare2)
	{
		Check(false, "the kind and fields these checks use are in the tables");
		return;
	}

	Check(strcmp(set->parent, "REBENVDT") == 0 && set->nameFirst && set->listCountAt == 0 && set->listItemBytes == 4 && set->trailerBytes == 8 &&
	      set->fixedBytes == 4,
	      "a set of random textures is read inside the environment data: its name, a count, 4-byte items and 8 bytes after them");
	Check(spare1->anchor == Anchor::AfterFirstList && spare1->offset == 0 && spare2->anchor == Anchor::AfterFirstList && spare2->offset == 4,
	      "its two spares are counted from the end of the list");

	// Sizes
	Check(SizeOf(*set, RandomTextures("hnpc_civvie", {}, 0, 0)) == SizeCheck::Right && RandomTextures("hnpc_civvie", {}, 0, 0).size() == 24 &&
	      SizeOf(*set, RandomTextures("hnpcmarine", {}, 0, 0)) == SizeCheck::Right,
	      "a set with no items is 24 bytes after a name of 10 or 11 letters, as the game's own eight are, and the right size");
	Check(SizeOf(*set, RandomTextures("hnpc_civvie", { 1, 3, 5 }, 0, 0)) == SizeCheck::Right &&
	      SizeOf(*set, RandomTextures("hnpc_civvie", { 1, 3, 5 }, 0, 0, 12)) == SizeCheck::Right,
	      "a set with items is the right size, and bytes after its spares are ignored, as the game ignores them");
	Check(SizeOf(*set, std::vector<uint8_t>(30, 'a')) == SizeCheck::NameCutOff && SizeOf(*set, std::vector<uint8_t>()) == SizeCheck::NameCutOff,
	      "a name with no ending zero is refused");
	Check(SizeOf(*set, Bytes(14, 0, "hnpc_civvie", 11)) == SizeCheck::CountCutOff && SizeOf(*set, Bytes(12, 0, "hnpc_civvie", 11)) == SizeCheck::CountCutOff,
	      "a set that ends before its count has room is refused with a reason of a list's");
	std::vector<uint8_t> shortSpares = RandomTextures("hnpc_civvie", { 1, 3, 5 }, 0, 0);
	shortSpares.pop_back();
	Check(SizeOf(*set, shortSpares) == SizeCheck::TooShort, "a set whose last spare is cut short is refused");
	std::vector<uint8_t> counted = RandomTextures("hnpc_civvie", { 7, 8 }, 0, 0);
	PutInt(counted, 12, 5);
	Check(SizeOf(*set, counted) == SizeCheck::CountTooBig, "a count of 5 items with room for 4 is refused as too big");
	PutInt(counted, 12, 3);
	Check(SizeOf(*set, counted) == SizeCheck::TooShort,
	      "a count of 3 that takes the first spare for an item is read, and then refused: the game finds only one spare left");
	PutInt(counted, 12, -1);
	Check(SizeOf(*set, counted) == SizeCheck::CountNegative, "a negative count is refused");

	// Where the spares are, whatever the list holds
	std::vector<uint8_t> two = RandomTextures("hnpc_civvie", { 7, 8 }, 11, 22);
	std::vector<uint8_t> none = RandomTextures("hnpcmarine", {}, 33, 44);
	size_t place = 0;
	Check(ChunkFieldPlace(*set, *spare1, two.data(), two.size(), &place) && place == 24 &&
	      ChunkFieldPlace(*set, *spare2, two.data(), two.size(), &place) && place == 28,
	      "after two items, the spares are 12 + 4 + 8 bytes into the payload");
	Check(ChunkFieldPlace(*set, *spare1, none.data(), none.size(), &place) && place == 16 &&
	      ChunkFieldPlace(*set, *spare2, none.data(), none.size(), &place) && place == 20,
	      "with no items, they follow the count");
	place = 12345;
	PutInt(two, 12, 9);
	Check(!ChunkFieldPlace(*set, *spare1, two.data(), two.size(), &place) && place == 12345,
	      "a spare of a set whose count is more than it holds has no place, and the place is left alone");
	Check(!ChunkFieldPlace(*set, *spare1, std::vector<uint8_t>(20, 'a').data(), 20, &place), "nor has a spare of a set whose name has no zero");

	// Setting them
	char const *why = 0;
	std::vector<uint8_t> items = RandomTextures("hnpc_civvie", { 7, 8, 9 }, 11, 22);
	Check(SetChunkField(*set, *spare2, items.data(), items.size(), -5, &why) && GetUInt(items, 12 + 4 + 12 + 4) == (uint32_t)-5 &&
	      GetUInt(items, 12 + 4 + 12) == 11 && GetUInt(items, 12) == 3 && GetUInt(items, 12 + 4 + 8) == 9,
	      "setting the second spare writes it after the last item and changes no item, count or spare");
	Check(spare1->limit == Limit::None && spare1->label[0] == 0 && spare1->hasCreation && spare1->creation == 0 && spare2->hasCreation &&
	      spare2->creation == 0,
	      "the spares take any value, have no label, and a new set starts them at 0, as the chunk library makes one");
}

// A game mode's header as the game's writer makes it (fill_data_block, chunk_palette.cpp:637-669): the 20 bytes, the mode's name, each .rif
// file's name, and the empty name that ends the list, then extra zero bytes of padding.
static std::vector<uint8_t> GameModeHeader(uint32_t flags, int32_t version, char const *mode, std::vector<std::string> const &rifs, size_t extra = 0)
{
	std::vector<uint8_t> bytes(20, 0);
	PutInt(bytes, 0, (int32_t)flags);
	PutInt(bytes, 16, version);
	bytes.insert(bytes.end(), mode, mode + strlen(mode) + 1);

	for (std::string const &rif : rifs)
	{
		bytes.insert(bytes.end(), rif.c_str(), rif.c_str() + rif.size() + 1);
	}

	bytes.push_back(0);
	bytes.resize(bytes.size() + extra, 0);
	return bytes;
}

// THE GAME MODES (L1): a mode's header, laid out as the game reads it (chunk_palette.cpp:588-627), with the game's words for its flags.
// (Added 2026-11-14 by Melanikus (Vasquez).)
static void GameModesAreTheGames()
{
	printf("Game modes: a header's numbers, its mode's name, and the list of .rif files:\n");

	ChunkKind const *const header = FindChunkKind("GMODHEAD");
	Field const *const flags = header ? FindChunkField(*header, "flags") : 0;
	Field const *const reserved1 = header ? FindChunkField(*header, "reserved1") : 0;
	Field const *const reserved3 = header ? FindChunkField(*header, "reserved3") : 0;
	Field const *const version = header ? FindChunkField(*header, "version_num") : 0;

	if (!header || !flags || !reserved1 || !reserved3 || !version)
	{
		Check(false, "the kind and fields these checks use are in the tables");
		return;
	}

	Check(strcmp(header->parent, "GAMEMODE") == 0 && header->leastBytes == 20 && header->fixedBytes == 20 && header->namesRequired == 1 &&
	      header->namesOptional == 0 && header->namesToEmpty && !header->nameFirst && header->listCountAt < 0,
	      "a game mode's header is read only inside its game mode: 20 bytes, its name, then a list of names");
	Check(flags->offset == 0 && reserved1->offset == 4 && reserved3->offset == 12 && version->offset == 16,
	      "its flags are at +0, its three reserved numbers at +4, +8 and +12, and its version number at +16");

	// Sizes
	Check(SizeOf(*header, GameModeHeader(1, 2, "multip", {})) == SizeCheck::Right && GameModeHeader(1, 2, "multip", {}).size() == 28,
	      "a header with no .rif files is 28 bytes after a mode name of 6 letters, as the game's own are, and the right size");
	Check(SizeOf(*header, GameModeHeader(1, 3, "marine", { "a.rif", "b\\c.rif" })) == SizeCheck::Right &&
	      SizeOf(*header, GameModeHeader(1, 3, "marine", { "a.rif" }, 3)) == SizeCheck::Right,
	      "a header with .rif files is the right size, with or without padding");
	Check(SizeOf(*header, Bytes(19, 0, "", 0)) == SizeCheck::TooShort && SizeOf(*header, Bytes(20, 0, "", 0)) == SizeCheck::NameCutOff &&
	      SizeOf(*header, Bytes(21, 0, "", 0)) == SizeCheck::Right,
	      "a header under 20 bytes is refused as too short, and one with no room for its mode's name is refused");
	Check(SizeOf(*header, Bytes(24, 20, "abcd", 4)) == SizeCheck::NameCutOff && SizeOf(*header, Bytes(25, 20, "abcd", 4)) == SizeCheck::Right,
	      "a mode's name needs its ending zero inside the chunk");

	// The list of names: it ends at an empty name, or at the chunk's end
	std::vector<uint8_t> noEnd = GameModeHeader(1, 2, "mode", { "a.rif" });
	noEnd.pop_back();
	Check(SizeOf(*header, noEnd) == SizeCheck::Right, "a list that reaches the end of the chunk with no empty name is read");
	std::vector<uint8_t> afterEnd = GameModeHeader(1, 2, "mode", { "a.rif" });
	afterEnd.push_back('x');
	afterEnd.push_back('y');
	Check(SizeOf(*header, afterEnd) == SizeCheck::Right, "what comes after the empty name is ignored, even letters with no ending zero");
	std::vector<uint8_t> cutOff = GameModeHeader(1, 2, "mode", { "a.rif" });
	cutOff.pop_back();
	cutOff.pop_back();
	Check(SizeOf(*header, cutOff) == SizeCheck::NameCutOff, "a .rif name with no ending zero inside the chunk is refused");
	std::vector<uint8_t> second = GameModeHeader(1, 2, "mode", { "a.rif", "b.rif" });
	second.pop_back();
	second.pop_back();
	Check(SizeOf(*header, second) == SizeCheck::NameCutOff, "and so is a second one, which the first doesn't excuse");

	// The flags, in the game's words
	Check(BitWords("GMODHEAD", "flags", 0x1) == "Editable" && BitWords("GMODHEAD", "flags", 0x2) == "Deleted" &&
	      BitWords("GMODHEAD", "flags", 0x4) == "Added" && flags->mask == 0x7,
	      "the flags have the game's own words for its three bits, and no other bit is a choice");
	Check(strcmp(Verdict(*flags, 0x1), "") == 0 && strcmp(Verdict(*flags, 0x7), "") == 0 && Verdict(*flags, 0x8) == DoesNotFitReason,
	      "a flag value outside those bits is refused");
	char const *why = 0;
	std::vector<uint8_t> bytes = GameModeHeader(0x100, 2, "mode", {});
	Check(SetChunkField(*header, *flags, bytes.data(), bytes.size(), 0x5, &why) && GetUInt(bytes, 0) == 0x105 && GetUInt(bytes, 16) == 2,
	      "setting the flags keeps the bits that aren't its three, and changes no other number");
	Check(SetChunkField(*header, *version, bytes.data(), bytes.size(), 7, &why) && GetUInt(bytes, 16) == 7 && GetUInt(bytes, 0) == 0x105,
	      "setting the version number writes it at +16");
	Check(flags->limit == Limit::None && version->limit == Limit::None && version->label[0] == 0 && reserved1->label[0] == 0,
	      "the game takes any version and reserved number, and the editor keeps them as they are");
}

// THE SOUND FOLDER (L1): a name and nothing else (environment_chunk.cpp:241-245). (Added 2026-11-14 by Melanikus (Vasquez).)
static void SoundFolderIsTheGames()
{
	printf("The sound folder: a name, and no numbers:\n");

	ChunkKind const *const folder = FindChunkKind("SOUNDDIR");

	if (!folder)
	{
		Check(false, "the kind these checks use is in the table");
		return;
	}

	Check(strcmp(folder->parent, "REBENVDT") == 0 && folder->fieldCount == 0 && folder->fixedBytes == 0 && folder->leastBytes == 0 &&
	      folder->namesRequired == 1 && folder->namesOptional == 0 && !folder->nameFirst && !folder->namesToEmpty,
	      "the sound folder is read only inside the environment data, and holds just its name, with no number");
	Check(SizeOf(*folder, std::vector<uint8_t>()) == SizeCheck::NameCutOff && SizeOf(*folder, Bytes(8, 0, "env\\Fall", 8)) == SizeCheck::NameCutOff,
	      "a sound folder with no bytes, or whose name has no ending zero inside the chunk, is refused, as the game refuses it");
	Check(SizeOf(*folder, Bytes(9, 0, "env\\Fall", 8)) == SizeCheck::Right && SizeOf(*folder, Bytes(12, 0, "env\\Fall", 8)) == SizeCheck::Right &&
	      SizeOf(*folder, Bytes(40, 0, "env\\Fall", 8)) == SizeCheck::Right,
	      "a sound folder with its name's zero is read, however many bytes follow, as the game's own, padded to 4 bytes, are");
	Check(SizeOf(*folder, Bytes(1, 0, "", 0)) == SizeCheck::Right, "an empty folder name is the right size (the name rule, not the size rule, says it's needed)");
	Check(strcmp(ChunkSizeCheckWords(SizeCheck::NameCutOff), "a name at the end of the chunk has no ending zero inside it") == 0,
	      "the refusal is said in words");
}

// A SETTER GIVEN NO PAYLOAD refuses it before it looks for a place. Finding a place can read the payload (a leading name, a list's count),
// and storing writes through it, so a null payload would end the program. Each check that would crash if the guard were gone runs under
// Survives (crash_guard.h), so a lost guard fails that check by name and the others still run. (Added 2026-11-15 by Melanikus (Vasquez), at
// Drake's catch: the chunk readers had the guard, the setters didn't.)
static void SettersRefuseANullPayload()
{
	printf("The setters refuse a null payload, and never go through it:\n");

	ChunkKind const *const light = FindChunkKind("STDLIGHT");
	ChunkKind const *const shape = FindChunkKind("FRGTYPSC");
	ChunkKind const *const set = FindChunkKind("RANTEXID");
	Field const *const blue = light ? FindChunkField(*light, "colour_blue") : 0;
	Field const *const range = light ? FindChunkField(*light, "range") : 0;
	Field const *const lightPlace = light ? FindChunkField(*light, "location") : 0;
	Field const *const quantity = shape ? FindChunkField(*shape, "num_fragments") : 0;
	Field const *const shapePlace = shape ? FindChunkField(*shape, "location") : 0;
	Field const *const spare = set ? FindChunkField(*set, "spare1") : 0;

	if (!light || !shape || !set || !blue || !range || !lightPlace || !quantity || !shapePlace || !spare)
	{
		Check(false, "the kinds and fields these checks use are in the tables");
		return;
	}

	// The lengths are real ones, so that without the guard the setter would go on to read or write the payload it doesn't have
	char const *why = 0;
	bool refused = false;

	Check(Survives([&] { refused = !SetChunkField(*light, *blue, nullptr, 84, 200, &why); }) && refused,
	      "a light's blue byte isn't set in no payload (its place is counted from the start, and storing would write through the null)");
	refused = false;
	Check(Survives([&] { refused = !SetChunkField(*shape, *quantity, nullptr, 40, 5, &why); }) && refused,
	      "a shape's quantity isn't set in no payload (finding its place would read the null for the name)");
	refused = false;
	Check(Survives([&] { refused = !SetChunkField(*set, *spare, nullptr, 40, 5, &why); }) && refused,
	      "a random texture set's spare isn't set in no payload (finding its place would read the null for the name and the count)");
	refused = false;
	Check(Survives([&] { refused = !SetChunkFieldAxis(*light, *lightPlace, nullptr, 84, 1, 5000, &why); }) && refused,
	      "one axis of a light's position isn't set in no payload");
	refused = false;
	Check(Survives([&] { refused = !SetChunkFieldAxis(*shape, *shapePlace, nullptr, 40, 0, 5, &why); }) && refused,
	      "one axis of a shape's location isn't set in no payload (finding its place would read the null for the name)");

	// A wrong value still gets its reason with no payload, so a caller can show it
	why = 0;
	refused = false;
	Check(Survives([&] { refused = !SetChunkField(*light, *range, nullptr, 84, 0, &why); }) && refused && why == range->why,
	      "a wrong value is still given the field's own reason when there is no payload");
}

// THE LIMITS THE PINNED LOADER ENFORCES ITSELF are Loader limits, each cited at the line of its check (AvPx 491deb22: chunk.hpp:488-545,
// chunk_light.cpp:121-150 and :377, chunk_sound.cpp:55, avp_chunk.cpp:80, :309-320, :817-839 and :455). (Added 2026-11-14 by Melanikus (Vasquez).)
static void OlderKindsFollowThePinnedLoader()
{
	printf("The limits the pinned loader enforces:\n");

	struct Expected { char const *chunk; char const *field; };
	Expected const loader[] = { { "STDLIGHT", "location" }, { "STDLIGHT", "range" }, { "PLOBJLIT", "range" }, { "SOUNDOB2", "position" },
	                            { "AVPGENER", "location" }, { "GENEXSET", "PulseMarine_Wt" }, { "GENEXSET", "PistolMarine_Wt" },
	                            { "PARGENDA", "type" }, { "PARGENDA", "location" }, { "PARGENDA", "orientation_x" },
	                            { "PARGENDA", "orientation_w" }, { "AVPSTART", "location" } };
	bool allLoader = true;
	for (Expected const &e : loader)
	{
		ChunkKind const *const kind = FindChunkKind(e.chunk);
		Field const *const field = kind ? FindChunkField(*kind, e.field) : 0;
		bool const isLoader = field && field->limit == Limit::Loader;
		if (!isLoader)
		{
			printf("    not a loader limit: %s %s\n", e.chunk, e.field);
		}
		allLoader = isLoader && allLoader;
	}
	Check(allLoader, "each limit the pinned loader's reader enforces itself is a loader limit, not a planned or an editor one");
}

int main()
{
	TablesHoldTogether();
	SizesAreTheGames();
	ValuesAreChecked();
	SettersWriteOnlyTheirPart();
	SoundsAreTheGames();
	MarkersAreTheGames();
	LevelSettingsAreTheGames();
	ModuleFlagsAreInteng();
	FragmentsAreTheGames();
	RandomTexturesAreTheGames();
	GameModesAreTheGames();
	SoundFolderIsTheGames();
	SettersRefuseANullPayload();
	OlderKindsFollowThePinnedLoader();

	printf("%s: %d check(s) failed\n", checksFailed ? "FAILED" : "PASSED", checksFailed);
	return checksFailed ? 1 : 0;
}
