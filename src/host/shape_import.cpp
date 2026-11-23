// See shape_import.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "shape_import.h"

#include "level_file_name.h"   // LevelFileName_PartIsDevice: CON, PRN, AUX, NUL, COM1-9, LPT1-9

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>           // ShapeImport_Read's one handle

#include <string>
#include <vector>

namespace host
{
	namespace
	{
		using namespace shape_chunks;

		size_t const kHeadPayload = 68;           // SHPHEAD1 up to the count of the objects that use the shape
		size_t const kMostNameBytes = 63;
		int32_t const kMostNumber = 65535;        // Shape_Header_Chunk::Max_File_Id_Num
		uint32_t const kOutsideShapeFlag = 0x1000;
		int32_t const kMostChunkBytes = 0x7fffffff;

		bool Fail(std::string *why, char const *words)
		{
			if (why)
			{
				*why = words;
			}

			return false;
		}

		// A whole chunk of the name `as`, holding the payload of the chunk at `span`
		Bytes Renamed(char const *as, Bytes const &from, Span const &span)
		{
			Bytes payload(from.begin() + (std::ptrdiff_t)(span.at + kHeadingBytes), from.begin() + (std::ptrdiff_t)(span.at + span.size));

			return MakeChunk(as, payload);
		}

		bool NameIsPlain(std::string const &name)
		{
			// the file opened is <folder>\<name>.rif, and Windows opens a DEVICE for "COM1.rif" or "nul.rif" in any folder (level_file_name.h): such a name is refused before anything is
			// opened (OP-141 (a); the GetFileAttributes test of the outside-shape update never sees a device). Found by the 11-17 review (open-path-2).
			if (name.empty() || name.size() > kMostNameBytes || LevelFileName_PartIsDevice(name.c_str(), name.size()))
			{
				return false;
			}

			for (char const letter : name)
			{
				bool const ok = (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') || (letter >= '0' && letter <= '9') || letter == '_' || letter == '-';

				if (!ok)
				{
					return false;
				}
			}

			return true;
		}

	}

	char const *ShapeImportWords(ShapeImportResult result)
	{
		switch (result)
		{
		case ShapeImportResult::Done: return "done";
		case ShapeImportResult::BadName: return "the shape's name has a letter that isn't a letter, digit, _ or -";
		case ShapeImportResult::NoFolder: return "the shape folder isn't set (AVP_OpenInteng.ini, [Editor] ShapeFolder=)";
		case ShapeImportResult::NotFound: return "there is no such shape file in the shape folder";
		case ShapeImportResult::TooBig: return "the shape file is empty or too big";
		case ShapeImportResult::BadFile: return "the shape file isn't one this can copy";
		case ShapeImportResult::NumberTaken: return "no shape number is left in the level";
		case ShapeImportResult::NotPlain: return "the shape file is a link, a second name of another file, a folder, or not a plain file that can be read";
		}

		return "unknown answer";
	}

	bool ShapeImport_NameIsPlain(std::string const &name)
	{
		return NameIsPlain(name);
	}

	ShapeImportResult ShapeImport_Read(char const *folder, std::string const &name, std::vector<char> *bytes, std::string *fileName)
	{
		bytes->clear();

		if (!NameIsPlain(name))
		{
			return ShapeImportResult::BadName;
		}

		if (!folder || !*folder)
		{
			return ShapeImportResult::NoFolder;
		}

		std::string path = folder;

		if (path.back() != '\\' && path.back() != '/')
		{
			path += '\\';
		}

		path += name + ".rif";

		// ONE HANDLE (shape_import.h): opened without following a link, so a link or a junction of that name is opened as itself and refused below
		HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN,
			nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			DWORD const error = GetLastError();

			// nothing of that name: the folder doesn't hold it. Anything else (a folder of that name, a file another program holds): something is there
			return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? ShapeImportResult::NotFound : ShapeImportResult::NotPlain;
		}

