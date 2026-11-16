// See strategy_lookup.h. Written 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "strategy_lookup.h"

#include <string.h>

#include <exception>
#include <vector>
#include <memory>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk, Object_Project_Data_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "strategy_chunk.hpp" // AVP_Strategy_Chunk

// An object's ID is the game's ObjectID, two ints: the 8 bytes a pick compares (selection.h)
static_assert(sizeof(ObjectID) == host::kSelectionIdBytes, "an ObjectID is the 8 bytes a pick in the view compares");

namespace host
{
	namespace
	{
		// The 12 bytes at the front of every chunk: its 8-letter name, then its size (chunk.hpp)
		size_t const ChunkHeadingBytes = 12;

		// The least a strategy's payload can be: the index and spare number (8 bytes), then the kind and two more numbers (12 bytes). The
		// game's own loader refuses a smaller one (strategy_chunk.cpp:52).
		size_t const LeastPayloadBytes = 20;

		bool IdIsAllZero(uint8_t const *id)
		{
			for (size_t i = 0; i < kSelectionIdBytes; i++)
			{
				if (id[i] != 0)
				{
					return false;
				}
			}

			return true;
		}

		// Copies a strategy chunk's payload into *out. The chunk is asked to write itself the way it would be saved (the strategies are kept
		// as the game's classes, not as bytes, so this is how their bytes are had), into a block of its own, and the heading is dropped.
		StrategyFound CopyPayload(AVP_Strategy_Chunk *strategy, ObjectStrategy *out)
		{
			// An editor-made chunk that hasn't been given its strategy yet holds none: asking it for its size would read through nothing
			if (!strategy->Strategy)
			{
				return StrategyFound::NoStrategyBytes;
			}

			size_t const whole = strategy->size_chunk();

			if (whole < ChunkHeadingBytes + LeastPayloadBytes)
			{
				return StrategyFound::NoStrategyBytes;
			}

			if (whole - ChunkHeadingBytes > kStrategyLookupMostBytes)
			{
				return StrategyFound::TooBig;
			}

			// The block is made with new[], so it goes back with delete[] (AVP_Strategy_Chunk::output_chunk does the same, strategy_chunk.cpp:221)
			std::unique_ptr<char[]> const block(strategy->make_data_block_from_chunk());

			if (!block)
			{
				return StrategyFound::NoStrategyBytes;
			}

			uint8_t const *const first = (uint8_t const *)block.get() + ChunkHeadingBytes;

			out->payload.assign(first, first + (whole - ChunkHeadingBytes));
			out->chunk = strategy;
			return StrategyFound::Found;
		}

		StrategyFound FindOnObject(File_Chunk *level, uint8_t const *id, ObjectStrategy *out)
		{
			if (!level)
			{
				return StrategyFound::NoLevel;
			}

			if (IdIsAllZero(id))
			{
				return StrategyFound::NoName;
			}

			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (!object || memcmp(&object->object_data.ID, id, kSelectionIdBytes) != 0)
				{
					continue;
				}

				// The first object with this ID is the one: if it has no strategy, that's the answer. A chunk the library kept as plain
				// bytes (too small for what its kind reads) holds no children and isn't looked into.
				Chunk *const data = object->lookup_single_child("OBJPRJDT");

				if (!data || data->r_u_miscellaneous())
				{
					return StrategyFound::NoStrategy;
				}

				Chunk *const found = ((Object_Project_Data_Chunk *)data)->lookup_single_child("AVPSTRAT");

				if (!found || found->r_u_miscellaneous())
				{
					return StrategyFound::NoStrategy;
				}

				return CopyPayload((AVP_Strategy_Chunk *)found, out);
			}

			return StrategyFound::NoSuchObject;
		}

