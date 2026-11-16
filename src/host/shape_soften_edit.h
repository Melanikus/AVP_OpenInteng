// shape_soften_edit.h: Light Sourcing's Edge > Soft (command 0x50) on the open level: the selected object's shape gets its vertex normals made again from a typed angle (model::SoftenShape, shape_soften.h). Written
// 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (FUN_004af810): it works only on a shape whose header flag 0x1000 is clear (a shape with it set is left alone and nothing is asked); it asks "Type in the maximum angle for soft edges (flat is 0)."
// (FUN_004b0270, an OK / Cancel dialog), reads a whole number from the text (a text of 256 letters or more is refused) and takes it when it is 0 to 180; the vertex normals are made again, the shape's chunks are made again
// (FUN_0050f570), the level is WRITTEN (FUN_004bd8e0, FUN_004719e0: the same write as Save), and the shape's light is worked out again (FUN_004ff010).
//
// WHAT THIS DOES: the same to the level's bytes through the edit store (the vertex normals of the shape, the same size), and the view's light again (the editor's own: editor_main.cpp). NAMED DEVIATIONS: the level is not
// written to a file (Save a copy does that); inteng's dialog is replaced by the editor's own prompt (its words are inteng's); a shape with no vertex normals chunk is refused (inteng's rebuild would make one).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

class File_Chunk;
class OpenLevelFile;

namespace host
{
	enum class ShapeSoftenResult
	{
		Done,
		BadCall,        // no level, no ID, or no place for an answer
		BadAngle,       // not a whole number from 0 to 180
		NoObject,       // no object has that ID
		NoShape,        // the object has no shape the editor can read
		LeftAlone,      // the shape's header flag 0x1000 is set: inteng does nothing
		NoNormals,      // the shape has no vertex normals to give
		BadShape,       // the shape could not be read
		Refused,        // the bytes wouldn't take the change
	};

	char const *ShapeSoftenWords(ShapeSoftenResult result);

	// The angle in a typed text, as inteng reads it (a whole number, sscanf %d), or false: the text has to be shorter than 256 letters and the number 0 to 180.
	bool ShapeSoften_ReadAngle(std::string const &typed, int *angle);

	// Makes the vertex normals of the shape of the object with this 8-byte ID again for that angle. *changed (when not null) gets how many points have another normal; *why the model's words on BadShape / Refused.
	ShapeSoftenResult ShapeSoften_ByAngle(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, int angle, double levelScale, size_t *changed, std::string *why);
}
