// See level_objects.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "level_objects.h"

#include <string.h>

#include <windows.h>          // MAX_PATH

#include "chunk_object.hpp"   // Object_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile: the editor's own open of the level
#include "level_view.h"       // LevelView_FilePath
#include "strategy_lookup.h"   // ListVirtualObjects

// The editor's own open of the level, which the three lookups below read (LevelObjects_Use). Not owned here.
static OpenLevelFile const *theOpenLevel = nullptr;

// The same open, when the editor gave it to be changed (LevelObjects_Hold): for the edits that add or take away an object
static OpenLevelFile *theOpenLevelForEdit = nullptr;

// An object's ID is the game's ObjectID, two ints: the 8 bytes a pick compares
static_assert(sizeof(ObjectID) == host::kObjectIdBytes, "an ObjectID is the 8 bytes a pick in the view compares");

// The name the header record copies is the game's whole o_name, so the record's bound is the game's
static_assert(sizeof(ChunkObject::o_name) == host::kHeaderNameBytes, "an object's name is the 64 bytes the game keeps (OBJECT_NAME_BYTES)");

void LevelObjects_Use(OpenLevelFile const *open)
{
	theOpenLevel = open;
	theOpenLevelForEdit = nullptr;
}

OpenLevelFile *LevelObjects_OpenForEdit()
{
	return theOpenLevelForEdit;
}

LevelOpenResult LevelObjects_Hold(OpenLevelFile *open)
{
	if (!open)
	{
		LevelObjects_Use(nullptr);
		return LevelOpenResult::NoPath;
	}

	// The path the game loaded the level from, so the picture and the objects are of one file. When no level is open it's empty, which
	// Open refuses as NoPath, and lets go of any level it held before.
	char path[MAX_PATH] = "";

	LevelView_FilePath(path, sizeof(path));

	LevelOpenResult const result = open->Open(path);

	LevelObjects_Use(open);
	theOpenLevelForEdit = open;
	return result;
}

// The level the editor's open holds at this moment: null when none was given, or the open holds no level (it holds none after a refused
// open, and none after it let go). Asked at each call, so a level it has let go of is never read.
static File_Chunk *TheLevel()
{
	return theOpenLevel ? theOpenLevel->Level() : nullptr;
}

bool LevelObjects_Read(std::vector<host::LevelObject> *objects)
{
	objects->clear();

	File_Chunk *const level = TheLevel();

	if (!level)
	{
		return false;
	}

	List<Object_Chunk *> found;

	level->list_objects(&found);

	for (LIF<Object_Chunk *> each(&found); !each.done() && objects->size() <= host::kObjectListMost; each.next())
	{
		Object_Chunk *const object = each();

		if (!object)
		{
			continue;
		}

		host::LevelObject row;

		row.name = object->object_data.o_name;
		memcpy(row.id, &object->object_data.ID, host::kObjectIdBytes);
		objects->push_back(row);
	}

	// the level's virtual objects are rows too (they are objects in inteng's table: FUN_005396d0 puts one there), with their names and IDs
	std::vector<host::VirtualObjectRef> virtuals;

	host::ListVirtualObjects(level, &virtuals);

	static std::vector<std::string> names;   // the rows point at their names, kept until the next read

	names.clear();
	names.reserve(virtuals.size());

	for (host::VirtualObjectRef const &one : virtuals)
	{
		if (objects->size() > host::kObjectListMost)
		{
			break;
		}

		names.push_back(one.name);

		host::LevelObject row;

		row.name = names.back().c_str();
		memcpy(row.id, one.id, host::kObjectIdBytes);
		objects->push_back(row);
	}

	return true;
}

bool LevelObjects_FindHeader(const uint8_t *id, host::ObjectHeader *header)
{
	File_Chunk *const level = TheLevel();

	if (!id || !header || !level)
	{
		return false;
	}

	List<Object_Chunk *> found;

	level->list_objects(&found);

	for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || memcmp(&object->object_data.ID, id, host::kSelectionIdBytes) != 0)
		{
			continue;
		}

		// The first with this ID is the one: the list and a pick find it too
		Object_Header_Chunk *const head = object->get_header();

		if (!head)
		{
			return false;
		}

		// The name to its first 0, and never past the game's 64 bytes, as a level's own bytes are hostile
		const char *const name = object->object_data.o_name;
		const void *const zero = memchr(name, 0, sizeof(object->object_data.o_name));
		const size_t length = zero ? (size_t)((const char *)zero - name) : sizeof(object->object_data.o_name);

		header->name.assign(name, length);
		memcpy(header->id, &object->object_data.ID, host::kSelectionIdBytes);
		header->flags = (uint32_t)head->flags;
		return true;
	}

	// a virtual object has no object header: one is made for it, named and numbered as it is, with the placed bit (it is placed by its own place, not a module)
	{
		std::vector<host::VirtualObjectRef> virtuals;

		host::ListVirtualObjects(level, &virtuals);

		for (host::VirtualObjectRef const &one : virtuals)
		{
			if (memcmp(one.id, id, host::kSelectionIdBytes) == 0)
			{
				header->name = one.name;
				memcpy(header->id, one.id, host::kSelectionIdBytes);
				header->flags = 0x400;
				return true;
			}
		}
	}

	return false;
}

bool LevelObjects_ShapeGroup(const uint8_t *id, std::vector<host::LevelObject> *objects)
{
	if (objects)
	{
		objects->clear();
	}

	File_Chunk *const level = TheLevel();

	if (!id || !objects || !level)
	{
		return false;
	}

	List<Object_Chunk *> found;

	level->list_objects(&found);

	// The first object with this ID is the clicked one (the list and a pick find it too); its shape is FUN_004d0fd0's (none without a header)
	Object_Chunk *clicked = nullptr;

	for (LIF<Object_Chunk *> each(&found); !each.done() && !clicked; each.next())
	{
		if (each() && memcmp(&each()->object_data.ID, id, host::kObjectIdBytes) == 0)
		{
			clicked = each();
		}
	}

	if (!clicked)
	{
		return false;
	}

	Shape_Chunk *const shape = clicked->get_assoc_shape();

	for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || object->get_assoc_shape() != shape)
		{
			continue;
		}

		host::LevelObject row;

		row.name = object->object_data.o_name;
		memcpy(row.id, &object->object_data.ID, host::kObjectIdBytes);
		objects->push_back(row);
	}

	return true;
}

File_Chunk *LevelObjects_Level()
{
	return TheLevel();
}

bool LevelObjects_Places(std::vector<int32_t> *places)
{
	File_Chunk *const level = TheLevel();

	if (!places)
	{
		return false;
	}

	places->clear();

	if (!level)
	{
		return false;
	}

	List<Object_Chunk *> objects;

	level->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		places->push_back(each()->object_data.location.x);
		places->push_back(each()->object_data.location.y);
		places->push_back(each()->object_data.location.z);
	}

	return true;
}

bool LevelObjects_PlaceOf(const uint8_t *id, int32_t place[3])
{
	File_Chunk *const level = TheLevel();

	if (!level || !id || !place)
	{
		return false;
	}

	List<Object_Chunk *> objects;

	level->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		if (memcmp(&each()->object_data.ID, id, sizeof(each()->object_data.ID)) == 0)
		{
			place[0] = each()->object_data.location.x;
			place[1] = each()->object_data.location.y;
			place[2] = each()->object_data.location.z;
			return true;
		}
	}

	return false;
}
