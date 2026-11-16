// texture_anim.h: a shape's TEXTANIM chunk (the textures that change: which polygons, their sequences of frames) as numbers, put back into bytes, and the pure edits of the Texture Map mode's Anim menu on one polygon's record.
// Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE LAYOUT (avpx 3dc/win95/animation_chunk.cpp, which reads it and checks every size): a count of polygons, then for each polygon six numbers - the polygon's number in the shape's SHPPOLYS (the file's, before the game merges triangles into quads), its
// GROUP (the ID: polygons of one group animate together; inteng's "current animation" is a group), its count of sequences, its count of corners (3: the game only takes triangles), its flags (bit 0: not playing at the start) and its "anim number"
// (inteng's Set Anim Number) - then for each sequence five numbers (speed in 16.16 frames a second, flags, count of frames, two spares), the picture number of each frame, then six numbers for each frame: the u and v (whole texels) of its three corners.
// The picture number is the polygon's own picture number (the low 12 bits of its colour word) and the three (u, v) are its own list of texture coordinates, so a polygon shows its current frame and the chunk keeps all of them (inteng: FUN_0053fa00 puts a
// frame onto the polygon, FUN_0053fa70 takes the polygon's picture and coordinates into the frame).
//
// THE RULES (the chunk's bytes are hostile; Tao 1 and 3): the game's loader refuses a chunk that is not exactly its size; a polygon with a corner count other than 3 or with no sequence; a sequence with no frames, more than 32767 frames, or ONE frame while
// it has the flag 0x80000000 (no interpolation from the last to the first). The writer here only writes what that loader takes, and says no otherwise.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace model
{
	int32_t const kAnimNoInterpToFirst = (int32_t)0x80000000u;   // txa_flag_nointerptofirst
	int32_t const kAnimNoLoop = 4;                               // txa_flag_noloop
	int32_t const kAnimInterpolate = 8;                          // txa_flag_interpolate_uvs
	int32_t const kAnimNotPlaying = 1;                           // AnimFlag_NotPlaying (a polygon's flags)
	int32_t const kAnimDefaultSpeed = 65536;                     // FrameList's own: one frame a second
	int32_t const kMostAnimFrames = 32767;

	struct AnimSequence
	{
		int32_t speed;
		int32_t flags;
		int32_t spare1;
		int32_t spare2;
		std::vector<int32_t> textures;   // the picture number of each frame
		std::vector<int32_t> uv;         // six numbers for each frame: u0 v0 u1 v1 u2 v2

		AnimSequence() : speed(kAnimDefaultSpeed), flags(0), spare1(0), spare2(0) {}
		size_t Frames() const { return textures.size(); }
	};

	struct AnimPolygon
	{
		int32_t polygon;      // the number in SHPPOLYS
		int32_t group;        // the ID
		int32_t numVerts;     // 3
		int32_t flags;
		int32_t identifier;   // inteng's anim number
		std::vector<AnimSequence> sequences;

		AnimPolygon() : polygon(0), group(0), numVerts(3), flags(0), identifier(0) {}
	};

	struct TextureAnim
	{
		std::vector<AnimPolygon> polygons;
	};

	enum class TextureAnimResult
	{
		Ok,
		NotFit,        // a polygon with other than 3 corners or no sequence, or a sequence with no frames, too many, or one frame with the "no interpolation" flag; or a frame without its six numbers
		TooBig,        // more bytes than a chunk's size field holds
	};

	char const *TextureAnimWords(TextureAnimResult result);

	// The bytes of the chunk's payload (after its 12-byte heading) for these numbers, by the rules above
	TextureAnimResult WriteTextureAnim(TextureAnim const &anim, std::vector<unsigned char> *payload);

	// The largest group number plus one over several animations (0 when none)
	int32_t NextAnimGroup(TextureAnim const &anim, int32_t atLeast);

	// The pure edits of one polygon's record. `texture` and `uv` (six numbers) are what the polygon shows now.
	AnimSequence NewAnimSequence(int32_t texture, int32_t const uv[6]);                                  // one sequence of one frame (Create, Sequence > Add)
	void AnimAddFrame(AnimSequence *sequence, size_t after, int32_t texture, int32_t const uv[6]);       // a frame after `after` holding what the polygon shows (Frame > Add)
	void AnimRemoveFrame(AnimSequence *sequence, size_t at);                                             // Frame > Remove
}
