// See module_site.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "module_site.h"

#include <math.h>
#include <string.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"

namespace host
{
	bool ModuleObjectBySite(File_Chunk *level, char const *name, int32_t const world[3], double scale, uint8_t id[8])
	{
		if (!level || !name || !name[0] || !world || !id || !(scale > 0.0) || scale > 1.0e9)
		{
			return false;
		}

		List<Object_Chunk *> found;
		Object_Chunk *byName = nullptr;
		Object_Chunk *byPlace = nullptr;
		int sameName = 0;

		level->list_objects(&found);

		for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (!object || !object->get_header() || (object->get_header()->flags & OBJECT_FLAG_PLACED_OBJECT) || _stricmp(object->object_data.o_name, name) != 0)
			{
				continue;
			}

			sameName++;
			byName = byName ? byName : object;

			double const at[3] = { (double)object->object_data.location.x * scale, (double)object->object_data.location.y * scale, (double)object->object_data.location.z * scale };

			if (!byPlace && fabs(at[0] - world[0]) <= 2.0 && fabs(at[1] - world[1]) <= 2.0 && fabs(at[2] - world[2]) <= 2.0)
			{
				byPlace = object;
			}
		}

		Object_Chunk *const chosen = byPlace ? byPlace : (sameName == 1 ? byName : nullptr);

		if (!chosen)
		{
			return false;
		}

		memcpy(id, &chosen->object_data.ID, 8);
		return true;
	}
}
