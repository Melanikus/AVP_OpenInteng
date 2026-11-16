// asc_text.h: the text of inteng's Write Asc File, line by line, from the numbers of asc_export.h and asc_level.h. Written 2026-11-15 by Melanikus
// (Drake) for AVP_OpenInteng (P7). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE FORMATS are inteng's own, read from its strings (AvP-docs\engine\inteng\raw\inteng-strings.txt, lines 2825-2837) and tied to each sprintf call
// by Vasquez's raw listings (p11-shape-handlers.md sections 32-34, out\raw-listing-14/15/16.txt): the file is the header (0x5ec93c, sprintf at
// 0x507068); then for each object (FUN_00506b00) `Named object: "%s"` (0x5ec82c, pushed at 0x506b24: the object's name, +0x64), `Tri-mesh, Vertices: %d
// Faces: %d` (0x5ec804), `Vertex list:` (0x5ec7f4), a `Vertex %d:  X: %f     Y: %f     Z: %f` line for each point (0x5ec7cc), `Face list:` (0x5ec7bc),
// a `Face %d:    A:%d B:%d C:%d AB:%d BC:%d CA:%d` line for each polygon (0x5ec78c) and the text its sprintf at 0x506eb7 pushes, a line break (see
// kAscAfterFaces); and last the "hudson" marker (0x5ec8f0, four vertex lines in the same format, then 0x5ec844). Every line ends in \r\n. "%f" is the
// C library's, with six decimals and a point, whatever the machine's language (the text is made with the "C" locale).
#pragma once

#include <stddef.h>

#include <string>
#include <vector>

#include "asc_level.h"

namespace host
{
	// The scale the vertices are written in: _DAT_005ae9d0, the double 0x3f6505d4bc6b91da (about 0.002566257, bytes da 91 6b bc d4 05 65 3f)
	double AscScale();

	// What inteng's sprintf at 0x506eb7 writes after an object's last face line: a line break, bytes 0d 0a (read in run 27).
	extern char const kAscAfterFaces[];

	// The whole file's text for the objects (asc_level.h, with offset) at the scale. The hudson marker is last.
	std::string AscFileText(std::vector<AscLevelObject> const &objects, AscOffset const &offset, double scale);
}
