// The test for the field model's chunk readers (ReadChunkField, ReadChunkFieldAxis, ReadChunkFieldObject) and for FindAnyChunkKind: each
// reader gives back exactly what its setter put in, and refuses what it must. Written 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It runs none of the editor and reads no file. It builds each kind's payload in memory, puts values in with the setters, and reads them
// back. The program gives back a non-zero result if any check failed. Each check's words say what it proves.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "chunk_payloads.h"
#include "crash_guard.h"
#include "field_model.h"
#include "real_numbers.h"

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

// A payload the game's reader accepts, with every number 0: a name first where the kind starts with one, the kind's fixed part with an
// empty list, the numbers after the list, its names (each just an ending zero), and a block's own size where the kind has one
// (chunk_payloads.h)
static std::vector<uint8_t> Valid(ChunkKind const &kind)
{
	return ValidChunkPayload(kind);
}

static bool IsAScalar(Field const &field)
{
	return field.storage != Storage::IntTriple && field.storage != Storage::ObjectId && field.storage != Storage::IntMatrix;
}

// Whether a value is held exactly by a field's storage: a float32 holds only the fractions that survive a round trip through one
static bool HeldExactly(Field const &field, double value)
{
	return field.storage != Storage::Float32 || (double)(float)value == value;
}

static void FindAny()
{
	printf("FindAnyChunkKind:\n");

	bool everyKindFound = true;
	bool namesAreTheBase = true;

	for (size_t k = 0; k < ChunkKindCount; k++)
	{
		ChunkNames names = { 0, 0, 0, 0 };
		ChunkKind const *const found = FindAnyChunkKind(ChunkKinds[k].id, &names);

		everyKindFound = everyKindFound && found == &ChunkKinds[k];
		namesAreTheBase = namesAreTheBase && names.bits == ChunkBits && names.bitCount == ChunkBitCount && names.choices == ChunkChoices &&
		                  names.choiceCount == ChunkChoiceCount;
	}

	Check(ChunkKindCount > 0 && everyKindFound, "every kind of chunk_fields.cpp's table is found by its 8-letter name, and it's the same kind");
	Check(namesAreTheBase, "each comes with the table's own lists of named bits and named choices");

	ChunkNames names = { ChunkBits, 1, ChunkChoices, 1 };
	Check(FindAnyChunkKind("NOSUCHCH", &names) == 0 && names.bits == 0 && names.bitCount == 0 && names.choices == 0 && names.choiceCount == 0,
	      "a chunk no family has gives no kind, and names with nothing in them");
	Check(FindAnyChunkKind("STDLIGH", 0) == 0, "a name of 7 letters and an ending zero matches nothing");
	Check(FindAnyChunkKind("STDLIGHT", 0) == FindChunkKind("STDLIGHT"), "no place for the names is fine");
}

