// See object_type_edit.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "object_type_edit.h"

#include <string.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "chunk_object.hpp"   // Object_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile: BuildLevel, ReplaceLevel
#include "level_image.h"      // the walking of the level's bytes

namespace host
{
	namespace
	{
		typedef image::Bytes Image;

		using image::Span;
		using image::IntAt;
		using image::PutIntAt;
		using image::AddInt;
		using image::NameIs;
		using image::Children;
		using image::FirstNamed;
		using image::AddCopy;
		using image::MakeChunk;
		using image::NameOfObject;
		using image::Walk;
		using image::ObjectsByName;
		using image::kHeadingBytes;
		using image::kLargest;

		// The header's flags (OBJHEAD1's first number: chunk_object.hpp OBJECT_FLAG_*)
		uint32_t const kFlagModule = 0x200;
		uint32_t const kFlagPlaced = 0x400;
		uint32_t const kFlagGameModes = 0x3800;  // Marine 0x800, Alien 0x1000, Predator 0x2000

		// FUN_00448990(0, 0, 0, 0): the strategy of a placed object as Change Object Type makes it. The 13 numbers are the ones object_make.cpp writes
		// for the Quick-create object (FUN_0052b120(0x3e)'s record), with this handler's own: Type 0, ExtraData 0, mass 0 (a, b, c = 0), integrity
		// 0 | 0x80000000, flags 1 (this + 0x24), no target request and no target (this + 0x28, +0x2c, +0x30 = 0).
		Image PlacedStrategyChunk()
		{
			Image strategy;

			AddInt(&strategy, 0);                          // index
			AddInt(&strategy, 0);                          // spare
			AddInt(&strategy, 62);                         // the kind
			AddInt(&strategy, 0);
			AddInt(&strategy, 0);
			AddInt(&strategy, 0);                          // Type
			AddInt(&strategy, 0);                          // ExtraData
			AddInt(&strategy, 0);                          // mass
			AddInt(&strategy, (int32_t)0x80000000u);       // integrity
			AddInt(&strategy, 1);                          // flags
			AddInt(&strategy, 0);                          // target_request
			AddInt(&strategy, 0);                          // targetID, two numbers
			AddInt(&strategy, 0);
			return MakeChunk("AVPSTRAT", strategy);
		}

		// The OBJPRJDT chunk at `project` made again: everything but its AVPSTRAT children, and (Placed) the new strategy put last. False: sizes wrong.
		bool RebuiltProject(Image const &image, Span const &project, bool addStrategy, Image *out)
		{
			std::vector<Span> inside;

			if (!Children(image, project.at + kHeadingBytes, project.at + project.size, &inside))
			{
				return false;
			}

			size_t const start = out->size();

			out->insert(out->end(), image.begin() + (std::ptrdiff_t)project.at, image.begin() + (std::ptrdiff_t)(project.at + kHeadingBytes));

			for (Span const &child : inside)
			{
				if (!NameIs(image, child.at, "AVPSTRAT"))
				{
					AddCopy(out, image, child);
				}
			}

			if (addStrategy)
			{
				Image const strategy = PlacedStrategyChunk();

				out->insert(out->end(), strategy.begin(), strategy.end());
			}

			if (out->size() - start > kLargest)
			{
				return false;
			}

			PutIntAt(out, start + 8, (int32_t)(out->size() - start));
			return true;
		}

		// What one object becomes. False: the bytes can't be put together (nothing in *out is good). *outcome says what happened to this object;
		// when it isn't Changed, *out is left alone.
		bool ChangedObject(Image const &image, Span const &object, ObjectTypeWhat what, bool deleteModuleData, Image *out, ObjectTypeOutcome *outcome)
		{
			std::vector<Span> inside;

			if (!Children(image, object.at + kHeadingBytes, object.at + object.size, &inside))
			{
				return false;
			}

			size_t const module = FirstNamed(image, inside, "MODULEDT");
			size_t const head = FirstNamed(image, inside, "OBJHEAD1");
			size_t const project = FirstNamed(image, inside, "OBJPRJDT");
			bool const placed = what == ObjectTypeWhat::Placed;

			if (module != inside.size() && (!placed || !deleteModuleData))
			{
				*outcome = ObjectTypeOutcome::HasModuleData;
				return true;
			}

			// FUN_004d0fc0 finds the header by name: a level with none (or one with no room for its flags) isn't one inteng's handler could work on
			if (head == inside.size() || inside[head].size < kHeadingBytes + sizeof(uint32_t))
			{
				return false;
			}

			size_t const start = out->size();

			out->insert(out->end(), image.begin() + (std::ptrdiff_t)object.at, image.begin() + (std::ptrdiff_t)(object.at + kHeadingBytes));

			for (size_t i = 0; i < inside.size(); i++)
			{
				if (i == module)
				{
					continue;   // Placed, the answer yes: the first MODULEDT is destroyed
				}

				if (i == head)
				{
					size_t const at = out->size();
					uint32_t flags;

					AddCopy(out, image, inside[i]);
					memcpy(&flags, out->data() + at + kHeadingBytes, sizeof(flags));

					if (placed)
					{
						flags = (flags | kFlagGameModes | kFlagPlaced) & ~kFlagModule;
					}
					else
					{
						flags = (flags & ~kFlagPlaced) | kFlagModule;
					}

					memcpy(out->data() + at + kHeadingBytes, &flags, sizeof(flags));
				}
				else if (i == project)
				{
					if (!RebuiltProject(image, inside[i], placed, out))
					{
						return false;
					}
				}
				else
				{
					AddCopy(out, image, inside[i]);
				}
			}

			if (placed && project == inside.size())
			{
				// No OBJPRJDT: a new one, last in the object, holding the new strategy
				Image const made = MakeChunk("OBJPRJDT", PlacedStrategyChunk());

				out->insert(out->end(), made.begin(), made.end());
			}

			if (out->size() - start > kLargest)
			{
				return false;
			}

			PutIntAt(out, start + 8, (int32_t)(out->size() - start));
			*outcome = ObjectTypeOutcome::Changed;
			return true;
		}

