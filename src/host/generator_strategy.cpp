// See generator_strategy.h. Written 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "generator_strategy.h"

#include <string.h>

#include <string>
#include <vector>

#include "field_model.h"       // model::MakeNewStrategy
#include "level_file.h"
#include "level_image.h"
#include "strategy_lookup.h"   // FindGenerator

namespace host
{
	using namespace image;

	namespace
	{
		size_t const kMostStrategyBytes = 65536;
		size_t const kMostGeneratorName = 255;

		// The name an AVPGENEX's AVPGENNM holds (the first one), read inside the chunk's bounds; false when it has none or the name has no end
		bool ExtraNameOf(Bytes const &image, Span const &extra, std::string *name)
		{
			std::vector<Span> inside;

			if (!Children(image, extra.at + kHeadingBytes, extra.at + extra.size, &inside))
			{
				return false;
			}

			size_t const at = FirstNamed(image, inside, "AVPGENNM");

			if (at == inside.size())
			{
				return false;
			}

			char const *const first = image.data() + inside[at].at + kHeadingBytes;
			char const *const end = image.data() + inside[at].at + inside[at].size;
			char const *const zero = (char const *)memchr(first, 0, (size_t)(end - first));

			if (!zero)
			{
				return false;
			}

			name->assign(first, (size_t)(zero - first));
			return true;
		}

		// The AVPGENNM chunk for a name: the name, its zero, padded with zeros to a multiple of four (AVP_Generator_Extra_Name_Chunk::size_chunk: (length + 4) & ~3)
		Bytes NameChunk(std::string const &name)
		{
			Bytes payload(name.begin(), name.end());

			payload.resize((name.size() + 4) & ~(size_t)3, 0);
			return MakeChunk("AVPGENNM", payload);
		}

		// Puts `strategy` (a whole AVPSTRAT chunk; empty: none) as the strategy of the enemy or generator with this ID
		GeneratorStrategyResult Change(OpenLevelFile *file, uint8_t const *id, Bytes const &strategy)
		{
			if (!file || !file->Level() || !id)
			{
				return GeneratorStrategyResult::BadCall;
			}

			GeneratorRef generator;

			if (!FindGenerator(file->Level(), id, &generator))
			{
				return GeneratorStrategyResult::NotFound;
			}

			if (generator.name.empty() || generator.name.size() > kMostGeneratorName)
			{
				return GeneratorStrategyResult::TooBig;
			}

			Bytes image;
			std::vector<Span> top, inside, special;

			if (!Walk(file, &image, &top))
			{
				return GeneratorStrategyResult::CantBuild;
			}

			size_t envAt = top.size();

			for (size_t i = 0; i < top.size() && envAt == top.size(); i++)
			{
				envAt = NameIs(image, top[i].at, "REBENVDT") ? i : envAt;
			}

			if (envAt == top.size() || !Children(image, top[envAt].at + kHeadingBytes, top[envAt].at + top[envAt].size, &inside))
			{
				return GeneratorStrategyResult::CantBuild;
			}

			size_t const specAt = FirstNamed(image, inside, "SPECLOBJ");

			if (specAt == inside.size() || !Children(image, inside[specAt].at + kHeadingBytes, inside[specAt].at + inside[specAt].size, &special))
			{
				return GeneratorStrategyResult::CantBuild;
			}

			// the first AVPGENEX named as the generator, as the game's get_extra_data_chunk takes it
			size_t extraAt = special.size();

			for (size_t i = 0; i < special.size() && extraAt == special.size(); i++)
			{
				std::string name;

				if (NameIs(image, special[i].at, "AVPGENEX") && ExtraNameOf(image, special[i], &name) && name == generator.name)
				{
					extraAt = i;
				}
			}

			Bytes extra;

			if (extraAt != special.size())
			{
				std::vector<Span> parts;
				bool had = false;

				if (!Children(image, special[extraAt].at + kHeadingBytes, special[extraAt].at + special[extraAt].size, &parts))
				{
					return GeneratorStrategyResult::CantBuild;
				}

				extra.assign(image.begin() + (std::ptrdiff_t)special[extraAt].at, image.begin() + (std::ptrdiff_t)(special[extraAt].at + kHeadingBytes));

				for (Span const &part : parts)
				{
					if (NameIs(image, part.at, "AVPSTRAT"))
					{
						had = true;
					}
					else
					{
						AddCopy(&extra, image, part);
					}
				}

				if (!had && strategy.empty())
				{
					return GeneratorStrategyResult::NoStrategy;
				}
			}
			else
			{
				if (strategy.empty())
				{
					return GeneratorStrategyResult::NoStrategy;
				}

				extra = MakeChunk("AVPGENEX", NameChunk(generator.name));
			}

			extra.insert(extra.end(), strategy.begin(), strategy.end());
			PutIntAt(&extra, 8, (int32_t)extra.size());

			// SPECLOBJ again: the AVPGENEX in its place, or at the end when it is new
			Bytes spec(image.begin() + (std::ptrdiff_t)inside[specAt].at, image.begin() + (std::ptrdiff_t)(inside[specAt].at + kHeadingBytes));

			for (size_t i = 0; i < special.size(); i++)
			{
				if (i == extraAt)
				{
					spec.insert(spec.end(), extra.begin(), extra.end());
				}
				else
				{
					AddCopy(&spec, image, special[i]);
				}
			}

			if (extraAt == special.size())
			{
				spec.insert(spec.end(), extra.begin(), extra.end());
			}

			PutIntAt(&spec, 8, (int32_t)spec.size());

			Bytes env(image.begin() + (std::ptrdiff_t)top[envAt].at, image.begin() + (std::ptrdiff_t)(top[envAt].at + kHeadingBytes));

			for (size_t i = 0; i < inside.size(); i++)
			{
				if (i == specAt)
				{
					env.insert(env.end(), spec.begin(), spec.end());
				}
				else
				{
					AddCopy(&env, image, inside[i]);
				}
			}

			PutIntAt(&env, 8, (int32_t)env.size());

			Bytes out(image.begin(), image.begin() + (std::ptrdiff_t)top[envAt].at);

			out.insert(out.end(), env.begin(), env.end());
			out.insert(out.end(), image.begin() + (std::ptrdiff_t)(top[envAt].at + top[envAt].size), image.end());

			if (out.size() > kLargest)
			{
				return GeneratorStrategyResult::TooBig;
			}

			PutIntAt(&out, 8, (int32_t)out.size());

			LevelOpenResult const result = file->ReplaceLevel(out);

			if (result == LevelOpenResult::Opened)
			{
				return GeneratorStrategyResult::Done;
			}

			return result == LevelOpenResult::CantRead ? GeneratorStrategyResult::CantBuild : GeneratorStrategyResult::LoaderRefused;
		}
	}