// Each kind: every one-number field reads back what was set, every three-number field its three numbers, every object field its two
// halves, and a list's count reads as it is.
static void EveryKindReadsBack()
{
	printf("Every kind of chunk reads back what was put in:\n");

	for (size_t k = 0; k < ChunkKindCount; k++)
	{
		ChunkKind const &kind = ChunkKinds[k];
		std::vector<uint8_t> bytes = Valid(kind);
		size_t scalars = 0;
		size_t triples = 0;
		size_t objects = 0;
		size_t wrong = 0;

		for (size_t f = 0; f < kind.fieldCount; f++)
		{
			Field const &field = kind.fields[f];
			char const *why = 0;

			if (field.storage == Storage::IntTriple)
			{
				int32_t const sent[3] = { 100, -200, 300 };
				int32_t got[3] = { 0, 0, 0 };
				bool ok = true;

				for (int axis = 0; axis < 3; axis++)
				{
					ok = ok && SetChunkFieldAxis(kind, field, bytes.data(), bytes.size(), axis, sent[axis], &why);
				}

				for (int axis = 0; axis < 3; axis++)
				{
					ok = ok && ReadChunkFieldAxis(kind, field, bytes.data(), bytes.size(), axis, &got[axis], &why);
				}

				triples++;
				wrong += (ok && got[0] == 100 && got[1] == -200 && got[2] == 300) ? 0 : 1;
			}
			else if (field.storage == Storage::ObjectId)
			{
				// The setter puts the two halves in, as the file stores them, and the reader gives them back
				uint32_t first = 0;
				uint32_t second = 0;

				objects++;
				wrong += (SetChunkFieldObject(kind, field, bytes.data(), bytes.size(), 0x01020304u, 0xA0B0C0D0u) &&
				          ReadChunkFieldObject(kind, field, bytes.data(), bytes.size(), &first, &second) && first == 0x01020304u &&
				          second == 0xA0B0C0D0u) ? 0 : 1;
			}
			else if (IsAScalar(field))
			{
				scalars++;

				if (field.unit == Unit::ListCount)
				{
					double count = -1;
					wrong += (ReadChunkField(kind, field, bytes.data(), bytes.size(), &count, &why) && count == 0) ? 0 : 1;
					continue;
				}

				double const candidates[] = { field.hasCreation ? field.creation : 0, 0, 1, 2, 5, field.lowest, field.highest };
				bool sent = false;
				double value = 0;

				for (double const candidate : candidates)
				{
					if (HeldExactly(field, candidate) && SetChunkField(kind, field, bytes.data(), bytes.size(), candidate, &why))
					{
						value = candidate;
						sent = true;
						break;
					}
				}

				double got = -12345;
				wrong += (sent && ReadChunkField(kind, field, bytes.data(), bytes.size(), &got, &why) && got == value) ? 0 : 1;
			}
		}

		Check(wrong == 0 && (kind.fieldCount == 0 || scalars + triples + objects > 0),
		      std::string(kind.id) + ": its " + std::to_string(scalars) + " one-number fields, " + std::to_string(triples) +
		      " fields of three numbers and " + std::to_string(objects) + " object fields all read back what was put in");
	}
}

