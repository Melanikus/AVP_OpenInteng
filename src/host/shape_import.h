// shape_import.h: inteng's "load a shape that the level doesn't hold yet, from the shape folder" (the no-shape branch of FUN_004de260, the Quick-create
// maker's first step), as two plain functions: read the shape's rif file, and make the shape chunk the level takes in. Written 2026-11-15 by Melanikus
// (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (out\menu-reads-15.txt:2424-2735; the callees FUN_004bef40 (the rif load), FUN_0050e460 (the shape's header), FUN_00512f30 (SHPEXTFL),
// FUN_00514dc0 (SHPEXTFN), FUN_0046ca80 ("RifSearchPath") are NOT read): it finds <shape>.rif on the RifSearchPath, loads it, and makes the level's
// shape from the rif's one shape: the shape's header gets flag 0x1000, the shape gets a SHPEXTFL (the outside file's name, a rescale, the bitmap list).
//
// WHAT THIS DOES instead of reading those functions: the level the editor saves is compared with the shape files. The 21 rif files of the original
// tools' shape_rifs folder against the 192 outside shapes in the 63 stock levels that still have the rescale 1 (a shape rescaled afterwards is the
// rescale applied to the same numbers): the level's shape is the file's shape, byte for byte, in the points (SHPRAWVT), the vertex normals (SHPVNORM)
// and the centre (SHPCENTR) in all 192; in the polygon normals, polygons and texture coordinates in 162 (the other 30 are polygons the level's editor
// changed afterwards); and it holds, in this order, SHPEXTFL = SHPEXTFN + BMPLSTST + BMNAMEXT + BMNAMVER + RIFFNAME. PROOF (a throwaway, not in the
// repository): ShapeImport_Build from the file against the level's own shape, for those 192: 161 are equal in every part checked (SHPEXTFN with its version,
// the SHPEXTFL children in order and size, the name text, SHPCENTR / SHPRAWVT / SHPVNORM, the header's flags, counts and extents); the other 31 are the shapes the
// level's editor changed later (their polygon counts differ), one shape with another version number and one with an extra BMNAMEXT. Then the maker over the 63
// stock levels x the 22 Quick-create names with the folder: 1,324 objects made, the game's loader took every one, 62 asks (6spears, whose file isn't in the folder)
// still answered "shape not in this level". The parts:
//   - SHPEXTFN is: rescale 1.0 (a double), the number the file's own SHPHEAD1 holds at +60 (191 of 192 agree), the file's name (as typed, with ".rif");
//   - BMPLSTST is the file's REBENVDT's BMPNAMES, BMNAMEXT and BMNAMVER and RIFFNAME are the file's REBENVDT's, byte for byte, but for the pad bytes after
//     the last name (the level has what was in memory there);
//   - SHPHEAD1 has the flags 0x1000 and nothing else of the file's flags (all 1,136 outside shapes in the stock levels have exactly 0x1000).
// What is NOT copied, each a NAMED GAP: SHPFRAGS (the stock levels' fragment lists are the level's own, not the file's FRAGTYPE: checked: they differ),
// the lock user (empty), the header's number at +20 (here the level's highest shape number + 1; the stock numbers are the editor's counter), the header's
// version number at +60 (the file's own: the stock values are an edit count), a SHPEXTFN case with a different capital (the name is the Quick-create
// name + ".rif"), a file with more than one shape (refused), a file with its own SHPEXTFL (refused).
//
// EVERY OUTSIDE FILE IS HOSTILE (Tao 3). The name is plain letters, digits, '_', '-' (at most 63), so it can't name another folder; the file is at most
// kShapeImportMostBytes; every size is checked as the chunks are walked; the shape that comes out is read by the game's own loader before the level takes
// it (OpenLevelFile::ReplaceLevel), and nothing changes when it refuses. The folder is the editor's own setting ([Editor] ShapeFolder= in the .ini); with none, the folder "shape_rifs" beside the program (Bishop 15 Nov: a folder the owner fills; never a folder of untrusted tools).
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

namespace host
{

	// THE CHUNK HELPERS this file's reader uses, made public so the outside-shape update (shape_update.h) reads hostile bytes with the SAME hardened code (Frost, 2026-11-16:
	// one parser, not two that drift apart). Moved here unchanged by Melanikus (Vasquez), 2026-11-16.
	namespace shape_chunks
	{
		typedef std::vector<char> Bytes;

