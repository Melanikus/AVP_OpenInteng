// See placed_hierarchy_strategy.h. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "placed_hierarchy_strategy.h"

#include <string.h>

#include <vector>

#include "field_model.h"       // model::MakeNewStrategy
#include "level_file.h"
#include "level_image.h"

namespace host
{
	using namespace image;

	namespace
	{
		size_t const kMostStrategyBytes = 65536;
		size_t const kIdBytes = 8;

		// The ID a PLACHIER's PLACHIDT holds, read inside the chunk's bounds: the name and its zero padded to four bytes, then the index (4), the place (12), the turn (16), the ID (8); false
		// when it has no PLACHIDT, the name has no end, or the bytes stop short
		bool PlacedIdOf(Bytes const &image, Span const &placed, uint8_t id[kIdBytes])
		{
			std::vector<Span> inside;

			if (!Children(image, placed.at + kHeadingBytes, placed.at + placed.size, &inside))
			{
				return false;
			}

			size_t const at = FirstNamed(image, inside, "PLACHIDT");

			if (at == inside.size() || inside[at].size < kHeadingBytes)
			{
				return false;
			}

			size_t const from = inside[at].at + kHeadingBytes;
			size_t const length = inside[at].size - kHeadingBytes;
			char const *const first = image.data() + from;
			char const *const zero = length ? (char const *)memchr(first, 0, length) : nullptr;

			if (!zero)
			{
				return false;
			}

			size_t const nameBytes = ((size_t)(zero - first) + 4) & ~(size_t)3;
			size_t const idAt = nameBytes + 4 + 12 + 16;

			if (length < idAt + kIdBytes)
			{
				return false;
			}

			memcpy(id, image.data() + from + idAt, kIdBytes);
			return true;
		}

		// Puts `strategy` (a whole AVPSTRAT chunk; empty: none) as the strategy of the placed hierarchy with this ID
		PlacedHierarchyStrategyResult Change(OpenLevelFile *file, uint8_t const *id, Bytes const &strategy)
		{
			if (!file || !file->Level() || !id)
			{
				return PlacedHierarchyStrategyResult::BadCall;
			}

			Bytes image;
			std::vector<Span> top, inside, special;

			if (!Walk(file, &image, &top))
			{
				return PlacedHierarchyStrategyResult::CantBuild;
			}

			size_t envAt = top.size();

			for (size_t i = 0; i < top.size() && envAt == top.size(); i++)
			{
				envAt = NameIs(image, top[i].at, "REBENVDT") ? i : envAt;
			}

			if (envAt == top.size() || !Children(image, top[envAt].at + kHeadingBytes, top[envAt].at + top[envAt].size, &inside))
			{
				return PlacedHierarchyStrategyResult::NotFound;
			}

			size_t const specAt = FirstNamed(image, inside, "SPECLOBJ");

			if (specAt == inside.size() || !Children(image, inside[specAt].at + kHeadingBytes, inside[specAt].at + inside[specAt].size, &special))
			{
				return PlacedHierarchyStrategyResult::NotFound;
			}

			// the first PLACHIER whose PLACHIDT holds the ID
			size_t placedAt = special.size();

			for (size_t i = 0; i < special.size() && placedAt == special.size(); i++)
			{
				uint8_t held[kIdBytes];

				if (NameIs(image, special[i].at, "PLACHIER") && PlacedIdOf(image, special[i], held) && memcmp(held, id, kIdBytes) == 0)
				{
					placedAt = i;
				}
			}

			if (placedAt == special.size())
			{
				return PlacedHierarchyStrategyResult::NotFound;
			}

			// the PLACHIER again: its children as they were, the AVPSTRAT replaced where it stands (or taken out), a new one at the end
			std::vector<Span> parts;

			if (!Children(image, special[placedAt].at + kHeadingBytes, special[placedAt].at + special[placedAt].size, &parts))
			{
				return PlacedHierarchyStrategyResult::CantBuild;
			}

			Bytes placed(image.begin() + (std::ptrdiff_t)special[placedAt].at, image.begin() + (std::ptrdiff_t)(special[placedAt].at + kHeadingBytes));
			bool had = false;

			for (Span const &part : parts)
			{
				if (NameIs(image, part.at, "AVPSTRAT") && !had)
				{
					had = true;
					placed.insert(placed.end(), strategy.begin(), strategy.end());
				}
				else if (!NameIs(image, part.at, "AVPSTRAT"))
				{
					AddCopy(&placed, image, part);
				}
			}

			if (!had)
			{
				if (strategy.empty())
				{
					return PlacedHierarchyStrategyResult::NoStrategy;
				}

				placed.insert(placed.end(), strategy.begin(), strategy.end());
			}

			PutIntAt(&placed, 8, (int32_t)placed.size());

			// SPECLOBJ, REBENVDT and the level with the sizes made again
			Bytes spec(image.begin() + (std::ptrdiff_t)inside[specAt].at, image.begin() + (std::ptrdiff_t)(inside[specAt].at + kHeadingBytes));

			for (size_t i = 0; i < special.size(); i++)
			{
				if (i == placedAt)
				{
					spec.insert(spec.end(), placed.begin(), placed.end());
				}
				else
				{
					AddCopy(&spec, image, special[i]);
				}
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
				return PlacedHierarchyStrategyResult::TooBig;
			}

			PutIntAt(&out, 8, (int32_t)out.size());

			LevelOpenResult const result = file->ReplaceLevel(out);

			if (result == LevelOpenResult::Opened)
			{
				return PlacedHierarchyStrategyResult::Done;
			}

			return result == LevelOpenResult::CantRead ? PlacedHierarchyStrategyResult::CantBuild : PlacedHierarchyStrategyResult::LoaderRefused;
		}
	}

