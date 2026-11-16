// anim_edit.h: the TEXTANIM chunk of a shape, read from the open level and put back (the Texture Map mode's Anim menu). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW (as shape_merge_edit.h and object_edit.h do it): the chunk's size changes with every sequence and frame, which SetPayload can't do, so the level is written out as the next save would write it (BuildLevel), the TEXTANIM chunk of the shape is made again
// (or put in, or cut out when no polygon is animated any more), the sizes around it are made again, and OpenLevelFile::ReplaceLevel reads the result with the game's own loader and swaps it in only if the loader takes it. After a success every Chunk * the editor held is gone.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "texture_anim.h"

class OpenLevelFile;

namespace host
{
	enum class AnimEditResult
	{
		Done,
		BadCall,
		NoObject,        // the ID names no object
		NoShape,         // the object has no shape the editor can read
		NotFit,          // the numbers would not go into a chunk the loader takes
		CantBuild,       // the level couldn't be put together from its bytes
		LoaderRefused,   // the game's own loader wouldn't take the changed level, so nothing was changed
	};

	// The shape of the object whose 8-byte ID is `object`: its number, and its animation (no polygons when it has no TEXTANIM chunk)
	AnimEditResult AnimEdit_Read(OpenLevelFile *file, uint8_t const object[8], int32_t *shapeNumber, model::TextureAnim *anim);

	struct AnimWrite
	{
		int32_t shapeNumber;
		model::TextureAnim anim;
	};

	// Makes the animation of each of those shapes (by its number) what is given, all in ONE new image of the level
	AnimEditResult AnimEdit_Write(OpenLevelFile *file, std::vector<AnimWrite> const &writes, std::string *why);
}
