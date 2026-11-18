// object_number.h: a NEW object's OBJHEAD1 number and lock user, as inteng's save gives them (V-27, AvP-docs/engine/inteng/reassess/object-placement.md). Header-only (inline), so
// every maker of a new object (object_make.cpp's CreateObjectFromShape, object_edit.cpp's CreateObjectLike) keeps one rule with no new file to link. Written 2026-11-18 by
// Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (traced read-only, FUN_004e0ed0 and the save FUN_004e1f90): nothing is numbered when the object is made; at its first save the new RBOBJECT is made by the
// Object_Chunk maker FUN_004d0e80, which (its parent being "REBINFF2") takes FUN_004bebc0's number: the FIRST EMPTY slot of the file's object table (FUN_004be970 builds it with
// every object at its own number), or the table's size when it has no gap. DAT_00636f4c is the next SHAPE slot, not an object counter. The save's lock call FUN_004bd890 then
// sets the lock user DAT_005ce6f4, the static "Player" (0x5ce6f8), which code_004d1460 copies with a strncpy of 16 (so ten zeros follow).

#pragma once

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <vector>

namespace host
{
	// The lowest number no object has. A negative number holds no slot; a repeated one holds one. Never past 2^31-1 (the caller refuses past Object_Chunk::Max_Index_Num).
	inline int32_t ObjectNumber_LowestFree(std::vector<int32_t> numbers)
	{
		std::sort(numbers.begin(), numbers.end());

		int32_t free = 0;

		for (int32_t number : numbers)
		{
			if (number < free)
			{
				continue;   // a negative one, or a repeat
			}

			if (number > free || free == 0x7fffffff)
			{
				break;      // the first gap (or no number is left)
			}

			free++;
		}

		return free;
	}

	// The 16 bytes of the lock user: "Player" and ten zeros
	inline void ObjectNumber_LockUser(char lock[16])
	{
		memset(lock, 0, 16);
		memcpy(lock, "Player", 6);
	}
}
