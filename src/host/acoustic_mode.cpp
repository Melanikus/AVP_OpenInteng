// acoustic_mode.cpp: Object Placement's Acoustic Mode, the data side (acoustic_mode.h says what inteng does). Written 2026-11-17 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "acoustic_mode.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "level_file.h"

namespace host
{
	namespace
	{
		typedef std::vector<char> Image;

		size_t const kHeadingBytes = 12;         // a chunk: its 8-letter name, then its size (which counts these 12 bytes too)
		size_t const kRecordBytes = 12;          // MODACOUS / ENVACOUS after the heading: the type, the reverb, the spare word
		size_t const kNameAt = 60;               // in an object header's payload: the name, after 60 bytes of numbers
		size_t const kMostNameBytes = 63;
		uint32_t const kDefaultReverbBits = 0xbf800000u;   // -1.0, the module record's "default" (0x4f58c8, 0x4f640b)

		// FUN_004f69e0's names (0x5e4e90, 20 bytes each)
		char const *const kTypeNames[kAcousticTypeCount] =
		{
			"Generic", "Padded Cell", "Room", "Bathroom", "Living Room", "Stone Room", "Auditorium", "Concert Hall", "Cave", "Arena", "Hangar",
			"Carpeted Hallway", "Hallway", "Stone Corridor", "Alley", "Forest", "City", "Mountains", "Quarry", "Plain", "Parking Lot", "Sewer Pipe",
			"Underwater", "Drugged", "Dizzy", "Psychotic",
		};

		struct Span
		{
			size_t at;
			size_t size;
		};