static void Hostile()
{
	printf("What the readers refuse:\n");

	ChunkKind const *const light = FindChunkKind("PLOBJLIT");
	ChunkKind const *const sound = FindChunkKind("SOUNDOB2");
	ChunkKind const *const particle = FindChunkKind("PARGENDA");
	ChunkKind const *const start = FindChunkKind("AVPSTART");

	Check(light && sound && particle && start, "the kinds this part reads are in the table");

	if (!light || !sound || !particle || !start)
	{
		return;
	}

	Field const *const fadeUp = FindChunkField(*light, "fade_up_time");
	Field const *const soundVolume = FindChunkField(*sound, "max_volume");
	Field const *const soundPosition = FindChunkField(*sound, "position");
	Field const *const orientationX = FindChunkField(*particle, "orientation_x");
	Field const *const parentId = FindChunkField(*particle, "parent_id");
	Field const *const startOrientation = FindChunkField(*start, "orientation");

	Check(fadeUp && soundVolume && soundPosition && orientationX && parentId && startOrientation, "and so are the fields");

	if (!fadeUp || !soundVolume || !soundPosition || !orientationX || !parentId || !startOrientation)
	{
		return;
	}

	std::vector<uint8_t> lightBytes = Valid(*light);
	PutInt(lightBytes, fadeUp->offset, 1234);
	std::vector<uint8_t> const lightBefore = lightBytes;

	double value = 77;
	char const *why = 0;

	Check(ReadChunkField(*light, *fadeUp, lightBytes.data(), lightBytes.size(), &value, &why) && value == 1234, "a field of its own kind reads");
	Check(!ReadChunkField(*sound, *fadeUp, lightBytes.data(), lightBytes.size(), &value, &why) && value == 1234,
	      "a field of another kind is refused, and the value is as it was");
	Check(!ReadChunkField(*light, *soundVolume, lightBytes.data(), lightBytes.size(), &value, &why) && value == 1234,
	      "the same the other way about (the sound's volume is no light's field, though it has a place in the light's payload)");

	// A payload too short for the field's place
	value = 77;
	Check(!ReadChunkField(*light, *fadeUp, lightBytes.data(), fadeUp->offset + 3, &value, &why) && value == 77,
	      "a payload that ends inside the field is refused, and the value is left alone");
	Check(ReadChunkField(*light, *fadeUp, lightBytes.data(), fadeUp->offset + 4, &value, &why) && value == 1234,
	      "one that ends exactly at the field's end is read");
	Check(!ReadChunkField(*light, *fadeUp, lightBytes.data(), 0, &value, &why), "an empty payload is refused");

	// Nothing handed over
	// Each check that would read or write through a null pointer if its guard were gone runs under Survives (crash_guard.h), so a lost
	// guard makes ITS check FAILED, by name, instead of stopping the program
	bool refused = false;
	bool ran = Survives([&] { refused = !ReadChunkField(*light, *fadeUp, 0, lightBytes.size(), &value, &why); });
	Check(ran && refused, "no payload, with a length said, is refused");
	refused = false;
	ran = Survives([&] { refused = !ReadChunkField(*light, *fadeUp, lightBytes.data(), lightBytes.size(), 0, &why); });
	Check(ran && refused, "no place for the value is refused");
	Check(ReadChunkField(*light, *fadeUp, lightBytes.data(), lightBytes.size(), &value, 0) && value == 1234, "a read with no place for a reason works");
	Check(lightBytes == lightBefore, "no read changed the payload");

	// The three kinds of field one number can't hold
	std::vector<uint8_t> soundBytes = Valid(*sound);
	std::vector<uint8_t> particleBytes = Valid(*particle);
	std::vector<uint8_t> startBytes = Valid(*start);
	value = 77;
	Check(!ReadChunkField(*sound, *soundPosition, soundBytes.data(), soundBytes.size(), &value, &why) && value == 77,
	      "a field of three numbers isn't read as one number");
	Check(!ReadChunkField(*particle, *parentId, particleBytes.data(), particleBytes.size(), &value, &why) && value == 77,
	      "an object field isn't read as one number");
	Check(!ReadChunkField(*start, *startOrientation, startBytes.data(), startBytes.size(), &value, &why) && value == 77,
	      "an orientation isn't read as one number");

	// The axis and object readers
	int32_t axis = 55;
	Check(!ReadChunkFieldAxis(*sound, *soundPosition, soundBytes.data(), soundBytes.size(), 3, &axis, &why) && axis == 55,
	      "a fourth axis is refused");
	Check(!ReadChunkFieldAxis(*sound, *soundPosition, soundBytes.data(), soundBytes.size(), -1, &axis, &why) && axis == 55,
	      "so is an axis below the first");
	Check(!ReadChunkFieldAxis(*sound, *soundVolume, soundBytes.data(), soundBytes.size(), 0, &axis, &why) && axis == 55,
	      "a one-number field has no axes");
	Check(!ReadChunkFieldAxis(*sound, *soundPosition, soundBytes.data(), 10, 2, &axis, &why) && axis == 55,
	      "a payload that ends inside the three numbers is refused");
	Check(!ReadChunkFieldAxis(*light, *soundPosition, lightBytes.data(), lightBytes.size(), 0, &axis, &why) && axis == 55,
	      "a three-number field of another kind is refused");

	uint32_t first = 7;
	uint32_t second = 9;
	Check(!ReadChunkFieldObject(*particle, *fadeUp, particleBytes.data(), particleBytes.size(), &first, &second) && first == 7 && second == 9,
	      "an object read of a field that isn't one of the kind's is refused");
	Check(!ReadChunkFieldObject(*sound, *soundVolume, soundBytes.data(), soundBytes.size(), &first, &second) && first == 7 && second == 9,
	      "an object read of a number is refused");
	Check(!ReadChunkFieldObject(*particle, *parentId, particleBytes.data(), parentId->offset + 7, &first, &second) && first == 7,
	      "a payload that ends inside the object's 8 bytes is refused");
	refused = false;
	ran = Survives([&] { refused = !ReadChunkFieldObject(*particle, *parentId, 0, particleBytes.size(), &first, &second); });
	Check(ran && refused && first == 7 && second == 9, "an object read with no payload, with a length said, is refused, and leaves the halves alone");
	refused = false;
	ran = Survives([&] { refused = !ReadChunkFieldObject(*particle, *parentId, particleBytes.data(), particleBytes.size(), 0, &second); });
	Check(ran && refused && second == 9, "an object read with no place for the first half is refused, and leaves the second alone");
	refused = false;
	ran = Survives([&] { refused = !ReadChunkFieldObject(*particle, *parentId, particleBytes.data(), particleBytes.size(), &first, 0); });
	Check(ran && refused && first == 7, "and with no place for the second half, leaves the first alone");
	// An object field of ANOTHER kind: the player's start has one too, 48 bytes in, and the particle's own is at 32, inside the start's payload
	Check(!ReadChunkFieldObject(*start, *parentId, startBytes.data(), startBytes.size(), &first, &second) && first == 7 && second == 9,
	      "an object field of another kind is refused, though it would fit in this payload");

	// The object setter: the same refusals, and it changes nothing when it refuses
	Check(SetChunkFieldObject(*particle, *parentId, particleBytes.data(), particleBytes.size(), 7, 9) &&
	      ReadChunkFieldObject(*particle, *parentId, particleBytes.data(), particleBytes.size(), &first, &second) && first == 7 && second == 9,
	      "an object field is set to its two halves, and reads back as them");
	std::vector<uint8_t> const particleBefore = particleBytes;
	Check(!SetChunkFieldObject(*start, *parentId, startBytes.data(), startBytes.size(), 1, 2) && !SetChunkFieldObject(*particle, *fadeUp, particleBytes.data(), particleBytes.size(), 1, 2) &&
	      !SetChunkFieldObject(*particle, *parentId, particleBytes.data(), parentId->offset + 7, 1, 2) && particleBytes == particleBefore,
	      "an object field of another kind, a field that isn't an object field, and a payload that ends inside the 8 bytes are refused, and the payload is as it was");
	bool setRefused = false;
	bool setRan = Survives([&] { setRefused = !SetChunkFieldObject(*particle, *parentId, 0, particleBytes.size(), 1, 2); });
	Check(setRan && setRefused, "no payload, with a length said, is refused by the setter too, and nothing is written through it");

	// A value that isn't a real number comes back as it is
	std::vector<uint8_t> nanBytes = Valid(*particle);
	PutInt(nanBytes, orientationX->offset, 0x7FC00001);
	value = 0;
	Check(ReadChunkField(*particle, *orientationX, nanBytes.data(), nanBytes.size(), &value, &why) && !IsRealNumber(value),
	      "a float that isn't a number is read as it is (a not-a-number)");
	PutInt(nanBytes, orientationX->offset, 0x7F800000);
	Check(ReadChunkField(*particle, *orientationX, nanBytes.data(), nanBytes.size(), &value, &why) && !IsRealNumber(value),
	      "and so is an endless one");
}

// A masked field is given with its mask applied and not shifted, as the setter takes it. The base tables have none, so this makes a kind.
static void MaskedField()
{
	printf("A masked field:\n");

	static Field const masked = { "word", "A word", Anchor::Start, 4, Storage::UInt32, Unit::Plain, 0x0FF00000u, Limit::None, 0, 0, "", false, 0, "this test" };
	static Field const fields[] = { masked };
	static ChunkKind const kind = { "TESTMASK", "test", "", 8, 8, -1, 0, fields, 1, "this test", 0, 0, -1 };

	std::vector<uint8_t> bytes(8, 0);
	PutInt(bytes, 4, (int32_t)0xF1234567u);
	double value = 0;
	char const *why = 0;

	Check(ReadChunkField(kind, kind.fields[0], bytes.data(), bytes.size(), &value, &why) && value == (double)(0xF1234567u & 0x0FF00000u),
	      "a masked word is given with only its masked bits, where they are (not shifted)");
}

int main()
{
	FindAny();
	EveryKindReadsBack();
	Hostile();
	MaskedField();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "all checks passed");
	return checksFailed ? 1 : 0;
}
