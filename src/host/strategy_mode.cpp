// See strategy_mode.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "strategy_mode.h"

#include <map>
#include <string>
#include <vector>

#include <set>

#include "chunk_object.hpp"
#include "field_model.h"
#include "level_file.h"
#include "level_image.h"
#include "object_edit.h"           // DeleteObject: the pickups' objects go (FUN_00538e70)
#include "special_objects_edit.h"
#include "strategy_lookup.h"

namespace host
{
	namespace
	{
		using namespace image;

		// Where the object's strategy chunk is in the image: its RBOBJECT, its first OBJPRJDT, the first AVPSTRAT in that. Answers: Done (found, *objectAt
		// is the index among the top chunks), NoStrategy, NoObject, or CantBuild when the bytes can't be walked.
		StrategyModeResult Find(OpenLevelFile *file, uint8_t const *id, Bytes *image, std::vector<Span> *top, size_t *objectAt, size_t *projectAt,
		                        std::vector<Span> *inside, size_t *strategyAt, std::vector<Span> *project)
		{
			if (!file || !id || !file->Level())
			{
				return StrategyModeResult::BadCall;
			}

			std::string name;

			if (!NameOfObject(file->Level(), id, &name))
			{
				return StrategyModeResult::NoObject;
			}

			if (!Walk(file, image, top))
			{
				return StrategyModeResult::CantBuild;
			}

			std::map<std::string, size_t> byName;

			ObjectsByName(*image, *top, &byName);

			std::map<std::string, size_t>::const_iterator found = byName.find(name);

			if (found == byName.end())
			{
				return StrategyModeResult::NoObject;
			}

			*objectAt = found->second;

			Span const object = (*top)[*objectAt];

			if (!Children(*image, object.at + kHeadingBytes, object.at + object.size, inside))
			{
				return StrategyModeResult::CantBuild;
			}

			*projectAt = FirstNamed(*image, *inside, "OBJPRJDT");

			if (*projectAt == inside->size())
			{
				return StrategyModeResult::NoStrategy;
			}

			Span const holder = (*inside)[*projectAt];

			if (!Children(*image, holder.at + kHeadingBytes, holder.at + holder.size, project))
			{
				return StrategyModeResult::CantBuild;
			}

			*strategyAt = FirstNamed(*image, *project, "AVPSTRAT");
			return *strategyAt == project->size() ? StrategyModeResult::NoStrategy : StrategyModeResult::Done;
		}
	}