		int32_t IntAt(Image const &bytes, size_t at)
		{
			int32_t value;

			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		void PutIntAt(Image *bytes, size_t at, int32_t value)
		{
			memcpy(bytes->data() + at, &value, sizeof(value));
		}

		bool ChunkSizeAt(Image const &bytes, size_t at, size_t end, size_t *size)
		{
			if (end > bytes.size() || at > end || end - at < kHeadingBytes)
			{
				return false;
			}

			int32_t const stored = IntAt(bytes, at + 8);

			if (stored < (int32_t)kHeadingBytes || (size_t)stored > end - at)
			{
				return false;
			}

			*size = (size_t)stored;
			return true;
		}

		bool NameIs(Image const &bytes, size_t at, char const *name)
		{
			return memcmp(bytes.data() + at, name, 8) == 0;
		}

		bool Children(Image const &bytes, size_t from, size_t to, std::vector<Span> *out)
		{
			out->clear();

			for (size_t at = from; at < to; )
			{
				size_t size = 0;

				if (!ChunkSizeAt(bytes, at, to, &size))
				{
					return false;
				}

				Span const span = { at, size };

				out->push_back(span);
				at += size;
			}

			return true;
		}

		// The name an RBOBJECT chunk's OBJHEAD1 holds, or false
		bool NameOfObjectChunk(Image const &bytes, Span const &object, std::string *name)
		{
			std::vector<Span> inside;

			if (!Children(bytes, object.at + kHeadingBytes, object.at + object.size, &inside))
			{
				return false;
			}

			for (Span const &child : inside)
			{
				if (NameIs(bytes, child.at, "OBJHEAD1"))
				{
					size_t const length = child.size - kHeadingBytes;

					if (length < kNameAt + 1)
					{
						return false;
					}

					char const *const first = bytes.data() + child.at + kHeadingBytes + kNameAt;
					char const *const zero = (char const *)memchr(first, 0, length - kNameAt);

					if (!zero || (size_t)(zero - first) > kMostNameBytes)
					{
						return false;
					}

					name->assign(first, (size_t)(zero - first));
					return true;
				}
			}

			return false;
		}

		// A chunk that holds chunks (the library's own class, not one it kept as unknown bytes)
		Chunk_With_Children *Holder(Chunk *chunk)
		{
			Chunk_With_Children *const holder = dynamic_cast<Chunk_With_Children *>(chunk);

			return holder && !chunk->r_u_miscellaneous() ? holder : nullptr;
		}

		// The level's objects in the library's order (the file's), and the place of the first with this ID; -1 for none
		int32_t PlaceOfObject(File_Chunk *level, uint8_t const *id, Object_Chunk **object)
		{
			List<Object_Chunk *> found;
			int32_t place = 0;

			level->list_objects(&found);

			for (LIF<Object_Chunk *> each(&found); !each.done(); each.next(), place++)
			{
				if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
				{
					*object = each();
					return place;
				}
			}

			return -1;
		}

		// A record's 12 bytes as the level holds them now, or false (a record that is not 12 bytes is not read)
		bool ReadRecord(OpenLevelFile *file, Chunk *chunk, AcousticRecord *out)
		{
			std::vector<unsigned char> payload;

			if (!chunk || !file->PayloadOf(chunk, &payload) || payload.size() != kRecordBytes)
			{
				return false;
			}

			memcpy(&out->type, payload.data(), 4);
			memcpy(&out->reverb, payload.data() + 4, 4);
			memcpy(&out->spare, payload.data() + 8, 4);
			return true;
		}

		std::vector<unsigned char> RecordBytes(AcousticRecord const &record)
		{
			std::vector<unsigned char> bytes(kRecordBytes, 0);

			memcpy(bytes.data(), &record.type, 4);
			memcpy(bytes.data() + 4, &record.reverb, 4);
			memcpy(bytes.data() + 8, &record.spare, 4);
			return bytes;
		}

		void PutField(AcousticRecord *record, AcousticField field, int32_t type, float reverb)
		{
			if (field == AcousticField::Type)
			{
				record->type = type;
			}
			else
			{
				record->reverb = reverb;
			}
		}

		// A whole record chunk (heading and 12 bytes) for a new image
		void AppendRecordChunk(Image *out, char const *name, AcousticRecord const &record)
		{
			size_t const at = out->size();
			std::vector<unsigned char> const bytes = RecordBytes(record);

			out->insert(out->end(), name, name + 8);
			out->resize(at + kHeadingBytes);
			PutIntAt(out, at + 8, (int32_t)(kHeadingBytes + kRecordBytes));
			out->insert(out->end(), bytes.begin(), bytes.end());
		}

		// The module's MODULEDT and MODACOUS as the library holds them now (either may be null); false when MODULEDT is not a chunk holding chunks
		bool ModuleChunks(Object_Chunk *object, Chunk_With_Children **moduleData, Chunk **record)
		{
			*moduleData = nullptr;
			*record = nullptr;

			Chunk *const data = object->lookup_single_child("MODULEDT");

			if (!data)
			{
				return true;
			}

			*moduleData = Holder(data);

			if (!*moduleData)
			{
				return false;
			}

			*record = (*moduleData)->lookup_single_child("MODACOUS");
			return true;
		}

		AcousticResult FromLoader(LevelOpenResult result)
		{
			return result == LevelOpenResult::Opened ? AcousticResult::Done : AcousticResult::LoaderRefused;
		}

		// A module to give a new record: its object's place among the level's objects, and its name (checked against the image's OBJHEAD1)
		struct NewRecord
		{
			int32_t place;
			std::string name;
			bool hasModuleData;
			AcousticRecord record;
		};

		// One new image of the level with every listed module's new MODACOUS (and MODULEDT, when it has none) at the end of its chunks, then the
		// game's loader asked to take it
		AcousticResult PutNewModuleRecords(OpenLevelFile *file, std::vector<NewRecord> const &records)
		{
			Image image;

			if (!file->BuildLevel(image) || image.size() < kHeadingBytes || !NameIs(image, 0, "REBINFF2"))
			{
				return AcousticResult::CantBuild;
			}

			std::vector<Span> top;

			if (!Children(image, kHeadingBytes, image.size(), &top))
			{
				return AcousticResult::CantBuild;
			}

			Image out(image.begin(), image.begin() + (std::ptrdiff_t)kHeadingBytes);
			int32_t place = 0;
			size_t done = 0;

			for (Span const &chunk : top)
			{
				bool const isObject = NameIs(image, chunk.at, "RBOBJECT");
				const std::vector<NewRecord>::const_iterator wanted = !isObject ? records.end() :
					std::find_if(records.begin(), records.end(), [&](NewRecord const &r) { return r.place == place; });

				if (isObject)
				{
					place++;
				}

				if (wanted == records.end())
				{
					out.insert(out.end(), image.begin() + (std::ptrdiff_t)chunk.at, image.begin() + (std::ptrdiff_t)(chunk.at + chunk.size));
					continue;
				}

				std::string name;
				std::vector<Span> inside;

				if (!NameOfObjectChunk(image, chunk, &name) || name != wanted->name || !Children(image, chunk.at + kHeadingBytes, chunk.at + chunk.size, &inside))
				{
					return AcousticResult::CantBuild;   // the image's objects are not the library's, in its order: nothing is changed
				}

				size_t const objectStart = out.size();
				bool moduleDataSeen = false;

				out.insert(out.end(), image.begin() + (std::ptrdiff_t)chunk.at, image.begin() + (std::ptrdiff_t)(chunk.at + kHeadingBytes));

				for (Span const &child : inside)
				{
					// FUN_00431a00 takes the FIRST MODULEDT: the record goes at the end of that one
					if (moduleDataSeen || !NameIs(image, child.at, "MODULEDT"))
					{
						out.insert(out.end(), image.begin() + (std::ptrdiff_t)child.at, image.begin() + (std::ptrdiff_t)(child.at + child.size));
						continue;
					}

					moduleDataSeen = true;

					size_t const dataStart = out.size();

					out.insert(out.end(), image.begin() + (std::ptrdiff_t)child.at, image.begin() + (std::ptrdiff_t)(child.at + child.size));
					AppendRecordChunk(&out, "MODACOUS", wanted->record);
					PutIntAt(&out, dataStart + 8, (int32_t)(out.size() - dataStart));
				}

				if (!moduleDataSeen)
				{
					if (wanted->hasModuleData)
					{
						return AcousticResult::CantBuild;
					}

					// FUN_004f6670 0x4f66c2-0x4f66f0: a new MODULEDT at the end of the object's chunks, the record inside it
					size_t const dataStart = out.size();
					static char const kModuleData[9] = "MODULEDT";   // one array: its start and its end from the same object

					out.insert(out.end(), kModuleData, kModuleData + 8);
					out.resize(dataStart + kHeadingBytes);
					AppendRecordChunk(&out, "MODACOUS", wanted->record);
					PutIntAt(&out, dataStart + 8, (int32_t)(out.size() - dataStart));
				}

				PutIntAt(&out, objectStart + 8, (int32_t)(out.size() - objectStart));
				done++;
			}

			if (done != records.size())
			{
				return AcousticResult::CantBuild;
			}

			PutIntAt(&out, 8, (int32_t)out.size());
			return FromLoader(file->ReplaceLevel(out));
		}

		// The level's REBENVDT and ENVACOUS (either may be null); false when REBENVDT is not a chunk holding chunks
		bool LevelChunks(File_Chunk *level, Chunk_With_Children **environment, Chunk **record)
		{
			*environment = nullptr;
			*record = nullptr;

			Chunk *const data = level->lookup_single_child("REBENVDT");

			if (!data)
			{
				return true;
			}

			*environment = Holder(data);

			if (!*environment)
			{
				return false;
			}

			*record = (*environment)->lookup_single_child("ENVACOUS");
			return true;
		}

		// The level's ENVACOUS made at the end of REBENVDT, holding record (FUN_004f6760(1): Environment_Acoustics_Chunk's 0, 2.0, 0, then the field)
		AcousticResult PutNewLevelRecord(OpenLevelFile *file, AcousticRecord const &record)
		{
			Image image;

			if (!file->BuildLevel(image) || image.size() < kHeadingBytes || !NameIs(image, 0, "REBINFF2"))
			{
				return AcousticResult::CantBuild;
			}

			std::vector<Span> top;

			if (!Children(image, kHeadingBytes, image.size(), &top))
			{
				return AcousticResult::CantBuild;
			}

			Image out(image.begin(), image.begin() + (std::ptrdiff_t)kHeadingBytes);
			bool put = false;

			for (Span const &chunk : top)
			{
				out.insert(out.end(), image.begin() + (std::ptrdiff_t)chunk.at, image.begin() + (std::ptrdiff_t)(chunk.at + chunk.size));

				// the FIRST REBENVDT, as lookup_single_child gives it
				if (!put && NameIs(image, chunk.at, "REBENVDT"))
				{
					size_t const start = out.size() - chunk.size;

					AppendRecordChunk(&out, "ENVACOUS", record);
					PutIntAt(&out, start + 8, (int32_t)(out.size() - start));
					put = true;
				}
			}

			if (!put)
			{
				return AcousticResult::CantBuild;
			}

			PutIntAt(&out, 8, (int32_t)out.size());
			return FromLoader(file->ReplaceLevel(out));
		}

		AcousticRecord NewModuleRecord()
		{
			AcousticRecord record;

			record.type = -1;      // Module_Acoustics_Chunk(Object_Module_Data_Chunk *): "negative means default environment acoustics"
			record.reverb = -1.0f;
			record.spare = 0;
			return record;
		}

		AcousticRecord NewLevelRecord()
		{
			AcousticRecord record;

			record.type = 0;       // Environment_Acoustics_Chunk(Environment_Data_Chunk *)
			record.reverb = 2.0f;
			record.spare = 0;
			return record;
		}
	}