		// An ID that no object has may be a virtual object's: its strategy is the AVPSTRAT in its VIOBJECT
		StrategyFound Find(File_Chunk *level, uint8_t const *id, ObjectStrategy *out)
		{
			StrategyFound const onObject = FindOnObject(level, id, out);

			if (onObject != StrategyFound::NoSuchObject)
			{
				return onObject;
			}

			std::vector<VirtualObjectRef> virtuals;

			ListVirtualObjects(level, &virtuals);

			for (VirtualObjectRef const &one : virtuals)
			{
				if (memcmp(one.id, id, kSelectionIdBytes) != 0)
				{
					continue;
				}

				Chunk_With_Children *const holder = (Chunk_With_Children *)one.viobject;
				Chunk *const found = holder->lookup_single_child("AVPSTRAT");

				if (!found || found->r_u_miscellaneous())
				{
					return StrategyFound::NoStrategy;
				}

				return CopyPayload((AVP_Strategy_Chunk *)found, out);
			}

			return StrategyFound::NoSuchObject;
		}
	}

	void ListVirtualObjects(File_Chunk *level, std::vector<VirtualObjectRef> *out)
	{
		out->clear();

		if (!level)
		{
			return;
		}

		try
		{
			Chunk *const env = level->lookup_single_child("REBENVDT");

			if (!env || env->r_u_miscellaneous())
			{
				return;
			}

			Chunk *const spec = ((Chunk_With_Children *)env)->lookup_single_child("SPECLOBJ");

			if (!spec || spec->r_u_miscellaneous())
			{
				return;
			}

			List<Chunk *> found;

			((Chunk_With_Children *)spec)->lookup_child("VIOBJECT", found);

			for (LIF<Chunk *> each(&found); !each.done() && out->size() < kMostVirtualObjects; each.next())
			{
				Chunk *const one = each();

				if (!one || one->r_u_miscellaneous())
				{
					continue;
				}

				Chunk *const prop = ((Chunk_With_Children *)one)->lookup_single_child("VOBJPROP");

				if (!prop || prop->r_u_miscellaneous())
				{
					continue;
				}

				Virtual_Object_Properties_Chunk const *const p = (Virtual_Object_Properties_Chunk *)prop;
				VirtualObjectRef ref;

				ref.name = p->name ? p->name : "";
				memcpy(ref.id, &p->ID, kSelectionIdBytes);
				ref.viobject = one;
				out->push_back(ref);
			}
		}
		catch (std::exception const &)
		{
			out->clear();
		}
	}

	char const *StrategyFoundWords(StrategyFound found)
	{
		switch (found)
		{
		case StrategyFound::Found: return "the object's strategy was found";
		case StrategyFound::BadCall: return "there was nothing to look for, or nowhere to put the answer";
		case StrategyFound::NoLevel: return "no level is open";
		case StrategyFound::NotAnObject: return "what's selected isn't an object";
		case StrategyFound::NoName: return "the object has no name to look for";
		case StrategyFound::NoSuchObject: return "the level has no object with that name";
		case StrategyFound::NoStrategy: return "the object has no strategy";
		case StrategyFound::NoStrategyBytes: return "the object's strategy is empty or too short to read";
		case StrategyFound::TooBig: return "the object's strategy is larger than any real one";
		case StrategyFound::NoMemory: return "there isn't memory enough to read the object's strategy";
		}

		return "the object's strategy couldn't be looked up";
	}

	StrategyFound FindObjectStrategy(File_Chunk *level, uint8_t const *id, ObjectStrategy *out)
	{
		if (!out)
		{
			return StrategyFound::BadCall;
		}

		out->chunk = 0;
		out->payload.clear();

		if (!id)
		{
			return StrategyFound::BadCall;
		}

		// Running out of memory refuses the answer, with nothing in *out, as any other mistake does
		try
		{
			StrategyFound const found = Find(level, id, out);

			if (found != StrategyFound::Found)
			{
				out->chunk = 0;
				out->payload.clear();
			}

			return found;
		}
		catch (std::exception const &)
		{
			out->chunk = 0;
			out->payload.clear();
			return StrategyFound::NoMemory;
		}
	}

	StrategyFound FindSelectedStrategy(File_Chunk *level, Selected const &selected, ObjectStrategy *out)
	{
		if (!out)
		{
			return StrategyFound::BadCall;
		}

		if (selected.kind != SelectedKind::Object)
		{
			out->chunk = 0;
			out->payload.clear();
			return StrategyFound::NotAnObject;
		}

		return FindObjectStrategy(level, selected.objectId, out);
	}
}