	char const *StrategyModeWords(StrategyModeResult result)
	{
		switch (result)
		{
		case StrategyModeResult::Done: return "done";
		case StrategyModeResult::BadCall: return "nothing to change";
		case StrategyModeResult::NoObject: return "no object has that name";
		case StrategyModeResult::NoStrategy: return "the object has no strategy";
		case StrategyModeResult::CantBuild: return "the level couldn't be put together from its bytes";
		case StrategyModeResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	char const *StrategyMode_DeleteQuestion()
	{
		return "Are you sure you want to delete the \nstrategy on this object?";
	}

	char const *StrategyMode_ResetFansQuestion()
	{
		return "Would you like to reset the wind strength for all fans";
	}

	StrategyModeResult StrategyMode_Has(OpenLevelFile *file, uint8_t const *id)
	{
		Bytes image;
		std::vector<Span> top, inside, project;
		size_t objectAt = 0, projectAt = 0, strategyAt = 0;

		return Find(file, id, &image, &top, &objectAt, &projectAt, &inside, &strategyAt, &project);
	}

	StrategyModeResult StrategyMode_Delete(OpenLevelFile *file, uint8_t const *id)
	{
		Bytes image;
		std::vector<Span> top, inside, project;
		size_t objectAt = 0, projectAt = 0, strategyAt = 0;
		StrategyModeResult const found = Find(file, id, &image, &top, &objectAt, &projectAt, &inside, &strategyAt, &project);

		if (found != StrategyModeResult::Done)
		{
			return found;
		}

		// The new image: everything as it was, the object's chunk made again with its first OBJPRJDT made again without that one AVPSTRAT; the sizes of the
		// OBJPRJDT, the object and the level made right
		Span const object = top[objectAt];
		Bytes out(image.begin(), image.begin() + (std::ptrdiff_t)object.at);
		size_t const objectStart = out.size();

		out.insert(out.end(), image.begin() + (std::ptrdiff_t)object.at, image.begin() + (std::ptrdiff_t)(object.at + kHeadingBytes));

		for (size_t i = 0; i < inside.size(); i++)
		{
			if (i != projectAt)
			{
				AddCopy(&out, image, inside[i]);
				continue;
			}

			size_t const projectStart = out.size();

			out.insert(out.end(), image.begin() + (std::ptrdiff_t)inside[i].at, image.begin() + (std::ptrdiff_t)(inside[i].at + kHeadingBytes));

			for (size_t j = 0; j < project.size(); j++)
			{
				if (j != strategyAt)
				{
					AddCopy(&out, image, project[j]);
				}
			}

			PutIntAt(&out, projectStart + 8, (int32_t)(out.size() - projectStart));
		}

		PutIntAt(&out, objectStart + 8, (int32_t)(out.size() - objectStart));
		out.insert(out.end(), image.begin() + (std::ptrdiff_t)(object.at + object.size), image.end());

		if (out.size() > kLargest)
		{
			return StrategyModeResult::CantBuild;
		}

		PutIntAt(&out, 8, (int32_t)out.size());

		LevelOpenResult const result = file->ReplaceLevel(out);

		if (result == LevelOpenResult::Opened)
		{
			return StrategyModeResult::Done;
		}

		return result == LevelOpenResult::CantRead ? StrategyModeResult::CantBuild : StrategyModeResult::LoaderRefused;
	}

	StrategyModeResult StrategyMode_ResetFans(OpenLevelFile *file, size_t *fans)
	{
		if (fans)
		{
			*fans = 0;
		}

		if (!file || !file->Level())
		{
			return StrategyModeResult::BadCall;
		}

		Bytes out;
		std::vector<Span> top;

		if (!Walk(file, &out, &top))
		{
			return StrategyModeResult::CantBuild;
		}

		// The kind is the third number of an AVPSTRAT payload (offset 8); a fan's is 75 (0x4b). Its wind strength is at 28 and the pad after it at 32.
		size_t const kFanKind = 75, kKindAt = 8, kWindAt = 28, kPadAt = 32, kFanPayload = 36;
		size_t count = 0;

		for (Span const &chunk : top)
		{
			if (!NameIs(out, chunk.at, "RBOBJECT"))
			{
				continue;
			}

			std::vector<Span> inside, project;

			if (!Children(out, chunk.at + kHeadingBytes, chunk.at + chunk.size, &inside))
			{
				return StrategyModeResult::CantBuild;
			}

			size_t const holder = FirstNamed(out, inside, "OBJPRJDT");

			if (holder == inside.size() || !Children(out, inside[holder].at + kHeadingBytes, inside[holder].at + inside[holder].size, &project))
			{
				continue;
			}

			size_t const strategy = FirstNamed(out, project, "AVPSTRAT");

			if (strategy == project.size() || project[strategy].size < kHeadingBytes + kFanPayload)
			{
				continue;
			}

			size_t const payload = project[strategy].at + kHeadingBytes;

			if (IntAt(out, payload + kKindAt) == (int32_t)kFanKind)
			{
				PutIntAt(&out, payload + kWindAt, 100);
				PutIntAt(&out, payload + kPadAt, 0);
				count++;
			}
		}

		if (count == 0)
		{
			return StrategyModeResult::NoStrategy;
		}

		LevelOpenResult const result = file->ReplaceLevel(out);

		if (result == LevelOpenResult::Opened)
		{
			if (fans)
			{
				*fans = count;
			}

			return StrategyModeResult::Done;
		}

		return result == LevelOpenResult::CantRead ? StrategyModeResult::CantBuild : StrategyModeResult::LoaderRefused;
	}

	StrategyModeResult StrategyMode_SetModuleFlag(OpenLevelFile *file, uint8_t const *ids, size_t count, uint32_t bit, bool add, size_t *changed)
	{
		if (changed)
		{
			*changed = 0;
		}

		if (!file || !file->Level() || (count != 0 && !ids) || count > 100000)
		{
			return StrategyModeResult::BadCall;
		}

		Bytes image;
		std::vector<Span> top;

		if (!Walk(file, &image, &top))
		{
			return StrategyModeResult::CantBuild;
		}

		std::map<std::string, size_t> byName;

		ObjectsByName(image, top, &byName);

		std::map<size_t, Bytes> made;   // the object's new chunk, by its place among the top chunks

		for (size_t i = 0; i < count; i++)
		{
			std::string name;

			if (!NameOfObject(file->Level(), ids + 8 * i, &name))
			{
				continue;
			}

			std::map<std::string, size_t>::const_iterator found = byName.find(name);

			if (found == byName.end() || made.count(found->second))
			{
				continue;   // not in the bytes, or already done (a module twice is the same)
			}

			Span const object = top[found->second];
			std::vector<Span> inside;

			if (!Children(image, object.at + kHeadingBytes, object.at + object.size, &inside))
			{
				return StrategyModeResult::CantBuild;
			}

			// a placed object (header flags & 0x400) is skipped
			size_t const head = FirstNamed(image, inside, "OBJHEAD1");

			if (head == inside.size() || inside[head].size < kHeadingBytes + sizeof(uint32_t))
			{
				continue;
			}

			uint32_t headFlags;

			memcpy(&headFlags, image.data() + inside[head].at + kHeadingBytes, sizeof(headFlags));

			if ((headFlags & 0x400) != 0)
			{
				continue;
			}

			size_t const module = FirstNamed(image, inside, "MODULEDT");
			std::vector<Span> parts;
			size_t flagsAt = 0;

			if (module != inside.size())
			{
				if (!Children(image, inside[module].at + kHeadingBytes, inside[module].at + inside[module].size, &parts))
				{
					return StrategyModeResult::CantBuild;
				}

				flagsAt = FirstNamed(image, parts, "MODFLAGS");
			}
			else if (!add)
			{
				continue;
			}

			if ((module == inside.size() || flagsAt == parts.size()) && !add)
			{
				continue;
			}

			// the flag chunk's payload, the old one's or 8 zero bytes
			Bytes payload(8, 0);

			if (module != inside.size() && flagsAt != parts.size())
			{
				if (parts[flagsAt].size < kHeadingBytes + sizeof(uint32_t))
				{
					return StrategyModeResult::CantBuild;
				}

				payload.assign(image.begin() + (std::ptrdiff_t)(parts[flagsAt].at + kHeadingBytes), image.begin() + (std::ptrdiff_t)(parts[flagsAt].at + parts[flagsAt].size));
			}

			uint32_t word;

			memcpy(&word, payload.data(), sizeof(word));
			word = add ? (word | bit) : (word & ~bit);
			memcpy(payload.data(), &word, sizeof(word));

			Bytes const flagChunk = MakeChunk("MODFLAGS", payload);

			// the module's chunk made again: MODULEDT with its MODFLAGS replaced (or put last), MODULEDT itself last when it was missing
			Bytes out;
			size_t const start = out.size();

			out.insert(out.end(), image.begin() + (std::ptrdiff_t)object.at, image.begin() + (std::ptrdiff_t)(object.at + kHeadingBytes));

			for (size_t k = 0; k < inside.size(); k++)
			{
				if (k != module)
				{
					AddCopy(&out, image, inside[k]);
					continue;
				}

				size_t const moduleStart = out.size();

				out.insert(out.end(), image.begin() + (std::ptrdiff_t)inside[k].at, image.begin() + (std::ptrdiff_t)(inside[k].at + kHeadingBytes));

				for (size_t p = 0; p < parts.size(); p++)
				{
					if (p == flagsAt)
					{
						out.insert(out.end(), flagChunk.begin(), flagChunk.end());
					}
					else
					{
						AddCopy(&out, image, parts[p]);
					}
				}

				if (flagsAt == parts.size())
				{
					out.insert(out.end(), flagChunk.begin(), flagChunk.end());
				}

				PutIntAt(&out, moduleStart + 8, (int32_t)(out.size() - moduleStart));
			}

			if (module == inside.size())
			{
				Bytes const moduleChunk = MakeChunk("MODULEDT", flagChunk);

				out.insert(out.end(), moduleChunk.begin(), moduleChunk.end());
			}

			PutIntAt(&out, start + 8, (int32_t)(out.size() - start));
			made[found->second] = out;
		}

		if (made.empty())
		{
			return StrategyModeResult::Done;   // nothing to change: the level stays as it is
		}

		Bytes result(image.begin(), image.begin() + (std::ptrdiff_t)kHeadingBytes);

		for (size_t i = 0; i < top.size(); i++)
		{
			std::map<size_t, Bytes>::const_iterator again = made.find(i);

			if (again != made.end())
			{
				result.insert(result.end(), again->second.begin(), again->second.end());
			}
			else
			{
				AddCopy(&result, image, top[i]);
			}
		}

		if (result.size() > kLargest)
		{
			return StrategyModeResult::CantBuild;
		}

		PutIntAt(&result, 8, (int32_t)result.size());

		LevelOpenResult const loaded = file->ReplaceLevel(result);

		if (loaded == LevelOpenResult::Opened)
		{
			if (changed)
			{
				*changed = made.size();
			}

			return StrategyModeResult::Done;
		}

		return loaded == LevelOpenResult::CantRead ? StrategyModeResult::CantBuild : StrategyModeResult::LoaderRefused;
	}

	namespace
	{
		// The module's RBOBJECT index among the top chunks, its shape number (OBJHEAD1 payload +56), and the index of the REBSHAPE whose SHPHEAD1 number (+20) is that
		StrategyModeResult FindShape(OpenLevelFile *file, uint8_t const *id, Bytes *image, std::vector<Span> *top, size_t *shapeAt, size_t *headAt)
		{
			if (!file || !id || !file->Level())
			{
				return StrategyModeResult::BadCall;
			}

			std::string name;

			if (!NameOfObject(file->Level(), id, &name))
			{
				return StrategyModeResult::NoObject;
			}

			if (!Walk(file, image, top))
			{
				return StrategyModeResult::CantBuild;
			}

			std::map<std::string, size_t> byName;

			ObjectsByName(*image, *top, &byName);

			std::map<std::string, size_t>::const_iterator found = byName.find(name);

			if (found == byName.end())
			{
				return StrategyModeResult::NoObject;
			}

			std::vector<Span> inside;
			Span const object = (*top)[found->second];

			if (!Children(*image, object.at + kHeadingBytes, object.at + object.size, &inside))
			{
				return StrategyModeResult::CantBuild;
			}

			size_t const head = FirstNamed(*image, inside, "OBJHEAD1");

			if (head == inside.size() || inside[head].size < kHeadingBytes + 60)
			{
				return StrategyModeResult::NoStrategy;
			}

			int32_t const number = IntAt(*image, inside[head].at + kHeadingBytes + 56);

			for (size_t i = 0; i < top->size(); i++)
			{
				if (!NameIs(*image, (*top)[i].at, "REBSHAPE"))
				{
					continue;
				}

				std::vector<Span> parts;

				if (!Children(*image, (*top)[i].at + kHeadingBytes, (*top)[i].at + (*top)[i].size, &parts))
				{
					return StrategyModeResult::CantBuild;
				}

				size_t const shead = FirstNamed(*image, parts, "SHPHEAD1");

				if (shead != parts.size() && parts[shead].size >= kHeadingBytes + 24 && IntAt(*image, parts[shead].at + kHeadingBytes + 20) == number)
				{
					*shapeAt = i;
					*headAt = parts[shead].at;
					return StrategyModeResult::Done;
				}
			}

			return StrategyModeResult::NoStrategy;
		}
	}

	StrategyModeResult StrategyMode_GetStability(OpenLevelFile *file, uint8_t const *id, uint32_t *faces)
	{
		Bytes image;
		std::vector<Span> top;
		size_t shapeAt = 0, headAt = 0;
		StrategyModeResult const found = FindShape(file, id, &image, &top, &shapeAt, &headAt);

		*faces = 0;

		if (found != StrategyModeResult::Done)
		{
			return found;
		}

		uint32_t const word = (uint32_t)IntAt(image, headAt + kHeadingBytes);

		for (int i = 0; i < 6; i++)
		{
			*faces |= (word & kStabilityBits[i]) ? (1u << i) : 0u;
		}

		return StrategyModeResult::Done;
	}

	StrategyModeResult StrategyMode_SetStability(OpenLevelFile *file, uint8_t const *id, uint32_t faces)
	{
		Bytes image;
		std::vector<Span> top;
		size_t shapeAt = 0, headAt = 0;
		StrategyModeResult const found = FindShape(file, id, &image, &top, &shapeAt, &headAt);

		if (found != StrategyModeResult::Done)
		{
			return found;
		}

		uint32_t word = (uint32_t)IntAt(image, headAt + kHeadingBytes);

		for (int i = 0; i < 6; i++)
		{
			word = (faces & (1u << i)) ? (word | kStabilityBits[i]) : (word & ~kStabilityBits[i]);
		}

		PutIntAt(&image, headAt + kHeadingBytes, (int32_t)word);

		LevelOpenResult const result = file->ReplaceLevel(image);

		if (result == LevelOpenResult::Opened)
		{
			return StrategyModeResult::Done;
		}

		return result == LevelOpenResult::CantRead ? StrategyModeResult::CantBuild : StrategyModeResult::LoaderRefused;
	}

	namespace
	{
		struct FlagRow
		{
			int32_t add;          // the add command; remove is add + 1, view add + 2
			uint32_t bit;
		};

		// Airduct, Stair, Sky Visible, Fogged (set 3), then Victory Zone, Sneaky, Exclude from map, Not Passable, Not a Room, Draw Back Colour (set 4)
		FlagRow const kFlagRows[] =
		{
			{ 0xe, 0x80000000u }, { 0x11, 0x40000000u }, { 0x14, 0x20000000u }, { 0x17, 0x10000000u },
			{ 0x1a, 0x10000u }, { 0x1d, 0x20000u }, { 0x26, 0x200u }, { 0x29, 0x100000u }, { 0x2f, 0x400u }, { 0x32, 0x800u },
		};
	}

	size_t StrategyMode_FlagSteps(int32_t command, StrategyFlagStep *steps)
	{
		if (!steps)
		{
			return 0;
		}

		for (FlagRow const &row : kFlagRows)
		{
			if (command == row.add || command == row.add + 1)
			{
				steps[0].bit = row.bit;
				steps[0].add = command == row.add;
				return 1;
			}
		}

		// Doorway: 0x20 clears Not a Doorway's bit and sets its own; 0x21 clears its own. Not a Doorway: 0x23 the other way round; 0x24 clears its own.
		switch (command)
		{
			case 0x20:
				steps[0].bit = 0x80000u; steps[0].add = false;
				steps[1].bit = 0x40000u; steps[1].add = true;
				return 2;
			case 0x21:
				steps[0].bit = 0x40000u; steps[0].add = false;
				return 1;
			case 0x23:
				steps[0].bit = 0x40000u; steps[0].add = false;
				steps[1].bit = 0x80000u; steps[1].add = true;
				return 2;
			case 0x24:
				steps[0].bit = 0x80000u; steps[0].add = false;
				return 1;
			case 0x2d:
				steps[0].bit = kStrategyMessageBits; steps[0].add = false;
				return 1;
			default:
				return 0;
		}
	}

	uint32_t StrategyMode_ViewFlagBit(int32_t command)
	{
		for (FlagRow const &row : kFlagRows)
		{
			if (command == row.add + 2)
			{
				return row.bit;
			}
		}

		switch (command)
		{
			case 0x22: return 0x40000u;
			case 0x25: return 0x80000u;
			case 0x2e: return kStrategyMessageBits;
			default: return 0;
		}
	}

	namespace
	{
		StrategyGroup const kGroups[] =
		{
			{ "Switches", { 52, 61, 66, 68 }, 4 },
			{ "Lighting effects", { 64, 0, 0, 0 }, 1 },
			{ "Mission Objective", { 70, 0, 0, 0 }, 1 },
			{ "Messages", { 73, 0, 0, 0 }, 1 },
			{ "Volume of Death", { 77, 0, 0, 0 }, 1 },
			{ "Self Destruct", { 78, 0, 0, 0 }, 1 },
		};

		// The kind of an AVPSTRAT: after its two numbers; false when the chunk is too short for a kind
		bool KindOf(Bytes const &bytes, Span const &chunk, int32_t *kind)
		{
			if (chunk.size < kHeadingBytes + 12)
			{
				return false;
			}

			*kind = IntAt(bytes, chunk.at + kHeadingBytes + 8);
			return true;
		}

		bool Wanted(int32_t kind, int32_t const *kinds, size_t kindCount)
		{
			for (size_t i = 0; i < kindCount; i++)
			{
				if (kinds[i] == kind)
				{
					return true;
				}
			}

			return false;
		}

		// A holder made again without the children that `goes` says: its heading, then the others, with its size right
		void MakeWithout(Bytes const &image, Span const &holder, std::vector<Span> const &children, std::vector<bool> const &goes, Bytes *out)
		{
			size_t const start = out->size();

			out->insert(out->end(), image.begin() + (std::ptrdiff_t)holder.at, image.begin() + (std::ptrdiff_t)(holder.at + kHeadingBytes));

			for (size_t i = 0; i < children.size(); i++)
			{
				if (!goes[i])
				{
					AddCopy(out, image, children[i]);
				}
			}

			PutIntAt(out, start + 8, (int32_t)(out->size() - start));
		}
	}

	size_t StrategyMode_Groups(StrategyGroup const **groups)
	{
		if (groups)
		{
			*groups = kGroups;
		}

		return sizeof(kGroups) / sizeof(kGroups[0]);
	}

	StrategyModeResult StrategyMode_DeleteKinds(OpenLevelFile *file, int32_t const *kinds, size_t kindCount, size_t *deleted)
	{
		if (deleted)
		{
			*deleted = 0;
		}

		if (!file || !file->Level() || !kinds || kindCount == 0)
		{
			return StrategyModeResult::BadCall;
		}

		Bytes image;
		std::vector<Span> top;

		if (!Walk(file, &image, &top))
		{
			return StrategyModeResult::CantBuild;
		}

		Bytes out(image.begin(), image.begin() + (std::ptrdiff_t)kHeadingBytes);
		size_t count = 0;

		for (Span const &chunk : top)
		{
			std::vector<Span> inside;
			bool rebuilt = false;

			if (NameIs(image, chunk.at, "RBOBJECT") && Children(image, chunk.at + kHeadingBytes, chunk.at + chunk.size, &inside))
			{
				size_t const projectAt = FirstNamed(image, inside, "OBJPRJDT");
				std::vector<Span> project;

				if (projectAt != inside.size() && Children(image, inside[projectAt].at + kHeadingBytes, inside[projectAt].at + inside[projectAt].size, &project))
				{
					size_t const strategyAt = FirstNamed(image, project, "AVPSTRAT");
					int32_t kind = 0;

					if (strategyAt != project.size() && KindOf(image, project[strategyAt], &kind) && Wanted(kind, kinds, kindCount))
					{
						// the object made again with its OBJPRJDT made again without that AVPSTRAT
						size_t const objectStart = out.size();

						out.insert(out.end(), image.begin() + (std::ptrdiff_t)chunk.at, image.begin() + (std::ptrdiff_t)(chunk.at + kHeadingBytes));

						for (size_t i = 0; i < inside.size(); i++)
						{
							if (i == projectAt)
							{
								std::vector<bool> goes(project.size(), false);

								goes[strategyAt] = true;
								MakeWithout(image, inside[i], project, goes, &out);
							}
							else
							{
								AddCopy(&out, image, inside[i]);
							}
						}

						PutIntAt(&out, objectStart + 8, (int32_t)(out.size() - objectStart));
						count++;
						rebuilt = true;
					}
				}
			}
			else if (NameIs(image, chunk.at, "REBENVDT") && Children(image, chunk.at + kHeadingBytes, chunk.at + chunk.size, &inside))
			{
				size_t const specAt = FirstNamed(image, inside, "SPECLOBJ");
				std::vector<Span> special;

				if (specAt != inside.size() && Children(image, inside[specAt].at + kHeadingBytes, inside[specAt].at + inside[specAt].size, &special))
				{
					std::vector<bool> goes(special.size(), false);
					size_t here = 0;

					for (size_t i = 0; i < special.size(); i++)
					{
						std::vector<Span> parts;

						if (!NameIs(image, special[i].at, "VIOBJECT") || !Children(image, special[i].at + kHeadingBytes, special[i].at + special[i].size, &parts))
						{
							continue;
						}

						size_t const strategyAt = FirstNamed(image, parts, "AVPSTRAT");
						int32_t kind = 0;

						if (strategyAt != parts.size() && KindOf(image, parts[strategyAt], &kind) && Wanted(kind, kinds, kindCount))
						{
							goes[i] = true;
							here++;
						}
					}

					if (here > 0)
					{
						size_t const envStart = out.size();

						out.insert(out.end(), image.begin() + (std::ptrdiff_t)chunk.at, image.begin() + (std::ptrdiff_t)(chunk.at + kHeadingBytes));

						for (size_t i = 0; i < inside.size(); i++)
						{
							if (i == specAt)
							{
								MakeWithout(image, inside[i], special, goes, &out);
							}
							else
							{
								AddCopy(&out, image, inside[i]);
							}
						}

						PutIntAt(&out, envStart + 8, (int32_t)(out.size() - envStart));
						count += here;
						rebuilt = true;
					}
				}
			}

			if (!rebuilt)
			{
				AddCopy(&out, image, chunk);
			}
		}

		if (deleted)
		{
			*deleted = count;
		}

		if (count == 0)
		{
			return StrategyModeResult::Done;
		}

		if (out.size() > kLargest)
		{
			return StrategyModeResult::CantBuild;
		}

		PutIntAt(&out, 8, (int32_t)out.size());

		LevelOpenResult const result = file->ReplaceLevel(out);

		if (result == LevelOpenResult::Opened)
		{
			return StrategyModeResult::Done;
		}

		return result == LevelOpenResult::CantRead ? StrategyModeResult::CantBuild : StrategyModeResult::LoaderRefused;
	}

	namespace
	{
		// One whole number of a strategy's payload by the field's name (the table's own place for it); false when the kind or the field is not known or the bytes are too short
		bool ReadStrategyNumber(std::vector<uint8_t> const &payload, int32_t kindNumber, char const *name, int32_t *value)
		{
			model::StrategyKind const *const kind = model::FindStrategyKind(kindNumber);

			if (!kind)
			{
				return false;
			}

			for (size_t i = 0; i < kind->fieldCount; i++)
			{
				if (strcmp(kind->fields[i].name, name) == 0)
				{
					size_t place = 0;

					if (!model::FieldPlace(*kind, kind->fields[i], payload.data(), payload.size(), &place) || place + 4 > payload.size())
					{
						return false;
					}

					memcpy(value, payload.data() + place, 4);
					return true;
				}
			}

			return false;
		}

		// The objects of the level with a strategy: the ID, the kind, the payload, and whether the object is a PLACED one (OBJECT_FLAG_PLACED_OBJECT in its header:
		// what inteng's record for it holds at +0x2c, FUN_005387b0 0x538814)
		struct LiveStrategy
		{
			uint8_t id[8];
			int32_t kind;
			std::vector<uint8_t> payload;
			bool placed;
		};

		void LiveStrategies(OpenLevelFile *file, std::vector<LiveStrategy> *out)
		{
			List<Object_Chunk *> objects;

			file->Level()->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();
				LiveStrategy one;
				ObjectStrategy now;

				if (!object)
				{
					continue;
				}

				memcpy(one.id, &object->object_data.ID, 8);
				one.placed = object->get_header() && (object->get_header()->flags & OBJECT_FLAG_PLACED_OBJECT) != 0;

				if (FindObjectStrategy(file->Level(), one.id, &now) == StrategyFound::Found && now.payload.size() >= 12)
				{
					memcpy(&one.kind, now.payload.data() + 8, 4);
					one.payload = now.payload;
					out->push_back(one);
				}
			}
		}

		// Takes the strategy off each of these objects, one change after another (each is read again by the game's loader before it is kept)
		StrategyModeResult DeleteTheseStrategies(OpenLevelFile *file, std::vector<std::vector<uint8_t> > const &ids, size_t *deleted)
		{
			for (std::vector<uint8_t> const &id : ids)
			{
				StrategyModeResult const result = StrategyMode_Delete(file, id.data());

				if (result != StrategyModeResult::Done)
				{
					return result;
				}

				if (deleted)
				{
					(*deleted)++;
				}
			}

			return StrategyModeResult::Done;
		}
	}

	StrategyModeResult StrategyMode_DeletePickups(OpenLevelFile *file, bool marines, bool predators, size_t *deleted)
	{
		if (deleted)
		{
			*deleted = 0;
		}

		if (!file || !file->Level())
		{
			return StrategyModeResult::BadCall;
		}

		std::vector<LiveStrategy> all;
		std::vector<std::vector<uint8_t> > ids;

		LiveStrategies(file, &all);

		for (LiveStrategy const &one : all)
		{
			int32_t type = 0;

			// FUN_005373e0: the Placed Object strategies (0x3e) whose type is above 1 and below 4 (marines) or 15 (predators). For each it calls the object's record's
			// delete (+0x14, 0x5374f5), FUN_00538e70: a PLACED object (+0x2c) is deleted, its strategy with it (+0x60 |= 1, as Object Placement's Delete 0x4e0480 marks it;
			// its shape is left in the file); for any other object it does nothing, so a module keeps its strategy
			if (one.placed && one.kind == 62 && ReadStrategyNumber(one.payload, 62, "Type", &type) && type > 1 && ((type < 4 && marines) || (type == 15 && predators)))
			{
				ids.push_back(std::vector<uint8_t>(one.id, one.id + 8));
			}
		}

		// one change after another, each read again by the game's loader before it is kept (object_edit.h DeleteObject)
		for (std::vector<uint8_t> const &id : ids)
		{
			ObjectEditResult const result = DeleteObject(file, id.data());

			if (result != ObjectEditResult::Done)
			{
				return result == ObjectEditResult::LoaderRefused ? StrategyModeResult::LoaderRefused : (result == ObjectEditResult::NoSuchObject ? StrategyModeResult::NoObject : StrategyModeResult::CantBuild);
			}

			if (deleted)
			{
				(*deleted)++;
			}
		}

		return StrategyModeResult::Done;
	}

	StrategyModeResult StrategyMode_DeleteDoorsNonLift(OpenLevelFile *file, size_t *deleted)
	{
		if (deleted)
		{
			*deleted = 0;
		}

		if (!file || !file->Level())
		{
			return StrategyModeResult::BadCall;
		}

		std::vector<LiveStrategy> all;
		std::set<std::string> liftDoors;
		std::vector<std::vector<uint8_t> > ids;

		LiveStrategies(file, &all);

		// FUN_00536de0: first every Switch Door (0x3b) goes; then each Lift's (0x33) associated door is taken off the list of Doors (0x32), and the doors left lose their strategy
		for (LiveStrategy const &one : all)
		{
			if (one.kind == 51)
			{
				model::StrategyKind const *const kind = model::FindStrategyKind(51);

				for (size_t i = 0; kind && i < kind->fieldCount; i++)
				{
					size_t place = 0;

					if (strcmp(kind->fields[i].name, "AssocDoor") == 0 && model::FieldPlace(*kind, kind->fields[i], one.payload.data(), one.payload.size(), &place) && place + 8 <= one.payload.size())
					{
						liftDoors.insert(std::string((char const *)one.payload.data() + place, 8));
					}
				}
			}
		}

		for (LiveStrategy const &one : all)
		{
			if (one.kind == 59 || (one.kind == 50 && liftDoors.count(std::string((char const *)one.id, 8)) == 0))
			{
				ids.push_back(std::vector<uint8_t>(one.id, one.id + 8));
			}
		}

		return DeleteTheseStrategies(file, ids, deleted);
	}

	StrategyModeResult StrategyMode_DeleteSpecies(OpenLevelFile *file, uint32_t speciesMask, size_t *deleted)
	{
		if (deleted)
		{
			*deleted = 0;
		}

		if (!file || !file->Level())
		{
			return StrategyModeResult::BadCall;
		}

		std::vector<SpecialObject> specials;

		if (SpecialObjects_Read(file, &specials) != SpecialResult::Done)
		{
			return StrategyModeResult::CantBuild;
		}

		// the highest number of each kind first, so the places of the ones still to go do not move
		for (size_t i = specials.size(); i > 0; i--)
		{
			SpecialObject const &one = specials[i - 1];

			if ((one.kind != SpecialKind::Generator && one.kind != SpecialKind::BadGuy) || one.type < 0 || one.type > 9 || (speciesMask & (1u << one.type)) == 0)
			{
				continue;
			}

			if (SpecialObject_Delete(file, one.kind, one.number) != SpecialResult::Done)
			{
				return StrategyModeResult::CantBuild;
			}

			if (deleted)
			{
				(*deleted)++;
			}
		}

		return StrategyModeResult::Done;
	}

	StrategyModeResult StrategyMode_FlagWordChanged(OpenLevelFile *file, uint8_t const *id, uint32_t oldWord, uint32_t newWord, size_t *changed)
	{
		if (changed)
		{
			*changed = 0;
		}

		uint32_t const removed = oldWord & ~newWord;
		uint32_t const added = newWord & ~oldWord;
		size_t one = 0;
		StrategyModeResult result = StrategyModeResult::Done;

		if (removed)
		{
			result = StrategyMode_SetModuleFlag(file, id, 1, removed, false, &one);
		}

		if (result == StrategyModeResult::Done && added)
		{
			size_t two = 0;

			result = StrategyMode_SetModuleFlag(file, id, 1, added, true, &two);
			one = one > two ? one : two;
		}

		if (changed)
		{
			*changed = one;
		}

		return result;
	}
}
