// quick_create.h: the shape each of Object Placement's Quick-create items asks FUN_004de260 for. Written 2026-11-15 by Melanikus (Frost) for
// AVP_OpenInteng (P3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Each item is one case of FUN_004d9af0 that calls FUN_004de260 with a shape's name (out\menu-reads-1-full.txt:2224-2340; the names are those of
// out\map-1-strings.txt:4078-4262 and engine\inteng\wip-answers-quickcreate.md). object_make.h says what happens with the name.
#pragma once

#include <stdint.h>

namespace host
{
	// The shape's name for a Quick-create command (inteng's own command number), or null when it isn't one. Static text.
	char const *QuickCreate_ShapeName(int32_t command);
}