		bool AllZero(uint8_t const *id)
		{
			for (int i = 0; i < 8; i++)
			{
				if (id[i] != 0)
				{
					return false;
				}
			}

			return true;
		}
	}

	char const *ObjectTypeWords(ObjectTypeResult result)
	{
		switch (result)
		{
		case ObjectTypeResult::Done: return "done";
		case ObjectTypeResult::BadCall: return "nothing to change";
		case ObjectTypeResult::CantBuild: return "the level couldn't be put together from its bytes";
		case ObjectTypeResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	bool ObjectType_HaveModuleData(OpenLevelFile *file, uint8_t const *ids, size_t count, bool *has)
	{
		for (size_t i = 0; has && i < count; i++)
		{
			has[i] = false;
		}

		if (!file || !file->Level() || !ids || !has || count > kObjectTypeMostCalls)
		{
			return false;
		}

		Image image;
		std::vector<Span> top;
		std::map<std::string, size_t> byName;

		if (!Walk(file, &image, &top))
		{
			return false;
		}

		ObjectsByName(image, top, &byName);

		for (size_t i = 0; i < count; i++)
		{
			std::string name;
			std::map<std::string, size_t>::const_iterator found;
			std::vector<Span> inside;

			if (!NameOfObject(file->Level(), ids + 8 * i, &name) || (found = byName.find(name)) == byName.end())
			{
				continue;
			}

			Span const object = top[found->second];

			if (!Children(image, object.at + kHeadingBytes, object.at + object.size, &inside))
			{
				return false;
			}

			has[i] = FirstNamed(image, inside, "MODULEDT") != inside.size();
		}

		return true;
	}

	ObjectTypeResult ObjectType_Change(OpenLevelFile *file, ObjectTypeWhat what, ObjectTypeCall const *calls, size_t count, ObjectTypeOutcome *outcomes)
	{
		if (!file || !file->Level() || !calls || !outcomes || count == 0 || count > kObjectTypeMostCalls)
		{
			return ObjectTypeResult::BadCall;
		}

		for (size_t i = 0; i < count; i++)
		{
			outcomes[i] = ObjectTypeOutcome::NoObject;

			if (AllZero(calls[i].id))
			{
				return ObjectTypeResult::BadCall;
			}
		}

		Image image;
		std::vector<Span> top;
		std::map<std::string, size_t> byName;

		if (!Walk(file, &image, &top))
		{
			return ObjectTypeResult::CantBuild;
		}

		ObjectsByName(image, top, &byName);

		// Each object once: the first call that names it decides (its answer too)
		std::map<size_t, Image> made;          // the object's new chunk, by the object's place among the top chunks
		std::map<size_t, size_t> firstCall;    // the first call that named it
		std::vector<size_t> sameAs(count, count);

		for (size_t i = 0; i < count; i++)
		{
			std::string name;
			std::map<std::string, size_t>::const_iterator found;

			if (!NameOfObject(file->Level(), calls[i].id, &name) || (found = byName.find(name)) == byName.end())
			{
				continue;
			}

			size_t const place = found->second;
			std::map<size_t, size_t>::const_iterator seen = firstCall.find(place);

			if (seen != firstCall.end())
			{
				sameAs[i] = seen->second;
				continue;
			}

			firstCall[place] = i;

			Image out;

			if (!ChangedObject(image, top[place], what, calls[i].deleteModuleData, &out, &outcomes[i]))
			{
				return ObjectTypeResult::CantBuild;
			}

			if (outcomes[i] == ObjectTypeOutcome::Changed)
			{
				made[place] = out;
			}
		}

		for (size_t i = 0; i < count; i++)
		{
			if (sameAs[i] != count)
			{
				outcomes[i] = outcomes[sameAs[i]];
			}
		}

		if (made.empty())
		{
			return ObjectTypeResult::Done;   // nothing to change: the level stays as it is
		}

		// The new image: the top chunks as they were, but for the objects made again; the level's size made right
		Image out(image.begin(), image.begin() + (std::ptrdiff_t)kHeadingBytes);

		for (size_t i = 0; i < top.size(); i++)
		{
			std::map<size_t, Image>::const_iterator again = made.find(i);

			if (again != made.end())
			{
				out.insert(out.end(), again->second.begin(), again->second.end());
			}
			else
			{
				AddCopy(&out, image, top[i]);
			}
		}

		if (out.size() > kLargest)
		{
			return ObjectTypeResult::CantBuild;
		}

		PutIntAt(&out, 8, (int32_t)out.size());

		LevelOpenResult const result = file->ReplaceLevel(out);

		if (result == LevelOpenResult::Opened)
		{
			return ObjectTypeResult::Done;
		}

		// (outcomes[] means nothing when the answer isn't Done: nothing was changed)
		return result == LevelOpenResult::CantRead ? ObjectTypeResult::CantBuild : ObjectTypeResult::LoaderRefused;
	}
}
