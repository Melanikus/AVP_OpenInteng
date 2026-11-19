// movable_virtual_object.cpp: movable.h's adapter for a Strategy VIRTUAL OBJECT (a VIOBJECT > VOBJPROP of SPECLOBJ): its place read with VirtualObject_List and written with
// the new VirtualObject_SetPlace (virtual_object.h), which changes only the VOBJPROP's three place numbers; its name, size, ID and strategy stay as they are. A virtual object
// holds no turn. The write goes through the game's loader, which checks the changed level; nothing is changed if it refuses. Written 2026-11-18 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHICH ONE: by the ref's ID (the VOBJPROP's 8 bytes: md5("VirtOb" + name), virtual_object.h). An all-zero ID names none.

#include "movable.h"

#include <string.h>

#include <vector>

#include "virtual_object.h"

namespace host
{
	namespace
	{
		bool NamesSomething(MovableRef const &ref)
		{
			for (size_t i = 0; i < kSelectionIdBytes; i++)
			{
				if (ref.id[i] != 0)
				{
					return true;
				}
			}

			return false;
		}
	}

	MovableResult MovableVirtualObject_Get(OpenLevelFile *file, MovableRef const &ref, MovablePose *pose)
	{
		if (!file)
		{
			return MovableResult::NothingOpen;
		}

		std::vector<VirtualObjectInfo> all;

		if (!NamesSomething(ref) || !VirtualObject_List(file, &all))
		{
			return MovableResult::NoSuchThing;
		}

		for (VirtualObjectInfo const &each : all)
		{
			if (memcmp(each.id, ref.id, sizeof(each.id)) == 0)
			{
				MovablePose read;

				memcpy(read.place, each.place, sizeof(read.place));
				*pose = read;
				return MovableResult::Done;
			}
		}

		return MovableResult::NoSuchThing;
	}

	MovableResult MovableVirtualObject_Set(OpenLevelFile *file, MovableRef const &ref, MovablePose const &pose)
	{
		if (!file)
		{
			return MovableResult::NothingOpen;
		}

		if (!NamesSomething(ref))
		{
			return MovableResult::NoSuchThing;
		}

		switch (VirtualObject_SetPlace(file, ref.id, pose.place))
		{
			case VirtualObjectResult::Done: return MovableResult::Done;
			case VirtualObjectResult::BadCall: return MovableResult::NothingOpen;
			case VirtualObjectResult::NotFound: case VirtualObjectResult::NoEnvironment: return MovableResult::NoSuchThing;
			default: return MovableResult::WriterRefused;
		}
	}
}
