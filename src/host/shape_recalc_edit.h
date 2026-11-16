// shape_recalc_edit.h: Construction > Miscellaneous > Recalculate Normals and Extents (command 0x25b), done over the open level through the edit store.
// Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT COPIES. inteng's FUN_004452b0 case 0x25b calls FUN_0044baa0, which goes over EVERY object of the level that has a shape and, for each shape,
// works out all its polygon normals and point normals again and the extents (AvP-docs\engine\inteng\p11-shape-handlers.md sections 1, 8 and 10; the maths is
// model::RecalculateNormalsAndExtents, shape_maths.h). Here each shape is read through ShapeEdit_Read, worked out, and put back with
// ShapeEdit_WriteSameCounts: the chunks keep their sizes, so no chunk is rebuilt. A shape that several objects use is done once (inteng does it once per
// object, and the second time changes nothing, because the numbers come from the points alone).
// WHAT IS NOT DONE, named: inteng's rebuild (FUN_0050f570) also works out the shape's centre and radius about it and makes a SHPCENTR chunk when there is
// none; which radius ends up in SHPHEAD1 is not read (shape_maths.h, ShapeRadiusAboutCentre), so the radius and any SHPCENTR chunk are left as they are.
// A shape the model refuses (hostile counts, a point difference past 2^30) is skipped and counted, and nothing of it is changed.
#pragma once

#include <stddef.h>

#include <string>

class File_Chunk;
class OpenLevelFile;

namespace host
{
	struct ShapeRecalcReport
	{
		size_t objectsWithShape;   // objects of the level that have a shape
		size_t shapesDone;         // distinct shapes worked out and put back
		size_t shapesChanged;      // of those, how many had a normal or an extent that differed from what was stored
		size_t shapesSkipped;      // distinct shapes left alone because the model or the store refused them
	};

	// False only for a call with no level or no store. *report is filled in either way.
	bool ShapeRecalc_All(OpenLevelFile *store, File_Chunk *level, ShapeRecalcReport *report);

	// The status line's words for a report, e.g. "Normals and extents recalculated: 811 shapes (12 changed)"
	std::string ShapeRecalc_Words(ShapeRecalcReport const &report);
}
