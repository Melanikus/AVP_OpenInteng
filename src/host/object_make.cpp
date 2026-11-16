// See object_make.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng (P3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "object_make.h"

#include <math.h>
#include <string.h>

#include <exception>
#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk::Max_Index_Num
#include "mishchnk.hpp"       // File_Chunk::list_objects

#include "level_file.h"
#include "shape_import.h"

namespace host
{
	namespace
	{
		typedef std::vector<char> Bytes;

		size_t const HeadingBytes = 12;          // a chunk: its 8-letter name, then its size (which counts these 12 bytes too)
		size_t const NameAt = 60;                // in an object header's payload: the name starts here, after 60 bytes of numbers
		size_t const IndexAt = 48;
		size_t const MostNameBytes = 63;         // OBJECT_NAME_BYTES (64) with its ending zero
		int32_t const MostPlace = 1 << 24;       // CheckPlace (chunk.hpp): a place farther than 2^24 millimetres from the middle is refused

		// OBJHEAD1's flags for a new object (FUN_004e1f90, menu-reads-3.txt:495-562): placed (0x400), platform load set (0x4000), and the bits of
		// obj[0x13] = 0xb800: marine (0x800), alien (0x1000), predator (0x2000) and PC load (0x8000)
		uint32_t const NewObjectFlags = 0x400 | 0x4000 | 0xb800;