	char const *AcousticTypeName(int32_t type)
	{
		return type >= 0 && type < kAcousticTypeCount ? kTypeNames[type] : "Invalid type";
	}

	float AcousticReverbKept(float typed)
	{
		// as the code compares (0x4f64b7, 0x4f6609): not at or below 1.0 is 2.0 (a typed NaN too, which compares false), then at or below 0.0 is 0.0
		if (!(typed <= 1.0f))
		{
			return 2.0f;
		}

		return typed <= 0.0f ? 0.0f : typed;
	}

	char const *AcousticWords(AcousticResult result)
	{
		switch (result)
		{
		case AcousticResult::Done: return "done";
		case AcousticResult::BadCall: return "nothing to change";
		case AcousticResult::NoObject: return "no object has that ID";
		case AcousticResult::NoEnvironment: return "the level has no environment data (REBENVDT), so it has no acoustics of its own";
		case AcousticResult::BadRecord: return "an acoustics record in the level is not the size the game reads";
		case AcousticResult::CantBuild: return "the level couldn't be put together from its bytes";
		case AcousticResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	bool Acoustic_ModuleRecord(OpenLevelFile *file, uint8_t const *moduleId, AcousticRecord *out)
	{
		Object_Chunk *object = nullptr;
		Chunk_With_Children *moduleData = nullptr;
		Chunk *record = nullptr;

		return file && moduleId && out && file->Level() && PlaceOfObject(file->Level(), moduleId, &object) >= 0 && ModuleChunks(object, &moduleData, &record) &&
			ReadRecord(file, record, out);
	}

	bool Acoustic_LevelRecord(OpenLevelFile *file, AcousticRecord *out)
	{
		Chunk_With_Children *environment = nullptr;
		Chunk *record = nullptr;

		return file && out && file->Level() && LevelChunks(file->Level(), &environment, &record) && ReadRecord(file, record, out);
	}

	AcousticResult Acoustic_SetModules(OpenLevelFile *file, std::vector<std::array<uint8_t, 8>> const &modules, AcousticField field, int32_t type, float reverb,
		bool make, size_t *changed)
	{
		if (changed)
		{
			*changed = 0;
		}

		if (!file || !file->Level() || modules.empty())
		{
			return AcousticResult::BadCall;
		}

		// First every module is read, and nothing is written until all of them are known: a bad one changes nothing
		std::vector<Chunk *> inPlace;
		std::vector<AcousticRecord> inPlaceRecords;
		std::vector<NewRecord> made;

		for (size_t m = 0; m < modules.size(); m++)
		{
			if (std::find(modules.begin(), modules.begin() + (std::ptrdiff_t)m, modules[m]) != modules.begin() + (std::ptrdiff_t)m)
			{
				continue;   // named twice: written once
			}

			Object_Chunk *object = nullptr;
			int32_t const place = PlaceOfObject(file->Level(), modules[m].data(), &object);
			Chunk_With_Children *moduleData = nullptr;
			Chunk *record = nullptr;

			if (place < 0)
			{
				return AcousticResult::NoObject;
			}

			if (!ModuleChunks(object, &moduleData, &record))
			{
				return AcousticResult::BadRecord;
			}

			if (record)
			{
				AcousticRecord now;

				if (!ReadRecord(file, record, &now))
				{
					return AcousticResult::BadRecord;
				}

				PutField(&now, field, type, reverb);
				inPlace.push_back(record);
				inPlaceRecords.push_back(now);
			}
			else if (make)
			{
				NewRecord one;

				one.place = place;
				one.name.assign(object->object_data.o_name, strnlen(object->object_data.o_name, sizeof(object->object_data.o_name)));
				one.hasModuleData = moduleData != nullptr;
				one.record = NewModuleRecord();
				PutField(&one.record, field, type, reverb);
				made.push_back(one);
			}
		}

		// The records that are there: their bytes, in place (before any new image, which keeps these edits: BuildLevel builds from them)
		for (size_t i = 0; i < inPlace.size(); i++)
		{
			if (!file->SetPayload(inPlace[i], RecordBytes(inPlaceRecords[i])))
			{
				return AcousticResult::BadRecord;
			}
		}

		if (!made.empty())
		{
			AcousticResult const result = PutNewModuleRecords(file, made);

			if (result != AcousticResult::Done)
			{
				if (changed)
				{
					*changed = inPlace.size();
				}

				return result;
			}
		}

		if (changed)
		{
			*changed = inPlace.size() + made.size();
		}

		return AcousticResult::Done;
	}

	AcousticResult Acoustic_SetLevel(OpenLevelFile *file, AcousticField field, int32_t type, float reverb)
	{
		if (!file || !file->Level())
		{
			return AcousticResult::BadCall;
		}

		Chunk_With_Children *environment = nullptr;
		Chunk *record = nullptr;

		if (!LevelChunks(file->Level(), &environment, &record))
		{
			return AcousticResult::BadRecord;
		}

		if (!environment)
		{
			return AcousticResult::NoEnvironment;
		}

		if (!record)
		{
			AcousticRecord fresh = NewLevelRecord();

			PutField(&fresh, field, type, reverb);
			return PutNewLevelRecord(file, fresh);
		}

		AcousticRecord now;

		if (!ReadRecord(file, record, &now))
		{
			return AcousticResult::BadRecord;
		}

		PutField(&now, field, type, reverb);
		return file->SetPayload(record, RecordBytes(now)) ? AcousticResult::Done : AcousticResult::BadRecord;
	}

	AcousticResult Acoustic_MakeLevelRecord(OpenLevelFile *file)
	{
		if (!file || !file->Level())
		{
			return AcousticResult::BadCall;
		}

		Chunk_With_Children *environment = nullptr;
		Chunk *record = nullptr;

		if (!LevelChunks(file->Level(), &environment, &record))
		{
			return AcousticResult::BadRecord;
		}

		if (!environment)
		{
			return AcousticResult::NoEnvironment;
		}

		return record ? AcousticResult::Done : PutNewLevelRecord(file, NewLevelRecord());
	}

	float Acoustic_ModuleReverbStart(OpenLevelFile *file, std::vector<std::array<uint8_t, 8>> const &modules)
	{
		for (std::array<uint8_t, 8> const &module : modules)
		{
			AcousticRecord record;
			uint32_t bits = 0;

			if (Acoustic_ModuleRecord(file, module.data(), &record))
			{
				memcpy(&bits, &record.reverb, 4);

				if (bits != kDefaultReverbBits)
				{
					return record.reverb;
				}
			}
		}

		return 2.0f;
	}

	void Acoustic_FrameLines(OpenLevelFile *file, uint8_t const *moduleId, std::vector<AcousticLine> *lines)
	{
		if (!lines)
		{
			return;
		}

		lines->clear();

		char words[128];
		AcousticRecord level;

		if (!Acoustic_LevelRecord(file, &level))
		{
			level.type = 0;          // 0x4f5b7f: ESI = 0 when there is no record
			level.reverb = 0.0f;
		}

		snprintf(words, sizeof(words), "Default Type : %s", AcousticTypeName(level.type));
		lines->push_back({ 16, 32, words });
		snprintf(words, sizeof(words), "Default Reverb : %.2f", (double)level.reverb);
		lines->push_back({ 16, 48, words });

		Object_Chunk *object = nullptr;

		if (!file || !file->Level() || !moduleId || PlaceOfObject(file->Level(), moduleId, &object) < 0)
		{
			return;
		}

		lines->push_back({ 16, 64, std::string(object->object_data.o_name, strnlen(object->object_data.o_name, sizeof(object->object_data.o_name))) });

		AcousticRecord record;
		bool const held = Acoustic_ModuleRecord(file, moduleId, &record);

		if (held && record.type >= 0)
		{
			snprintf(words, sizeof(words), "Type : %s", AcousticTypeName(record.type));
			lines->push_back({ 16, 80, words });
		}
		else
		{
			lines->push_back({ 16, 80, "Type : Default" });
		}

		// "Reverb : Default" for no record or a reverb below 0.0 (FCOMP, 0x4f5c87); a NaN compares as not below, as on the x87
		if (held && !(record.reverb < 0.0f))
		{
			snprintf(words, sizeof(words), "Reverb : %.2f", (double)record.reverb);
			lines->push_back({ 16, 96, words });
		}
		else
		{
			lines->push_back({ 16, 96, "Reverb : Default" });
		}
	}
}
