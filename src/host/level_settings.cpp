// level_settings.cpp: the level's own settings, editable: the data side (level_settings.h says what and why). Written 2026-11-27 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "level_settings.h"

#include <string.h>

#include "field_model.h"
#include "level_file.h"
#include "level_image.h"

namespace host
{
	namespace
	{
		size_t const kEnvironmentWholeBytes = 24;   // the AVPENVIR every known RIF holds: 24 bytes that say 24

		char const ShortEnvironmentWhy[] = "this level's AVPENVIR is shorter than 24 bytes: the game reads only a whole one (24 bytes that say 24, as every "
		                                   "known level holds), so it isn't edited";
		char const NoTableWhy[] = "the editor's model has no table for this chunk";
		char const NoPayloadWhy[] = "the chunk holds no bytes at all";

		void Say(char const **why, char const *words)
		{
			if (why)
			{
				*why = words;
			}
		}

		model::ChunkKind const *KindOf(LevelSettingsKind kind)
		{
			return model::FindChunkKind(LevelSettings_ChunkName(kind));
		}

		model::Field const *FieldOf(LevelSettingsKind kind, char const *field)
		{
			model::ChunkKind const *const found = KindOf(kind);

			return found && field ? model::FindChunkField(*found, field) : 0;
		}

		void PutInt(std::vector<uint8_t> *bytes, size_t at, int32_t value)
		{
			memcpy(bytes->data() + at, &value, sizeof(value));
		}

		// The level as the next save writes it, its top chunks, which one is its REBENVDT (top.size() when it has none) and that one's children.
		// False when the bytes can't be walked.
		bool LevelAndEnvironment(OpenLevelFile *file, image::Bytes *bytes, std::vector<image::Span> *top, size_t *envAt, std::vector<image::Span> *inside)
		{
			inside->clear();

			if (!image::Walk(file, bytes, top))
			{
				return false;
			}

			*envAt = image::FirstNamed(*bytes, *top, "REBENVDT");

			if (*envAt == top->size())
			{
				return true;
			}

			image::Span const env = (*top)[*envAt];

			return image::Children(*bytes, env.at + image::kHeadingBytes, env.at + env.size, inside);
		}
	}

	char const *LevelSettingsWords(LevelSettingsResult result)
	{
		switch (result)
		{
		case LevelSettingsResult::Done:
			return "written";
		case LevelSettingsResult::BadCall:
			return "nothing to write it to, or settings of the wrong size";
		case LevelSettingsResult::NoEnvironment:
			return "the level has no environment data (REBENVDT) to hold it";
		case LevelSettingsResult::Refused:
			return "the level's own copy can't be read as the game reads it, so it isn't edited";
		case LevelSettingsResult::CantBuild:
			return "the level couldn't be put together from its bytes";
		case LevelSettingsResult::LoaderRefused:
			return "the game's loader wouldn't take the changed level, so nothing was changed";
		}

		return "an answer this editor doesn't know";
	}

	char const *LevelSettings_ChunkName(LevelSettingsKind kind)
	{
		return kind == LevelSettingsKind::Environment ? "AVPENVIR" : "GLOGENDC";
	}

	std::vector<uint8_t> LevelSettings_Defaults(LevelSettingsKind kind)
	{
		if (kind == LevelSettingsKind::Environment)
		{
			// avp_chunk.cpp:608-627: its size; the sky 200, 200, 200; pistol, plasma caster, disc and medicomp (bits 0, 1, 2, 4: 0x17); 30 spears
			std::vector<uint8_t> settings(kEnvironmentWholeBytes, 0);

			PutInt(&settings, 0, (int32_t)kEnvironmentWholeBytes);
			PutInt(&settings, 4, 200);
			PutInt(&settings, 8, 200);
			PutInt(&settings, 12, 200);
			PutInt(&settings, 16, 0x17);
			PutInt(&settings, 20, 30);
			return settings;
		}

		// the chunk's own constructor (avp_chunk.cpp, Global_Generator_Data_Chunk without data), and aliens when there is none (npcsetup.cpp):
		// aliens, 25 at most, 4 a minute, 2 more a minute each minute, 60 s, the two spares 0
		std::vector<uint8_t> settings(28, 0);

		PutInt(&settings, 4, 25);
		PutInt(&settings, 8, 4);
		PutInt(&settings, 12, 2);
		PutInt(&settings, 16, 60);
		return settings;
	}