		// judged on the handle: a file on a disk (not a device or a pipe), not a reparse point or a folder, and one name only
		ShapeImportResult result = ShapeImportResult::Done;
		BY_HANDLE_FILE_INFORMATION info = {};

		if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandle(file, &info) ||
			(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)) != 0 || info.nNumberOfLinks != 1)
		{
			result = ShapeImportResult::NotPlain;
		}
		else
		{
			uint64_t const length = ((uint64_t)info.nFileSizeHigh << 32) | info.nFileSizeLow;

			if (length == 0 || length > kShapeImportMostBytes)
			{
				result = ShapeImportResult::TooBig;
			}
			else
			{
				// read whole from the same handle
				bytes->assign((size_t)length, 0);

				size_t done = 0;

				while (done < (size_t)length)
				{
					DWORD got = 0;

					if (!ReadFile(file, bytes->data() + done, (DWORD)((size_t)length - done), &got, nullptr) || got == 0)
					{
						break;
					}

					done += got;
				}

				if (done != (size_t)length)
				{
					bytes->clear();
					result = ShapeImportResult::NotPlain;
				}
			}
		}

		CloseHandle(file);

		if (result == ShapeImportResult::Done && fileName)
		{
			*fileName = name + ".rif";
		}

		return result;
	}

	ShapeImportResult ShapeImport_Build(std::vector<char> const &rif, std::string const &fileName, int32_t newNumber, std::vector<char> *shapeChunk, std::string *why)
	{
		shapeChunk->clear();

		if (newNumber < 0 || newNumber > kMostNumber)
		{
			return ShapeImportResult::NumberTaken;
		}

		if (fileName.empty() || fileName.size() > kMostNameBytes + 4)
		{
			return ShapeImportResult::BadName;
		}

		// The file: a REBINFF2 chunk whose size is the file's (or less: what is past it is not read)
		std::vector<Span> top;
		size_t total = 0;

		if (rif.size() < kHeadingBytes || !NameIs(rif, 0, "REBINFF2") || !ChunkSizeAt(rif, 0, rif.size(), &total) || !Children(rif, kHeadingBytes, total, &top))
		{
			Fail(why, "not a rif file (its chunks can't be walked)");
			return ShapeImportResult::BadFile;
		}

		size_t shapeAt = top.size();
		size_t envAt = top.size();

		for (size_t i = 0; i < top.size(); i++)
		{
			if (NameIs(rif, top[i].at, "REBSHAPE"))
			{
				if (shapeAt != top.size())
				{
					Fail(why, "the file holds more than one shape");
					return ShapeImportResult::BadFile;
				}

				shapeAt = i;
			}
			else if (NameIs(rif, top[i].at, "REBENVDT") && envAt == top.size())
			{
				envAt = i;
			}
		}

		if (shapeAt == top.size())
		{
			Fail(why, "the file holds no shape");
			return ShapeImportResult::BadFile;
		}

		if (envAt == top.size())
		{
			Fail(why, "the file has no environment data (its bitmap list and name)");
			return ShapeImportResult::BadFile;
		}

		std::vector<Span> parts;
		std::vector<Span> env;

		if (!Children(rif, top[shapeAt].at + kHeadingBytes, top[shapeAt].at + top[shapeAt].size, &parts) ||
		    !Children(rif, top[envAt].at + kHeadingBytes, top[envAt].at + top[envAt].size, &env))
		{
			Fail(why, "the shape's chunks can't be walked");
			return ShapeImportResult::BadFile;
		}

		size_t const head = FirstNamed(rif, parts, "SHPHEAD1");

		if (head == parts.size() || parts[head].size < kHeadingBytes + kHeadPayload)
		{
			Fail(why, "the shape has no header, or one too short");
			return ShapeImportResult::BadFile;
		}

		if (FirstNamed(rif, parts, "SHPEXTFL") != parts.size())
		{
			Fail(why, "the shape is itself an outside shape");
			return ShapeImportResult::BadFile;
		}

		size_t const bitmaps = FirstNamed(rif, env, "BMPNAMES");
		size_t const extra = FirstNamed(rif, env, "BMNAMEXT");
		size_t const version = FirstNamed(rif, env, "BMNAMVER");
		size_t const riffName = FirstNamed(rif, env, "RIFFNAME");

		if (bitmaps == env.size() || riffName == env.size())
		{
			Fail(why, "the file has no bitmap list or no name");
			return ShapeImportResult::BadFile;
		}

		// SHPHEAD1: the flags 0x1000, no lock user, the new number, then the file's counts, size and version, and no object yet
		size_t const headPayload = parts[head].at + kHeadingBytes;
		Bytes newHead;

		AddInt(&newHead, (int32_t)kOutsideShapeFlag);
		newHead.insert(newHead.end(), 16, 0);
		AddInt(&newHead, newNumber);
		newHead.insert(newHead.end(), rif.begin() + (std::ptrdiff_t)(headPayload + 24), rif.begin() + (std::ptrdiff_t)(headPayload + 64));
		AddInt(&newHead, 0);

		// SHPEXTFN: rescale 1.0, the file's own version number (+60 in its header), the file's name
		Bytes extName;
		double const one = 1.0;

		extName.insert(extName.end(), (char const *)&one, (char const *)&one + sizeof(one));
		AddInt(&extName, IntAt(rif, headPayload + 60));
		extName.insert(extName.end(), fileName.begin(), fileName.end());
		extName.push_back(0);
		PadToFour(&extName);

		Bytes outside;
		Bytes const nameChunk = MakeChunk("SHPEXTFN", extName);

		outside.insert(outside.end(), nameChunk.begin(), nameChunk.end());

		Bytes const list = Renamed("BMPLSTST", rif, env[bitmaps]);

		outside.insert(outside.end(), list.begin(), list.end());

		if (extra != env.size())
		{
			AddCopy(&outside, rif, env[extra]);
		}

		if (version != env.size())
		{
			AddCopy(&outside, rif, env[version]);
		}

		AddCopy(&outside, rif, env[riffName]);

		Bytes const outsideChunk = MakeChunk("SHPEXTFL", outside);

		// the shape: header, the outside file's data, then the file's geometry in the order the stock levels keep it
		Bytes shape(kHeadingBytes, 0);
		Bytes const headChunk = MakeChunk("SHPHEAD1", newHead);

		memcpy(shape.data(), "REBSHAPE", 8);
		shape.insert(shape.end(), headChunk.begin(), headChunk.end());
		shape.insert(shape.end(), outsideChunk.begin(), outsideChunk.end());

		char const *const known[] = { "SHPCENTR", "SHPRAWVT", "SHPVNORM", "SHPPNORM", "SHPPOLYS", "SHPUVCRD", "SHPMRGDT" };
		std::vector<bool> taken(parts.size(), false);

		taken[head] = true;

		for (char const *const wanted : known)
		{
			size_t const at = FirstNamed(rif, parts, wanted);

			if (at != parts.size())
			{
				AddCopy(&shape, rif, parts[at]);
				taken[at] = true;
			}
		}

		for (size_t i = 0; i < parts.size(); i++)
		{
			// SHPFRAGS is the level's own (named gap); anything else the file holds goes in as it is
			if (!taken[i] && !NameIs(rif, parts[i].at, "SHPFRAGS"))
			{
				AddCopy(&shape, rif, parts[i]);
			}
		}

		if (shape.size() > (size_t)kMostChunkBytes)
		{
			Fail(why, "the shape is too big");
			return ShapeImportResult::TooBig;
		}

		PutIntAt(&shape, 8, (int32_t)shape.size());
		*shapeChunk = shape;
		return ShapeImportResult::Done;
	}
}