		size_t const kHeadingBytes = 12;          // a chunk: its 8-letter name, then its size (which counts these 12 bytes too)

		inline int32_t IntAt(Bytes const &bytes, size_t at)
		{
			int32_t value;

			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		inline void PutIntAt(Bytes *bytes, size_t at, int32_t value)
		{
			memcpy(bytes->data() + at, &value, sizeof(value));
		}

		inline void AddInt(Bytes *bytes, int32_t value)
		{
			char raw[sizeof(value)];

			memcpy(raw, &value, sizeof(value));
			bytes->insert(bytes->end(), raw, raw + sizeof(raw));
		}

		struct Span
		{
			size_t at;
			size_t size;
		};

		inline bool ChunkSizeAt(Bytes const &bytes, size_t at, size_t end, size_t *size)
		{
			if (end > bytes.size() || at > end || end - at < kHeadingBytes)
			{
				return false;
			}

			int32_t const stored = IntAt(bytes, at + 8);

			if (stored < (int32_t)kHeadingBytes || (size_t)stored > end - at)
			{
				return false;
			}

			*size = (size_t)stored;
			return true;
		}

		inline bool NameIs(Bytes const &bytes, size_t at, char const *name)
		{
			return memcmp(bytes.data() + at, name, 8) == 0;
		}

		inline bool Children(Bytes const &bytes, size_t from, size_t to, std::vector<Span> *out)
		{
			out->clear();

			for (size_t at = from; at < to; )
			{
				size_t size = 0;

				if (!ChunkSizeAt(bytes, at, to, &size))
				{
					return false;
				}

				Span const span = { at, size };

				out->push_back(span);
				at += size;
			}

			return true;
		}

		inline void PadToFour(Bytes *bytes)
		{
			while (bytes->size() % 4 != 0)
			{
				bytes->push_back(0);
			}
		}

		inline Bytes MakeChunk(char const *name, Bytes const &payload)
		{
			Bytes chunk(kHeadingBytes, 0);

			memcpy(chunk.data(), name, 8);
			chunk.insert(chunk.end(), payload.begin(), payload.end());
			PutIntAt(&chunk, 8, (int32_t)chunk.size());
			return chunk;
		}

		inline void AddCopy(Bytes *out, Bytes const &from, Span const &span)
		{
			out->insert(out->end(), from.begin() + (std::ptrdiff_t)span.at, from.begin() + (std::ptrdiff_t)(span.at + span.size));
		}

		inline size_t FirstNamed(Bytes const &bytes, std::vector<Span> const &inside, char const *name)
		{
			for (size_t i = 0; i < inside.size(); i++)
			{
				if (NameIs(bytes, inside[i].at, name))
				{
					return i;
				}
			}

			return inside.size();
		}
	}
	enum class ShapeImportResult
	{
		Done,
		BadName,        // not plain letters, digits, '_' or '-', empty, or longer than 63 letters
		NoFolder,       // the folder is not set
		NotFound,       // no <name>.rif in the folder (or it can't be opened)
		TooBig,         // the file is bigger than kShapeImportMostBytes, or empty
		BadFile,        // the file isn't a shape file this can copy (the words say what: see ShapeImportWords)
		NumberTaken,    // no shape number is left (more than 65,535)
	};

	// The most bytes of a shape file that are read (the stock shape files are under 40,000)
	size_t const kShapeImportMostBytes = 4 * 1024 * 1024;

	// In plain words for the editor's status line: static text, never null
	char const *ShapeImportWords(ShapeImportResult result);

	// Reads <folder>\<name>.rif into *bytes. *fileName is the name as it will be kept in the level (name + ".rif").
	ShapeImportResult ShapeImport_Read(char const *folder, std::string const &name, std::vector<char> *bytes, std::string *fileName);

	// Makes the level's shape chunk (a whole REBSHAPE, with its heading) from the bytes of a shape file, for the shape number newNumber. Its header lists
	// no object yet (the caller puts the new object's name there). *why, when not null, says what was wrong with the file.
	ShapeImportResult ShapeImport_Build(std::vector<char> const &rif, std::string const &fileName, int32_t newNumber, std::vector<char> *shapeChunk, std::string *why);
}