	bool LevelSettings_AsTheGameReads(LevelSettingsKind kind, std::vector<uint8_t> const &payload, std::vector<uint8_t> *out, char const **why)
	{
		Say(why, "");
		model::ChunkKind const *const found = KindOf(kind);

		if (!found || !out)
		{
			Say(why, NoTableWhy);
			return false;
		}

		if (payload.empty())
		{
			Say(why, NoPayloadWhy);
			return false;
		}

		if (kind == LevelSettingsKind::Environment && payload.size() < kEnvironmentWholeBytes)
		{
			Say(why, ShortEnvironmentWhy);
			return false;
		}

		model::SizeCheck const check = model::CheckChunkSize(*found, payload.data(), payload.size());

		if (check != model::SizeCheck::Right)
		{
			Say(why, model::ChunkSizeCheckWords(check));
			return false;
		}

		if (kind == LevelSettingsKind::Generators)
		{
			// the loader refuses any enemy but aliens or marines; a level that holds one never opens, but these bytes are checked as if it could
			model::Field const *const enemy = model::FindChunkField(*found, "EnemyGenerated");
			double value = 0;
			char const *reason = 0;

			if (!enemy || !model::ReadChunkField(*found, *enemy, payload.data(), payload.size(), &value, &reason) || !model::ValueIsAllowed(*enemy, value, &reason))
			{
				Say(why, reason ? reason : NoTableWhy);
				return false;
			}
		}

		*out = payload;
		return true;
	}

	LevelSettingsResult LevelSettings_Read(OpenLevelFile *file, LevelSettingsKind kind, std::vector<uint8_t> *settings, bool *had, char const **why)
	{
		Say(why, "");

		if (!file || !file->Level() || !settings || !had)
		{
			return LevelSettingsResult::BadCall;
		}

		*had = false;
		image::Bytes bytes;
		std::vector<image::Span> top, inside;
		size_t envAt = 0;

		if (!LevelAndEnvironment(file, &bytes, &top, &envAt, &inside))
		{
			return LevelSettingsResult::CantBuild;
		}

		if (envAt == top.size())
		{
			return LevelSettingsResult::NoEnvironment;
		}

		size_t const at = image::FirstNamed(bytes, inside, LevelSettings_ChunkName(kind));   // the first of its name: the one the game reads

		if (at == inside.size())
		{
			*settings = LevelSettings_Defaults(kind);
			return LevelSettingsResult::Done;
		}

		image::Span const chunk = inside[at];
		std::vector<uint8_t> const payload(bytes.begin() + (std::ptrdiff_t)(chunk.at + image::kHeadingBytes), bytes.begin() + (std::ptrdiff_t)(chunk.at + chunk.size));

		if (!LevelSettings_AsTheGameReads(kind, payload, settings, why))
		{
			return LevelSettingsResult::Refused;
		}

		*had = true;
		return LevelSettingsResult::Done;
	}

	bool LevelSettings_Get(LevelSettingsKind kind, std::vector<uint8_t> const &settings, char const *field, int32_t *value)
	{
		model::ChunkKind const *const found = KindOf(kind);
		model::Field const *const one = FieldOf(kind, field);
		double read = 0;
		char const *why = 0;

		if (!found || !one || !value || settings.empty() || !model::ReadChunkField(*found, *one, settings.data(), settings.size(), &read, &why))
		{
			return false;
		}

		*value = (int32_t)read;
		return true;
	}