	PlacedHierarchyStrategyResult PlacedHierarchyStrategy_Put(OpenLevelFile *file, uint8_t const *id, uint8_t const *payload, size_t length)
	{
		if (!payload || length < 12)
		{
			return PlacedHierarchyStrategyResult::BadCall;
		}

		if (length > kMostStrategyBytes)
		{
			return PlacedHierarchyStrategyResult::TooBig;
		}

		return Change(file, id, MakeChunk("AVPSTRAT", Bytes((char const *)payload, (char const *)payload + length)));
	}

	PlacedHierarchyStrategyResult PlacedHierarchyStrategy_Assign(OpenLevelFile *file, uint8_t const *id, int32_t kind)
	{
		uint8_t payload[1024];
		size_t length = 0;

		if (!model::MakeNewStrategy(kind, payload, sizeof(payload), &length))
		{
			return PlacedHierarchyStrategyResult::BadKind;
		}

		return PlacedHierarchyStrategy_Put(file, id, payload, length);
	}

	PlacedHierarchyStrategyResult PlacedHierarchyStrategy_Delete(OpenLevelFile *file, uint8_t const *id)
	{
		return Change(file, id, Bytes());
	}

	char const *PlacedHierarchyStrategyWords(PlacedHierarchyStrategyResult result)
	{
		switch (result)
		{
		case PlacedHierarchyStrategyResult::Done: return "done";
		case PlacedHierarchyStrategyResult::BadCall: return "no level, no object or no strategy to put";
		case PlacedHierarchyStrategyResult::NotFound: return "no placed hierarchy has that ID";
		case PlacedHierarchyStrategyResult::BadKind: return "the editor can't make a strategy of that kind";
		case PlacedHierarchyStrategyResult::TooBig: return "the strategy is larger than any real one";
		case PlacedHierarchyStrategyResult::NoStrategy: return "it has no strategy";
		case PlacedHierarchyStrategyResult::CantBuild: return "the level's bytes could not be made again";
		case PlacedHierarchyStrategyResult::LoaderRefused: return "the game would not take the level that way, so nothing was changed";
		}

		return "unknown answer";
	}
}