	GeneratorStrategyResult GeneratorStrategy_Put(OpenLevelFile *file, uint8_t const *id, uint8_t const *payload, size_t length)
	{
		if (!payload || length < 12)
		{
			return GeneratorStrategyResult::BadCall;
		}

		if (length > kMostStrategyBytes)
		{
			return GeneratorStrategyResult::TooBig;
		}

		return Change(file, id, MakeChunk("AVPSTRAT", Bytes((char const *)payload, (char const *)payload + length)));
	}

	GeneratorStrategyResult GeneratorStrategy_Assign(OpenLevelFile *file, uint8_t const *id, int32_t kind)
	{
		uint8_t payload[1024];
		size_t length = 0;

		if (!model::MakeNewStrategy(kind, payload, sizeof(payload), &length))
		{
			return GeneratorStrategyResult::BadKind;
		}

		return GeneratorStrategy_Put(file, id, payload, length);
	}

	GeneratorStrategyResult GeneratorStrategy_Delete(OpenLevelFile *file, uint8_t const *id)
	{
		return Change(file, id, Bytes());
	}

	char const *GeneratorStrategyWords(GeneratorStrategyResult result)
	{
		switch (result)
		{
		case GeneratorStrategyResult::Done: return "done";
		case GeneratorStrategyResult::BadCall: return "no level, no object or no strategy to put";
		case GeneratorStrategyResult::NotFound: return "no enemy or generator has that name";
		case GeneratorStrategyResult::BadKind: return "the editor can't make a strategy of that kind";
		case GeneratorStrategyResult::TooBig: return "the strategy or the name is larger than any real one";
		case GeneratorStrategyResult::NoStrategy: return "it has no strategy";
		case GeneratorStrategyResult::CantBuild: return "the level's bytes could not be made again";
		case GeneratorStrategyResult::LoaderRefused: return "the game would not take the level that way, so nothing was changed";
		}

		return "unknown answer";
	}
}