	bool LevelSettings_Set(LevelSettingsKind kind, std::vector<uint8_t> *settings, char const *field, int32_t value, char const **why)
	{
		Say(why, "");
		model::ChunkKind const *const found = KindOf(kind);
		model::Field const *const one = FieldOf(kind, field);

		if (!found || !one || !settings || settings->empty())
		{
			Say(why, NoTableWhy);
			return false;
		}

		// the model checks the value against the field's range first and changes nothing when it isn't allowed; for the kit (mask 0x7f) it
		// sets only the seven named bits and keeps the rest
		char const *reason = "";
		bool const set = model::SetChunkField(*found, *one, settings->data(), settings->size(), (double)value, &reason);

		Say(why, reason ? reason : "");
		return set;
	}

	bool LevelSettings_Range(LevelSettingsKind kind, char const *field, int32_t *lowest, int32_t *highest)
	{
		model::Field const *const one = FieldOf(kind, field);

		if (!one || !lowest || !highest || one->limit == model::Limit::None)
		{
			return false;
		}

		*lowest = (int32_t)one->lowest;
		*highest = (int32_t)one->highest;
		return true;
	}

	LevelSettingsResult LevelSettings_Write(OpenLevelFile *file, LevelSettingsKind kind, std::vector<uint8_t> const &settings, char const **why)
	{
		Say(why, "");

		if (!file || !file->Level())
		{
			return LevelSettingsResult::BadCall;
		}

		std::vector<uint8_t> checked;

		if (!LevelSettings_AsTheGameReads(kind, settings, &checked, why))
		{
			return LevelSettingsResult::BadCall;
		}

		image::Bytes bytes;
		std::vector<image::Span> top, inside;
		size_t envAt = 0;

		if (!LevelAndEnvironment(file, &bytes, &top, &envAt, &inside))
		{
			return LevelSettingsResult::CantBuild;
		}

		if (envAt == top.size())
		{
			return LevelSettingsResult::NoEnvironment;
		}

		// The new REBENVDT: its children as they are, this chunk in place of the first of its name (or last, when it has none)
		char const *const name = LevelSettings_ChunkName(kind);
		size_t const at = image::FirstNamed(bytes, inside, name);
		image::Bytes const made = image::MakeChunk(name, image::Bytes(checked.begin(), checked.end()));
		image::Span const env = top[envAt];
		image::Bytes newEnv(bytes.begin() + (std::ptrdiff_t)env.at, bytes.begin() + (std::ptrdiff_t)(env.at + image::kHeadingBytes));

		for (size_t i = 0; i < inside.size(); i++)
		{
			if (i == at)
			{
				newEnv.insert(newEnv.end(), made.begin(), made.end());
			}
			else
			{
				image::AddCopy(&newEnv, bytes, inside[i]);
			}
		}

		if (at == inside.size())
		{
			newEnv.insert(newEnv.end(), made.begin(), made.end());
		}

		if (newEnv.size() > image::kLargest)
		{
			return LevelSettingsResult::CantBuild;
		}

		image::PutIntAt(&newEnv, 8, (int32_t)newEnv.size());

		image::Bytes out(bytes.begin(), bytes.begin() + (std::ptrdiff_t)env.at);

		out.insert(out.end(), newEnv.begin(), newEnv.end());
		out.insert(out.end(), bytes.begin() + (std::ptrdiff_t)(env.at + env.size), bytes.end());

		if (out.size() > image::kLargest)
		{
			return LevelSettingsResult::CantBuild;
		}

		image::PutIntAt(&out, 8, (int32_t)out.size());

		LevelOpenResult const result = file->ReplaceLevel(out);

		if (result == LevelOpenResult::Opened)
		{
			return LevelSettingsResult::Done;
		}

		return result == LevelOpenResult::CantRead ? LevelSettingsResult::CantBuild : LevelSettingsResult::LoaderRefused;
	}
}