		int32_t IntAt(Bytes const &bytes, size_t at)
		{
			int32_t value;
			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		void PutIntAt(Bytes &bytes, size_t at, int32_t value)
		{
			memcpy(bytes.data() + at, &value, sizeof(value));
		}

		void AddInt(Bytes &bytes, int32_t value)
		{
			char raw[sizeof(value)];
			memcpy(raw, &value, sizeof(value));
			bytes.insert(bytes.end(), raw, raw + sizeof(value));
		}

		void AddFloat(Bytes &bytes, float value)
		{
			char raw[sizeof(value)];
			memcpy(raw, &value, sizeof(value));
			bytes.insert(bytes.end(), raw, raw + sizeof(value));
		}

		// The size of the chunk at `at`, inside [at, end), or false when its heading isn't there or its size isn't a size that fits
		bool ChunkSizeAt(Bytes const &bytes, size_t at, size_t end, size_t *size)
		{
			if (end > bytes.size() || at > end || end - at < HeadingBytes)
			{
				return false;
			}

			int32_t const stored = IntAt(bytes, at + 8);

			if (stored < (int32_t)HeadingBytes || (size_t)stored > end - at)
			{
				return false;
			}

			*size = (size_t)stored;
			return true;
		}

		bool NameIs(Bytes const &bytes, size_t at, char const *name)
		{
			return memcmp(bytes.data() + at, name, 8) == 0;
		}

		// A chunk's heading and what it holds, as bytes
		Bytes MakeChunk(char const *name, Bytes const &payload)
		{
			Bytes chunk(HeadingBytes, 0);

			memcpy(chunk.data(), name, 8);
			chunk.insert(chunk.end(), payload.begin(), payload.end());
			PutIntAt(chunk, 8, (int32_t)chunk.size());
			return chunk;
		}

		void PadToFour(Bytes &bytes)
		{
			while (bytes.size() % 4 != 0)
			{
				bytes.push_back(0);
			}
		}

		// Letters of either case are one
		bool SameName(char const *a, size_t aLength, std::string const &b)
		{
			if (aLength != b.size())
			{
				return false;
			}

			for (size_t i = 0; i < aLength; i++)
			{
				char x = a[i];
				char y = b[i];

				x = (x >= 'A' && x <= 'Z') ? (char)(x + 32) : x;
				y = (y >= 'A' && y <= 'Z') ? (char)(y + 32) : y;

				if (x != y)
				{
					return false;
				}
			}

			return true;
		}

		bool NameIsPlainText(std::string const &name)
		{
			if (name.empty() || name.size() > MostNameBytes)
			{
				return false;
			}

			for (size_t i = 0; i < name.size(); i++)
			{
				unsigned char const letter = (unsigned char)name[i];

				if (letter < 32 || letter > 126)
				{
					return false;
				}
			}

			return true;
		}

		struct ShapeInfo
		{
			size_t at;              // the REBSHAPE chunk
			size_t size;
			size_t headAt;          // its SHPHEAD1
			size_t headSize;
			int32_t fileId;         // the shape's number (the object's shape_id_no)
			std::string outsideName;   // SHPEXTFN's file name without its extension, "" when the shape has none
		};

		struct LevelInfo
		{
			std::vector<std::string> objectNames;
			int32_t nextIndex;
			size_t lastObjectEnd;   // where the level's last top-level RBOBJECT ends (the new object goes there); 0 when there is none
			std::vector<ShapeInfo> shapes;
		};

		// The file name of a SHPEXTFL's SHPEXTFN, without its extension: the chunk holds a double, a number, then the name (win95/chunk_shape.cpp)
		bool OutsideName(Bytes const &bytes, size_t extAt, size_t extSize, std::string *name)
		{
			for (size_t inner = extAt + HeadingBytes; inner < extAt + extSize; )
			{
				size_t size = 0;

				if (!ChunkSizeAt(bytes, inner, extAt + extSize, &size))
				{
					return false;
				}

				if (NameIs(bytes, inner, "SHPEXTFN") && size > HeadingBytes + 12)
				{
					char const *const first = bytes.data() + inner + HeadingBytes + 12;
					size_t const room = size - HeadingBytes - 12;
					char const *const zero = (char const *)memchr(first, 0, room);

					if (!zero)
					{
						return false;
					}

					std::string file(first, (size_t)(zero - first));
					size_t const slash = file.find_last_of("\\/");

					if (slash != std::string::npos)
					{
						file = file.substr(slash + 1);
					}

					size_t const dot = file.find_last_of('.');

					*name = dot == std::string::npos ? file : file.substr(0, dot);
					return true;
				}

				inner += size;
			}

			return true;   // no SHPEXTFN: no outside name
		}

		bool ReadLevel(Bytes const &bytes, LevelInfo *info)
		{
			info->objectNames.clear();
			info->nextIndex = 0;
			info->lastObjectEnd = 0;
			info->shapes.clear();

			if (bytes.size() < HeadingBytes || !NameIs(bytes, 0, "REBINFF2"))
			{
				return false;
			}

			for (size_t at = HeadingBytes; at < bytes.size(); )
			{
				size_t size = 0;

				if (!ChunkSizeAt(bytes, at, bytes.size(), &size))
				{
					return false;
				}

				if (NameIs(bytes, at, "RBOBJECT"))
				{
					info->lastObjectEnd = at + size;

					for (size_t inner = at + HeadingBytes; inner < at + size; )
					{
						size_t innerSize = 0;

						if (!ChunkSizeAt(bytes, inner, at + size, &innerSize))
						{
							return false;
						}

						if (NameIs(bytes, inner, "OBJHEAD1") && innerSize > HeadingBytes + NameAt)
						{
							char const *const first = bytes.data() + inner + HeadingBytes + NameAt;
							size_t const room = innerSize - HeadingBytes - NameAt;
							char const *const zero = (char const *)memchr(first, 0, room);

							if (!zero || (size_t)(zero - first) > MostNameBytes)
							{
								return false;
							}

							info->objectNames.push_back(std::string(first, (size_t)(zero - first)));

							int32_t const index = IntAt(bytes, inner + HeadingBytes + IndexAt);

							if (index >= info->nextIndex && index < 0x7fffffff)
							{
								info->nextIndex = index + 1;
							}
						}

						inner += innerSize;
					}
				}
				else if (NameIs(bytes, at, "REBSHAPE"))
				{
					ShapeInfo shape;

					shape.at = at;
					shape.size = size;
					shape.headAt = 0;
					shape.headSize = 0;
					shape.fileId = 0;

					bool hasHead = false;

					for (size_t inner = at + HeadingBytes; inner < at + size; )
					{
						size_t innerSize = 0;

						if (!ChunkSizeAt(bytes, inner, at + size, &innerSize))
						{
							return false;
						}

						if (NameIs(bytes, inner, "SHPHEAD1") && innerSize >= HeadingBytes + 68 && !hasHead)
						{
							hasHead = true;
							shape.headAt = inner;
							shape.headSize = innerSize;
							shape.fileId = IntAt(bytes, inner + HeadingBytes + 20);
						}
						else if (NameIs(bytes, inner, "SHPEXTFL"))
						{
							if (!OutsideName(bytes, inner, innerSize, &shape.outsideName))
							{
								return false;
							}
						}

						inner += innerSize;
					}

					if (hasHead)
					{
						info->shapes.push_back(shape);
					}
				}

				at += size;
			}

			return true;
		}

		ObjectMakeResult FromLoader(LevelOpenResult result)
		{
			switch (result)
			{
			case LevelOpenResult::Opened: return ObjectMakeResult::Done;
			case LevelOpenResult::NothingOpen: return ObjectMakeResult::NothingOpen;
			case LevelOpenResult::CantRead: return ObjectMakeResult::CantBuild;
			default: return ObjectMakeResult::LoaderRefused;
			}
		}

		// The ID of the first object of the library's level with that name, found after the level was replaced
		bool IdOfName(OpenLevelFile *file, std::string const &name, uint8_t *id)
		{
			File_Chunk *const level = file->Level();

			if (!level)
			{
				return false;
			}

			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && strncmp(object->object_data.o_name, name.c_str(), sizeof(object->object_data.o_name)) == 0)
				{
					memcpy(id, &object->object_data.ID, 8);
					return true;
				}
			}

