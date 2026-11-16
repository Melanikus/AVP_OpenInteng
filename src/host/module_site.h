// module_site.h: which object of the level file a module of the game is, by the module's name and its place (the game's module numbers and the file's object numbers are counted differently). Written 2026-11-15 by
// Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". (The same thing Frost's PolygonItems_ModuleObject does; kept apart here until the branches meet.)
#pragma once

#include <stdint.h>

class File_Chunk;

namespace host
{
	// The 8-byte ID of the module object of the level whose name is name (either case) and whose place, the file's number x scale, is within 2 of world; failing that the only module object of that name. False when there
	// is none or the arguments are not usable.
	bool ModuleObjectBySite(File_Chunk *level, char const *name, int32_t const world[3], double scale, uint8_t id[8]);
}
