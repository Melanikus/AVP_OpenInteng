// module_data_edit.h: Delete Module's Data (inteng command 0x50), the data side. P11, slice 10. Written 2026-11-15 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT INTENG DOES (out\menu-reads-1-full.txt:6125-6140, FUN_004452b0 0x445f5c-0x4460b6; p11-shape-handlers.md section 8 and
// wip-items.md): for the selected object it takes its MODULEDT chunk and DESTROYS every VMDARRAY child inside it (the module's adjacency
// array), then marks the level changed. Nothing else is touched: the MODULEDT chunk, the object and the shape stay.
//
// WHAT THIS DOES. The same, as one new image of the level through OpenLevelFile::ReplaceLevel (the chunk's size changes, which SetPayload can't
// do), so the game's own loader decides and nothing changes if it refuses. The object is found by its 8-byte ID. After a success every Chunk *
// the editor held is gone (object_edit.h says the same).
//
// NAMED POINT (not a deviation of the data effect): an object with no MODULEDT, or whose MODULEDT has no VMDARRAY, answers NothingToDelete and
// the level is left as it was.
#pragma once

#include <stddef.h>
#include <stdint.h>

class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

namespace host
{
	enum class ModuleDataResult
	{
		Done,
		BadCall,          // no level, or no ID
		NoObject,         // no object has that ID
		NothingToDelete,  // the object has no MODULEDT, or it holds no VMDARRAY
		CantBuild,        // the level couldn't be put together from its bytes
		LoaderRefused,    // the game's own loader wouldn't take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the editor's status line: static text, never null
	char const *ModuleDataWords(ModuleDataResult result);

	// Destroys every VMDARRAY inside the MODULEDT of the object whose 8-byte ID is id. *removed (when not null) is how many were taken out.
	ModuleDataResult ModuleData_DeleteLinks(OpenLevelFile *file, uint8_t const *id, size_t *removed);
}