			return false;
		}

		ObjectMakeResult MakeIn(OpenLevelFile *file, char const *shapeName, int32_t const *place, int32_t const *matrix, uint8_t *newId, std::string *newName,
		                        char const *shapeFolder, std::string *importWhy, Bytes const *givenShape = nullptr, float const *givenTurn = nullptr, bool exactName = false)
		{
			if (importWhy)
			{
				importWhy->clear();
			}

			if (!file || !file->Level())
			{
				return ObjectMakeResult::NothingOpen;
			}

			std::string const base = shapeName ? shapeName : "";

			if (!NameIsPlainText(base))
			{
				return ObjectMakeResult::BadShapeName;
			}

			if (base.find('@') != std::string::npos)
			{
				return ObjectMakeResult::SubNameNotCopied;
			}

			if (!place || (!matrix && !givenTurn))
			{
				return ObjectMakeResult::CantBuild;
			}

			for (int axis = 0; axis < 3; axis++)
			{
				if (place[axis] > MostPlace || place[axis] < -MostPlace)
				{
					return ObjectMakeResult::PlaceOutOfRange;
				}
			}

			Bytes image;
			LevelInfo info;

			if (!file->BuildLevel(image) || !ReadLevel(image, &info))
			{
				return ObjectMakeResult::CantBuild;
			}

			// 1. the shape, by name (an outside file's name without its extension, capitals not mattering)
			size_t found = info.shapes.size();

			if (givenShape)
			{
				// a whole REBSHAPE chunk the caller made (Split Object's new shape): it goes in after the level's last object, then it is the shape found, by its number
				int32_t givenId = -1;

				if (info.lastObjectEnd == 0 || givenShape->size() < HeadingBytes + HeadingBytes + 24 || !NameIs(*givenShape, 0, "REBSHAPE"))
				{
					return ObjectMakeResult::CantBuild;
				}

				for (size_t at = HeadingBytes; at < givenShape->size() && givenId < 0; )
				{
					size_t size = 0;

					if (!ChunkSizeAt(*givenShape, at, givenShape->size(), &size))
					{
						return ObjectMakeResult::CantBuild;
					}

					if (NameIs(*givenShape, at, "SHPHEAD1") && size >= HeadingBytes + 24)
					{
						givenId = IntAt(*givenShape, at + HeadingBytes + 20);
					}

					at += size;
				}

				if (givenId < 0)
				{
					return ObjectMakeResult::CantBuild;
				}

				image.insert(image.begin() + (std::ptrdiff_t)info.lastObjectEnd, givenShape->begin(), givenShape->end());

				if (image.size() > 0x7fffffff || !ReadLevel(image, &info))
				{
					return ObjectMakeResult::CantBuild;
				}

				found = info.shapes.size();   // the list was read again: its size is not what it was

				for (size_t i = 0; i < info.shapes.size() && found == info.shapes.size(); i++)
				{
					if (info.shapes[i].fileId == givenId && info.shapes[i].at < info.lastObjectEnd + givenShape->size() && info.shapes[i].size == givenShape->size())
					{
						found = i;
					}
				}

				if (found == info.shapes.size())
				{
					return ObjectMakeResult::CantBuild;
				}
			}

			for (size_t i = 0; !givenShape && i < info.shapes.size() && found == info.shapes.size(); i++)
			{
				if (!info.shapes[i].outsideName.empty() && SameName(info.shapes[i].outsideName.c_str(), info.shapes[i].outsideName.size(), base))
				{
					found = i;
				}
			}

			if (found == info.shapes.size())
			{
				// inteng's no-shape branch: the shape is loaded from the shape folder (shape_import.h: what the level holds for it is the stock levels' own)
				if (!shapeFolder || !*shapeFolder)
				{
					return ObjectMakeResult::NoSuchShape;
				}

				std::vector<char> rif;
				std::string fileName;
				ShapeImportResult step = ShapeImport_Read(shapeFolder, base, &rif, &fileName);

				if (step == ShapeImportResult::NotFound)
				{
					if (importWhy)
					{
						*importWhy = "there is no " + base + ".rif in the shape folder";
					}

					return ObjectMakeResult::NoSuchShape;
				}

				int32_t highest = -1;

				for (ShapeInfo const &each : info.shapes)
				{
					highest = each.fileId > highest ? each.fileId : highest;
				}

				std::vector<char> chunk;
				std::string why;

				if (step == ShapeImportResult::Done)
				{
					step = ShapeImport_Build(rif, fileName, highest + 1, &chunk, &why);
				}

				if (step != ShapeImportResult::Done)
				{
					if (importWhy)
					{
						*importWhy = why.empty() ? ShapeImportWords(step) : why;
					}

					return ObjectMakeResult::ShapeFileRefused;
				}

				if (info.lastObjectEnd == 0)
				{
					return ObjectMakeResult::CantBuild;
				}

				image.insert(image.begin() + (std::ptrdiff_t)info.lastObjectEnd, chunk.begin(), chunk.end());

				if (image.size() > 0x7fffffff || !ReadLevel(image, &info))
				{
					return ObjectMakeResult::CantBuild;
				}

				for (size_t i = 0; i < info.shapes.size() && found == info.shapes.size(); i++)
				{
					if (!info.shapes[i].outsideName.empty() && SameName(info.shapes[i].outsideName.c_str(), info.shapes[i].outsideName.size(), base))
					{
						found = i;
					}
				}

				if (found == info.shapes.size())
				{
					return ObjectMakeResult::CantBuild;
				}
			}

			ShapeInfo const shape = info.shapes[found];

			// 2. the name: FUN_004d5620, "%s%d" with a counter from 0, the first no object has
			std::string made;

			for (int n = 0; n < 1000000 && made.empty(); n++)
			{
				std::string const candidate = exactName ? base : base + std::to_string(n);

				if (candidate.size() > MostNameBytes)
				{
					break;
				}

				bool taken = false;

				for (size_t i = 0; i < info.objectNames.size() && !taken; i++)
				{
					taken = SameName(info.objectNames[i].c_str(), info.objectNames[i].size(), candidate);
				}

				if (!taken)
				{
					made = candidate;
				}
				else if (exactName)
				{
					break;   // a name the caller chose is taken: no counter is tried
				}
			}

			if (made.empty())
			{
				return ObjectMakeResult::NoNameFree;
			}

			if (info.nextIndex < 0 || info.nextIndex > Object_Chunk::Max_Index_Num)
			{
				return ObjectMakeResult::TooManyObjects;
			}

			// 3. the object's chunks, in the library's creation order: the header, the interface data (the notes), the project data (the strategy)
			float turn[4];

			if (givenTurn)
			{
				memcpy(turn, givenTurn, sizeof(turn));   // Split Object: the new object turned as the old one is
			}
			else
			{
				ObjectMake_TurnFromMatrix(matrix, turn);
			}

			Bytes header;

			AddInt(header, (int32_t)NewObjectFlags);
			header.insert(header.end(), 16, 0);                     // the lock user: nobody (named gap)
			for (int axis = 0; axis < 3; axis++)
			{
				AddInt(header, place[axis]);
			}
			for (int part = 0; part < 4; part++)
			{
				AddFloat(header, turn[part]);
			}
			AddInt(header, info.nextIndex);                         // the number (named gap)
			AddInt(header, 1);                                      // version_no: 0 at birth, +1 when written (Object_Header_Chunk::prepare_for_output)
			AddInt(header, shape.fileId);                           // the shape's number
			header.insert(header.end(), made.begin(), made.end());
			header.push_back(0);
			PadToFour(header);

			Bytes notesText;
			{
				char const text[] = "Enter notes here";

				notesText.insert(notesText.end(), text, text + sizeof(text));   // with its ending zero
				PadToFour(notesText);
			}

			Bytes interfaceData = MakeChunk("OBJNOTES", notesText);

			Bytes strategy;                                         // AVPSTRAT: 52 bytes, kind 62 (FUN_0052b120(0x3e), :597-611)
			AddInt(strategy, 0);                                    // index
			AddInt(strategy, 0);                                    // spare
			AddInt(strategy, 62);                                   // the kind
			AddInt(strategy, 0);
			AddInt(strategy, 0);
			AddInt(strategy, 0);                                    // Type: Static
			AddInt(strategy, 0);                                    // ExtraData
			AddInt(strategy, (int32_t)(0x20032 & 0xffff));          // mass 50 (obj[0x14] & 0xffff)
			AddInt(strategy, (int32_t)((0x20032 >> 16) | 0x80000000u));   // integrity 2 | 0x80000000
			AddInt(strategy, 1);                                    // flags
			AddInt(strategy, 0);                                    // target_request
			AddInt(strategy, 0);                                    // targetID, two numbers
			AddInt(strategy, 0);

			Bytes object(HeadingBytes, 0);
			Bytes const headChunk = MakeChunk("OBJHEAD1", header);
			Bytes const interfaceChunk = MakeChunk("OBINTDT", interfaceData);
			Bytes const projectChunk = MakeChunk("OBJPRJDT", MakeChunk("AVPSTRAT", strategy));

			memcpy(object.data(), "RBOBJECT", 8);
			object.insert(object.end(), headChunk.begin(), headChunk.end());
			object.insert(object.end(), interfaceChunk.begin(), interfaceChunk.end());
			object.insert(object.end(), projectChunk.begin(), projectChunk.end());
			PutIntAt(object, 8, (int32_t)object.size());

			// 4. the shape's header lists the names of the objects that use it: the new name goes last
			Bytes newHead(image.begin() + (std::ptrdiff_t)(shape.headAt + HeadingBytes), image.begin() + (std::ptrdiff_t)(shape.headAt + shape.headSize));
			int32_t const users = IntAt(image, shape.headAt + HeadingBytes + 64);

			if (users < 0)
			{
				return ObjectMakeResult::CantBuild;
			}

			// the names after the count are walked, so a count that the bytes can't hold is refused and the padding after them is dropped
			size_t at = 68;

			for (int32_t i = 0; i < users; i++)
			{
				if (at >= newHead.size())
				{
					return ObjectMakeResult::CantBuild;
				}

				char const *const zero = (char const *)memchr(newHead.data() + at, 0, newHead.size() - at);

				if (!zero)
				{
					return ObjectMakeResult::CantBuild;
				}

				at += (size_t)(zero - (newHead.data() + at)) + 1;
			}

			newHead.resize(at);
			PutIntAt(newHead, 64, users + 1);
			newHead.insert(newHead.end(), made.begin(), made.end());
			newHead.push_back(0);
			PadToFour(newHead);

			Bytes const newHeadChunk = MakeChunk("SHPHEAD1", newHead);

			if (info.lastObjectEnd == 0)
			{
				return ObjectMakeResult::CantBuild;   // a level with no object has no place to take one in after
			}

			// apply the later change first, so the earlier offset still holds
			int64_t const shapeGrows = (int64_t)newHeadChunk.size() - (int64_t)shape.headSize;

			auto putObject = [&]() { image.insert(image.begin() + (std::ptrdiff_t)info.lastObjectEnd, object.begin(), object.end()); };
			auto putShape = [&]()
			{
				image.erase(image.begin() + (std::ptrdiff_t)shape.headAt, image.begin() + (std::ptrdiff_t)(shape.headAt + shape.headSize));
				image.insert(image.begin() + (std::ptrdiff_t)shape.headAt, newHeadChunk.begin(), newHeadChunk.end());
				PutIntAt(image, shape.at + 8, (int32_t)((int64_t)shape.size + shapeGrows));
			};

			// (a shape that starts where the last object ends, as a shape just imported does, lies after the object's place too)
			if (shape.at >= info.lastObjectEnd)
			{
				putShape();
				putObject();
			}
			else
			{
				putObject();

				// the shape lies before the object's place: its offsets haven't moved
				putShape();
			}

			if (image.size() > 0x7fffffff)
			{
				return ObjectMakeResult::TooManyObjects;
			}

			PutIntAt(image, 8, (int32_t)image.size());

			ObjectMakeResult const done = FromLoader(file->ReplaceLevel(image));

			if (done == ObjectMakeResult::Done)
			{
				if (newName)
				{
					*newName = made;
				}

				if (newId)
				{
					memset(newId, 0, 8);
					IdOfName(file, made, newId);
				}
			}

			return done;
		}
	}

	char const *ObjectMakeWords(ObjectMakeResult result)
	{
		switch (result)
		{
		case ObjectMakeResult::Done: return "done";
		case ObjectMakeResult::NothingOpen: return "no level is open";
		case ObjectMakeResult::BadShapeName: return "the shape's name is empty, too long, or has a letter that isn't plain text";
		case ObjectMakeResult::SubNameNotCopied: return "a shape name with an @ isn't copied yet (inteng's second compare isn't read)";
		case ObjectMakeResult::NoSuchShape: return "shape not in this level";
		case ObjectMakeResult::ShapeFileRefused: return "the shape's file couldn't be used";
		case ObjectMakeResult::NoNameFree: return "no name was free for the new object";
		case ObjectMakeResult::PlaceOutOfRange: return "the place is farther than 2^24 millimetres from the middle of the level";
		case ObjectMakeResult::TooManyObjects: return "the level has no room for another object";
		case ObjectMakeResult::CantBuild: return "the level couldn't be put together from its bytes";
		case ObjectMakeResult::LoaderRefused: return "the game's loader wouldn't take the level with that change, so nothing was changed";
		}

		return "the object couldn't be made";
	}

	// FUN_004de070 (out\menu-reads-12.txt, the function at 004de070): the usual turn from a matrix. M[r][c] = whole number (3 r + c) / 65536
	// (_DAT_005ac120 = 1/65536, _DAT_005ac128 = 0.5, _DAT_005ab2b0 = 1.0: engine\inteng\p11-shape-handlers.md section 30). The threshold _DAT_005ab360 is read as 0.0.
	void ObjectMake_TurnFromMatrix(int32_t const *matrix, float *turn)
	{
		double m[3][3];

		for (int row = 0; row < 3; row++)
		{
			for (int column = 0; column < 3; column++)
			{
				m[row][column] = (double)matrix[3 * row + column] * (1.0 / 65536.0);
			}
		}

		double q[4] = { 0, 0, 0, 0 };
		double const trace = m[0][0] + m[1][1] + m[2][2];

		if (trace > 0.0)
		{
			double s = sqrt(trace + 1.0);

			q[3] = s * 0.5;
			s = 0.5 / s;
			q[0] = (m[1][2] - m[2][1]) * s;
			q[1] = (m[2][0] - m[0][2]) * s;
			q[2] = (m[0][1] - m[1][0]) * s;
		}
		else
		{
			int const next[3] = { 1, 2, 0 };
			int i = m[0][0] < m[1][1] ? 1 : 0;

			if (m[i][i] < m[2][2])
			{
				i = 2;
			}

			int const j = next[i];
			int const k = next[j];
			double s = sqrt((m[i][i] - (m[k][k] + m[j][j])) + 1.0);

			q[i] = s * 0.5;
			s = 0.5 / s;
			q[3] = (m[j][k] - m[k][j]) * s;
			q[j] = (m[j][i] + m[i][j]) * s;
			q[k] = (m[k][i] + m[i][k]) * s;
		}

		for (int part = 0; part < 4; part++)
		{
			turn[part] = (float)q[part];
		}
	}

	ObjectMakeResult CreateObjectFromShape(OpenLevelFile *file, char const *shapeName, int32_t const *place, int32_t const *matrix, uint8_t *newId,
	                                       std::string *newName, char const *shapeFolder, std::string *importWhy)
	{
		try
		{
			return MakeIn(file, shapeName, place, matrix, newId, newName, shapeFolder, importWhy);
		}
		catch (std::exception const &)
		{
			return ObjectMakeResult::CantBuild;
		}
	}

	ObjectMakeResult CreateObjectWithShapeChunk(OpenLevelFile *file, std::vector<char> const &shapeChunk, char const *objectBase, int32_t const *place, float const *turn, uint8_t *newId, std::string *newName, bool exactName)
	{
		try
		{
			return MakeIn(file, objectBase, place, nullptr, newId, newName, nullptr, nullptr, &shapeChunk, turn, exactName);
		}
		catch (std::exception const &)
		{
			return ObjectMakeResult::CantBuild;
		}
	}
}
