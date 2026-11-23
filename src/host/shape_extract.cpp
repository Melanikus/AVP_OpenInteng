// See shape_extract.h. Written 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "shape_extract.h"

#include "huffman.hpp"          // avpx 3dc/win95: the game's own decoder of a packed level, with its checks (HuffmanDecompressChecked)
#include "level_file_name.h"   // LevelFileName_PartIsDevice: CON, PRN, AUX, NUL, COM1-9, LPT1-9
#include "sha256_stream.h"     // avpx 3dc: the loader's own SHA-256, header only
#include "shape_import.h"      // shape_chunks: the one hardened chunk reader, shared with the import

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <set>

namespace host
{
	namespace
	{
		using namespace shape_chunks;

		uint32_t const kOutsideShapeFlag = 0x1000;   // shape_import.cpp: every outside shape in a level has it
		size_t const kHeadPayload = 68;              // SHPHEAD1 up to the count of the objects that use the shape
		size_t const kMostNameBytes = 63;
		size_t const kNamePayloadFrom = 12;          // SHPEXTFN: the rescale (a double), the version (an int), then the name
		int64_t const kMostChunkBytes = 0x7fffffff;

		bool Fail(std::string *why, char const *words)
		{
			if (why)
			{
				*why = words;
			}

			return false;
		}

		// The file's name as the level gives it, made plain (the owner's rule, 2026-11-23: a shape is called by the name the game uses for it): "<name>.rif" in
		// small letters, every letter of the name that isn't a letter, a digit, '_' or '-' made '_' ("spaceship bulb.rif" -> "spaceship_bulb.rif"; a "\" or
		// a "." in the name too, so it can never name another folder), at most 63; refused when it doesn't end ".rif", has nothing before it, or is a device
		bool PlainFileName(std::string const &given, std::string *plain)
		{
			if (given.size() < 5 || given.size() > kMostNameBytes + 4)
			{
				return false;
			}

			std::string lower = given;

			for (char &letter : lower)
			{
				letter = (letter >= 'A' && letter <= 'Z') ? (char)(letter - 'A' + 'a') : letter;
			}

			if (lower.compare(lower.size() - 4, 4, ".rif") != 0)
			{
				return false;
			}

			std::string stem = lower.substr(0, lower.size() - 4);

			for (char &letter : stem)
			{
				letter = ((letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9') || letter == '_' || letter == '-') ? letter : '_';
			}

			if (LevelFileName_PartIsDevice(stem.c_str(), stem.size()))
			{
				return false;
			}

			*plain = stem + ".rif";
			return true;
		}

		// The name a chunk holds from `offset` into its payload, up to its end marker or the chunk's end, whichever comes first
		std::string NameAt(Bytes const &bytes, Span const &chunk, size_t offset)
		{
			size_t const from = chunk.at + kHeadingBytes + offset, end = chunk.at + chunk.size;
			size_t z = from;

			while (z < end && bytes[z] != 0)
			{
				z++;
			}

			return from < end ? std::string(bytes.data() + from, bytes.data() + z) : std::string();
		}

		// The file's name a SHPEXTFN chunk holds
		std::string NameIn(Bytes const &bytes, Span const &name)
		{
			return NameAt(bytes, name, kNamePayloadFrom);
		}

		// The object's name an outside file's data holds when the copy is one PART of that file: its EXTOBJNM (chunk_shape.cpp's
		// Shape_External_Object_Name_Chunk: a number, then the name), or "" when it has none
		size_t const kObjectNameFrom = 4;

		std::string ObjectNameIn(Bytes const &bytes, std::vector<Span> const &inside)
		{
			size_t const object = FirstNamed(bytes, inside, "EXTOBJNM");
			return object == inside.size() ? std::string() : NameAt(bytes, inside[object], kObjectNameFrom);
		}

		// The game's own name for a copy: the file's ("medbox01.rif"), or for a part "<object>@<file>" ("ductcove01@Missfrg.RIF"), not yet made plain
		std::string GameName(std::string const &fileName, std::string const &objectName)
		{
			return objectName.empty() ? fileName : objectName + "@" + fileName;
		}

		Bytes PayloadOf(Bytes const &from, Span const &span)
		{
			return Bytes(from.begin() + (std::ptrdiff_t)(span.at + kHeadingBytes), from.begin() + (std::ptrdiff_t)(span.at + span.size));
		}

		// THE PAD BYTES: a name in a chunk is its letters, an end 0, then pad to a whole 4 bytes. A level's copy holds whatever was in the old editor's memory
		// there ("Pack\0frg" for "Pack"), the package's files 0s (measured 2026-11-23, medbox01 in Derelict, E3demo, Genshd1). The file gets 0s: the format's own.
		// RIFFNAME: one name, from the start.
		void ZeroAfterEnd(Bytes *payload)
		{
			bool ended = false;

			for (char &b : *payload)
			{
				b = ended ? 0 : b;
				ended = ended || b == 0;
			}
		}

		// BMPNAMES (bitmap_names.cpp, Chunk_With_BMPs): a count in the low 16 bits of the first number, then per picture five numbers (its flags first) and its
		// name, ended by a 0 and padded to 4. Each name's pad set to 0, and each picture's flags given "priority and transparency are valid" (0x100,
		// bitmap_names.hpp), as the package's files have it and a level's copy hasn't (Bishop 2's ruling, 2026-11-23: 78 of the 166 extracted shapes then come
		// back as the package's, byte for byte); false when the list doesn't fit its chunk (then it isn't taken)
		uint32_t const kPriorityAndTransparencyAreValid = 0x100;

		bool ZeroNamePads(Bytes *payload)
		{
			if (payload->size() < 4)
			{
				return false;
			}

			uint32_t const count = (uint32_t)IntAt(*payload, 0) & 0xffff;
			size_t at = 4;

			for (uint32_t i = 0; i < count; i++)
			{
				if (at + 20 >= payload->size())
				{
					return false;
				}

				PutIntAt(payload, at, (int32_t)((uint32_t)IntAt(*payload, at) | kPriorityAndTransparencyAreValid));
				at += 20;
				size_t end = at;

				while (end < payload->size() && (*payload)[end] != 0)
				{
					end++;
				}

				size_t const length = end - at;
				size_t const next = at + length + (4 - length % 4);

				if (end >= payload->size() || next > payload->size())
				{
					return false;
				}

				for (size_t z = end; z < next; z++)
				{
					(*payload)[z] = 0;
				}

				at = next;
			}

			return true;
		}
	}

	char const *ShapeExtractWords(ShapeExtractResult result)
	{
		switch (result)
		{
		case ShapeExtractResult::Done: return "made";
		case ShapeExtractResult::NotOutside: return "not an outside shape: it came from no file";
		case ShapeExtractResult::Rescaled: return "a rescaled copy: its points are the file's scaled, so it isn't the file";
		case ShapeExtractResult::BadName: return "the file name it gives isn't a plain one";
		case ShapeExtractResult::BadShape: return "its chunks can't be read as an outside shape";
		}

		return "not made";
	}

	bool ShapeExtract_Placed(std::vector<char> const &level, std::vector<ShapeExtractPlaced> *placed)
	{
		placed->clear();

		size_t total = 0;
		std::vector<Span> top;

		if (level.size() < kHeadingBytes || !NameIs(level, 0, "REBINFF2") || !ChunkSizeAt(level, 0, level.size(), &total) || !Children(level, kHeadingBytes, total, &top))
		{
			return false;
		}

		for (Span const &chunk : top)
		{
			if (!NameIs(level, chunk.at, "REBSHAPE"))
			{
				continue;
			}

			std::vector<Span> parts, inside;

			if (!Children(level, chunk.at + kHeadingBytes, chunk.at + chunk.size, &parts))
			{
				placed->clear();
				return false;
			}

			size_t const outside = FirstNamed(level, parts, "SHPEXTFL");

			if (outside == parts.size())
			{
				continue;
			}

			if (!Children(level, parts[outside].at + kHeadingBytes, parts[outside].at + parts[outside].size, &inside))
			{
				placed->clear();
				return false;
			}

			size_t const name = FirstNamed(level, inside, "SHPEXTFN");

			if (name == inside.size() || inside[name].size < kHeadingBytes + kNamePayloadFrom + 1)
			{
				continue;
			}

			ShapeExtractPlaced one;
			one.at = chunk.at;
			one.size = chunk.size;
			memcpy(&one.rescale, level.data() + inside[name].at + kHeadingBytes, sizeof(one.rescale));
			one.fileName = NameIn(level, inside[name]);
			one.objectName = ObjectNameIn(level, inside);
			placed->push_back(one);
		}

		return true;
	}

	ShapeExtractResult ShapeExtract_File(std::vector<char> const &level, ShapeExtractPlaced const &shape, std::vector<char> *file, std::string *fileName, std::string *why)
	{
		file->clear();
		fileName->clear();

		// the shape as given must be a REBSHAPE chunk inside the level, of that size
		size_t size = 0;
		std::vector<Span> parts, inside;

		if (shape.at >= level.size() || !ChunkSizeAt(level, shape.at, level.size(), &size) || size != shape.size || !NameIs(level, shape.at, "REBSHAPE") ||
		    !Children(level, shape.at + kHeadingBytes, shape.at + size, &parts))
		{
			Fail(why, "the shape's chunk isn't a whole REBSHAPE inside the level");
			return ShapeExtractResult::BadShape;
		}

		size_t const head = FirstNamed(level, parts, "SHPHEAD1");
		size_t const outside = FirstNamed(level, parts, "SHPEXTFL");

		if (outside == parts.size())
		{
			return ShapeExtractResult::NotOutside;
		}

		if (head == parts.size() || parts[head].size < kHeadingBytes + kHeadPayload)
		{
			Fail(why, "the shape has no header, or one too short");
			return ShapeExtractResult::BadShape;
		}

		if (!Children(level, parts[outside].at + kHeadingBytes, parts[outside].at + parts[outside].size, &inside))
		{
			Fail(why, "the outside file's data can't be walked");
			return ShapeExtractResult::BadShape;
		}

		size_t const nameAt = FirstNamed(level, inside, "SHPEXTFN");
		size_t const bitmaps = FirstNamed(level, inside, "BMPLSTST");
		size_t const extra = FirstNamed(level, inside, "BMNAMEXT");
		size_t const version = FirstNamed(level, inside, "BMNAMVER");
		size_t const riffName = FirstNamed(level, inside, "RIFFNAME");

		if (nameAt == inside.size() || inside[nameAt].size < kHeadingBytes + kNamePayloadFrom + 1 || bitmaps == inside.size() || riffName == inside.size())
		{
			Fail(why, "the outside file's data has no name, no bitmap list or no rif name");
			return ShapeExtractResult::BadShape;
		}

		double rescale = 0;
		memcpy(&rescale, level.data() + inside[nameAt].at + kHeadingBytes, sizeof(rescale));

		if (rescale != 1.0)
		{
			return ShapeExtractResult::Rescaled;
		}

		// the game's own name made plain: the whole file's, or a part's "<object>@<file>" -> "<object>_<file>.rif" (shape_extract.h)
		if (!PlainFileName(GameName(NameIn(level, inside[nameAt]), ObjectNameIn(level, inside)), fileName))
		{
			fileName->clear();
			return ShapeExtractResult::BadName;
		}

		// SHPHEAD1: the flags without the outside mark, no lock user, number 0, the level's counts and sizes (+24 to +59), the FILE'S version (+60: the one the
		// level's SHPEXTFN keeps; the level's own +60 is the old editor's edit count, measured 2026-11-23: medbox01 is 27 in the package and in every SHPEXTFN,
		// and 1287, 1072, 4089 in Derelict's, E3demo's and Genshd1's headers), no object using it
		size_t const headPayload = parts[head].at + kHeadingBytes;
		Bytes header;

		AddInt(&header, (int32_t)((uint32_t)IntAt(level, headPayload) & ~kOutsideShapeFlag));
		header.insert(header.end(), 16, 0);
		AddInt(&header, 0);
		header.insert(header.end(), level.begin() + (std::ptrdiff_t)(headPayload + 24), level.begin() + (std::ptrdiff_t)(headPayload + 60));
		AddInt(&header, IntAt(level, inside[nameAt].at + kHeadingBytes + 8));
		AddInt(&header, 0);

		// the shape: its header, then every chunk the level holds but its outside file's data and its own fragment list, in the level's order
		Bytes body(kHeadingBytes, 0);
		Bytes const headChunk = MakeChunk("SHPHEAD1", header);

		memcpy(body.data(), "REBSHAPE", 8);
		body.insert(body.end(), headChunk.begin(), headChunk.end());

		for (size_t i = 0; i < parts.size(); i++)
		{
			if (i != head && i != outside && !NameIs(level, parts[i].at, "SHPFRAGS"))
			{
				AddCopy(&body, level, parts[i]);
			}
		}

		PutIntAt(&body, 8, (int32_t)body.size());

		// the file's environment data: its name, its bitmap list (the level's BMPLSTST under the file's own name), and the bitmaps' extra data and versions
		Bytes environment, nameBytes = PayloadOf(level, inside[riffName]), listBytes = PayloadOf(level, inside[bitmaps]);

		ZeroAfterEnd(&nameBytes);

		if (!ZeroNamePads(&listBytes))
		{
			Fail(why, "the bitmap list doesn't fit its chunk");
			return ShapeExtractResult::BadShape;
		}

		Bytes const nameChunk = MakeChunk("RIFFNAME", nameBytes), list = MakeChunk("BMPNAMES", listBytes);

		environment.insert(environment.end(), nameChunk.begin(), nameChunk.end());
		environment.insert(environment.end(), list.begin(), list.end());

		if (extra != inside.size())
		{
			AddCopy(&environment, level, inside[extra]);
		}

		if (version != inside.size())
		{
			AddCopy(&environment, level, inside[version]);
		}

		Bytes const environmentChunk = MakeChunk("REBENVDT", environment);
		Bytes riffVersion;

		AddInt(&riffVersion, 0);

		Bytes const versionChunk = MakeChunk("RIFVERIN", riffVersion);
		Bytes whole;

		whole.insert(whole.end(), versionChunk.begin(), versionChunk.end());
		whole.insert(whole.end(), body.begin(), body.end());
		whole.insert(whole.end(), environmentChunk.begin(), environmentChunk.end());

		if ((int64_t)whole.size() + (int64_t)kHeadingBytes > kMostChunkBytes)
		{
			Fail(why, "the shape is too big");
			return ShapeExtractResult::BadShape;
		}

		*file = MakeChunk("REBINFF2", whole);
		return ShapeExtractResult::Done;
	}

	std::string ShapeExtract_Sha256(std::vector<char> const &bytes)
	{
		Sha256Stream stream;
		Sha256_Start(&stream);
		Sha256_Add(&stream, bytes.data(), bytes.size());
		uint8_t sum[32];
		Sha256_Finish(&stream, sum);
		static char const digits[] = "0123456789abcdef";
		std::string out;

		for (uint8_t b : sum)
		{
			out += digits[b >> 4];
			out += digits[b & 15];
		}

		return out;
	}

	namespace
	{
		size_t const kMostLevelStem = 40;

		// A level's name made plain for a conflict's name: small letters, every letter that isn't a letter, a digit, '_' or '-' made '_', its ".rif" taken off
		// ("Genshd1.RIF" -> "genshd1")
		std::string PlainLevelStem(std::string const &levelName)
		{
			std::string stem = levelName;

			if (stem.size() >= 4)
			{
				std::string end = stem.substr(stem.size() - 4);

				for (char &letter : end)
				{
					letter = (letter >= 'A' && letter <= 'Z') ? (char)(letter - 'A' + 'a') : letter;
				}

				stem = end == ".rif" ? stem.substr(0, stem.size() - 4) : stem;
			}

			for (char &letter : stem)
			{
				letter = (letter >= 'A' && letter <= 'Z') ? (char)(letter - 'A' + 'a') : letter;
				letter = ((letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9') || letter == '_' || letter == '-') ? letter : '_';
			}

			stem.resize(stem.size() > kMostLevelStem ? kMostLevelStem : stem.size());   // (a stranger's level can have a long name; the shape's own name must still fit)
			return stem;
		}

		// A conflict's name: <name>_<level>, then _2, _3 ... until one is free (the names the scan has given so far, as a set: Vasquez's review, 11-23, a walk
		// over every shape for every try made K copies of one name cost about K^3/3 compares); cut to the 63 letters a plain name may have
		std::string ConflictName(std::set<std::string> const &names, std::string const &plain, std::string const &levelName)
		{
			for (int n = 1;; n++)
			{
				std::string const name = ShapeExtract_ConflictName(plain, levelName, n);

				if (!names.count(name))
				{
					return name;
				}
			}
		}
	}

	std::string ShapeExtract_ConflictName(std::string const &plain, std::string const &levelName, int n)
	{
		std::string const stem = plain.size() > 4 ? plain.substr(0, plain.size() - 4) : plain, level = PlainLevelStem(levelName);
		std::string const tail = "_" + level + (n <= 1 ? std::string() : "_" + std::to_string(n));
		std::string const head = stem.size() + tail.size() > kMostNameBytes ? stem.substr(0, tail.size() < kMostNameBytes ? kMostNameBytes - tail.size() : 0) : stem;
		return head + tail + ".rif";
	}

	void ShapeExtract_Scan(std::vector<ShapeExtractLevel> const &levels, std::vector<ShapeExtractShape> *shapes, std::vector<ShapeExtractSkipped> *skipped,
		std::vector<std::string> *refusedLevels)
	{
		shapes->clear();
		skipped->clear();
		refusedLevels->clear();

		// THE WORK IS KEPT IN STEP WITH THE SHAPES (Vasquez's MUST, 11-23: a hostile map's copies compared byte for byte with every shape made so far, and
		// its names found by walking them all, cost K^2 and K^3): the shapes made so far by their SHA-256 (the bytes compared only where it is the same), the
		// names given as a set, and how many shapes use each name as their original (their own, or for a conflict the one it came from)
		std::map<std::string, std::vector<size_t>> bySha256;
		std::set<std::string> names;
		std::map<std::string, size_t> originals;

		for (ShapeExtractLevel const &level : levels)
		{
			std::vector<ShapeExtractPlaced> placed;

			if (!ShapeExtract_Placed(level.bytes, &placed))
			{
				refusedLevels->push_back(level.name);
				continue;
			}

			for (ShapeExtractPlaced const &one : placed)
			{
				Bytes file;
				std::string plain;
				ShapeExtractResult const made = ShapeExtract_File(level.bytes, one, &file, &plain, nullptr);

				if (made != ShapeExtractResult::Done)
				{
					ShapeExtractSkipped s;
					s.level = level.name;
					s.givenName = GameName(one.fileName, one.objectName);
					s.result = made;
					skipped->push_back(s);
					continue;
				}

				// the whole file's plain name (for a whole-file copy, its own): the same bytes as a part and as the whole file are one shape
				std::string fileOf;

				if (!PlainFileName(one.fileName, &fileOf))
				{
					fileOf = plain;
				}

				// the same bytes under the same name, or from the same file, already made: one more level holds it (the first such shape, in the order made).
				// Else the name is new (it keeps it) or taken by other bytes (a conflict).
				std::string const sha256 = ShapeExtract_Sha256(file);
				bool known = false;

				for (size_t i : bySha256[sha256])
				{
					ShapeExtractShape &s = (*shapes)[i];
					std::string const original = s.conflictOf.empty() ? s.fileName : s.conflictOf;

					if (s.file == file && (original == plain || s.fileOf == fileOf))
					{
						if (s.levels.empty() || s.levels.back() != level.name)
						{
							s.levels.push_back(level.name);
							s.levelSha256.push_back(level.sha256);
						}

						// a part's name gives way to the whole file's, when these very bytes turn up as the whole file and that name is free
						if (one.objectName.empty() && s.conflictOf.empty() && s.fileName != fileOf && plain == fileOf && !names.count(fileOf))
						{
							names.erase(s.fileName);
							originals[s.fileName]--;
							s.fileName = fileOf;
							s.givenName = GameName(one.fileName, one.objectName);
							names.insert(s.fileName);
							originals[s.fileName]++;
						}

						s.copies.push_back({ level.name, one.at });
						known = true;
						break;
					}
				}

				if (known)
				{
					continue;
				}

				bool const nameTaken = originals.count(plain) && originals[plain] > 0;
				ShapeExtractShape s;
				s.givenName = GameName(one.fileName, one.objectName);
				s.fileOf = fileOf;
				s.fileName = nameTaken || names.count(plain) ? ConflictName(names, plain, level.name) : plain;
				s.conflictOf = s.fileName == plain ? std::string() : plain;
				s.file = file;
				s.sha256 = sha256;
				s.levels.push_back(level.name);
				s.levelSha256.push_back(level.sha256);
				s.copies.push_back({ level.name, one.at });
				names.insert(s.fileName);
				originals[s.conflictOf.empty() ? s.fileName : s.conflictOf]++;
				bySha256[sha256].push_back(shapes->size());
				shapes->push_back(s);
			}
		}
	}

	namespace
	{
		// The kinds' words, from the game's own lists (strategy_chunk.hpp's kinds; pvisible.h's object types; equipment.h's weapons; the ammunition FILE
		// numbers add_newsimpleobject turns into the game's)
		std::string StrategyWords(int32_t kind)
		{
			struct { int32_t kind; char const *words; } const kinds[] = {
				{ 50, "door" }, { 51, "lift" }, { 52, "binary switch" }, { 53, "simple object (old)" }, { 54, "platform lift" }, { 57, "airlock" },
				{ 59, "switch door" }, { 60, "lift (no teleport)" }, { 61, "link switch" }, { 62, "placed object" }, { 63, "console" }, { 64, "lighting" },
				{ 66, "multi switch" }, { 67, "teleport" }, { 68, "area switch" }, { 69, "enemy" }, { 70, "mission objective" }, { 71, "mission hint" },
				{ 72, "track" }, { 73, "message" }, { 75, "fan" }, { 76, "hierarchy" }, { 77, "death volume" }, { 78, "self destruct" }, { 79, "generator" },
				{ 80, "track destruct" } };

			for (auto const &one : kinds)
			{
				if (one.kind == kind)
				{
					return one.words;
				}
			}

			return "strategy " + std::to_string(kind);
		}

		std::string ObjectKindWords(int32_t type, int32_t extra)
		{
			static char const *const types[] = { "static", "furniture", "weapon", "ammo", "health", "armour", "key", "boxed sentry gun",
				"IR goggles (AvPx: auto shotgun)", "video screen", "data tape (AvPx: new guns by name)", "motion tracker upgrade", "pheromone pod", "special pickup",
				"hit-me-and-destroy-base", "field charge" };
			static char const *const weapons[] = { "pulse rifle", "auto shotgun", "smartgun", "flamethrower", "plasma gun", "SADAR", "grenade launcher", "minigun",
				"sonic cannon", "beam cannon", "mystery gun", "pred wristblade", "pred pistol", "pred speargun", "pred shoulder cannon", "pred disc",
				"pred medicomp", "pred staff", "alien claw", "alien grab", "alien spit", "cudgel", "marine pistol", "frisbee launcher", "two pistols" };
			struct { int32_t number; char const *words; } const ammo[] = { { 0, "pulse rifle" }, { 1, "shotgun (map tool; no gun)" }, { 2, "smartgun" },
				{ 3, "flamethrower" }, { 5, "SADAR" }, { 6, "grenade launcher" }, { 7, "minigun" }, { 8, "pulse grenade" }, { 9, "pred speargun" } };
			size_t const typeCount = sizeof(types) / sizeof(types[0]), weaponCount = sizeof(weapons) / sizeof(weapons[0]);

			if (type < 0 || (size_t)type >= typeCount)
			{
				return "type " + std::to_string(type);
			}

			if (type == 2)
			{
				return extra >= 0 && (size_t)extra < weaponCount ? std::string("weapon: ") + weapons[extra] : "weapon: number " + std::to_string(extra);
			}

			if (type == 3)
			{
				for (auto const &one : ammo)
				{
					if (one.number == extra)
					{
						return std::string("ammo: ") + one.words;
					}
				}

				return "ammo: file number " + std::to_string(extra);
			}

			if (type == 0 || type == 1 || extra == 0)
			{
				return types[type];
			}

			return std::string(types[type]) + " (sub " + std::to_string(extra) + ")";
		}

		// The kind of one placing object (RBOBJECT), as shape_extract.h says
		std::string KindOf(Bytes const &level, Span const &object)
		{
			std::vector<Span> parts, project;

			if (!Children(level, object.at + kHeadingBytes, object.at + object.size, &parts))
			{
				return "unreadable object";
			}

			size_t const data = FirstNamed(level, parts, "OBJPRJDT");

			if (data == parts.size() || !Children(level, parts[data].at + kHeadingBytes, parts[data].at + parts[data].size, &project))
			{
				return "prop (no strategy)";
			}

			size_t const strategy = FirstNamed(level, project, "AVPSTRAT");

			if (strategy == project.size())
			{
				return "prop (no strategy)";
			}

			size_t const at = project[strategy].at + kHeadingBytes, size = project[strategy].size - kHeadingBytes;

			if (size < 20)
			{
				return "strategy too short";
			}

			int32_t const kind = IntAt(level, at + 8);

			if (kind != 62 && kind != 53)
			{
				return StrategyWords(kind);
			}

			if (kind == 62 && size < 28)
			{
				return "strategy too short";
			}

			if (kind == 62 && FirstNamed(level, parts, "PLOBJLIT") != parts.size())
			{
				return "placed light";
			}

			return ObjectKindWords(IntAt(level, at + (kind == 62 ? 20 : 12)), IntAt(level, at + (kind == 62 ? 24 : 16)));
		}
	}

	bool ShapeExtract_Placements(std::vector<char> const &level, std::vector<ShapeExtractPlacement> *placements)
	{
		placements->clear();

		size_t total = 0;
		std::vector<Span> top;

		if (level.size() < kHeadingBytes || !NameIs(level, 0, "REBINFF2") || !ChunkSizeAt(level, 0, level.size(), &total) || !Children(level, kHeadingBytes, total, &top))
		{
			return false;
		}

		// each outside shape's number (its SHPHEAD1 +20), its file name in small letters, and where its REBSHAPE starts
		struct Numbered
		{
			int32_t number;
			std::string name;
			size_t at;
		};

		// (by number, the FIRST shape of each number kept: Vasquez's review, 11-23, a walk over every shape for every object cost objects x shapes)
		std::map<int32_t, Numbered> numbered;
		std::vector<ShapeExtractPlaced> placed;

		if (!ShapeExtract_Placed(level, &placed))
		{
			return false;
		}

		for (ShapeExtractPlaced const &one : placed)
		{
			std::vector<Span> parts;
			Children(level, one.at + kHeadingBytes, one.at + one.size, &parts);   // (ShapeExtract_Placed walked it already)
			size_t const head = FirstNamed(level, parts, "SHPHEAD1");

			if (head == parts.size() || parts[head].size < kHeadingBytes + 24)
			{
				continue;
			}

			std::string name = one.fileName;

			for (char &letter : name)
			{
				letter = (letter >= 'A' && letter <= 'Z') ? (char)(letter - 'A' + 'a') : letter;
			}

			int32_t const number = IntAt(level, parts[head].at + kHeadingBytes + 20);
			numbered.insert({ number, { number, name, one.at } });   // (insert keeps the first of a number)
		}

		for (Span const &chunk : top)
		{
			std::vector<Span> parts;

			if (!NameIs(level, chunk.at, "RBOBJECT") || !Children(level, chunk.at + kHeadingBytes, chunk.at + chunk.size, &parts))
			{
				continue;
			}

			size_t const head = FirstNamed(level, parts, "OBJHEAD1");

			if (head == parts.size() || parts[head].size < kHeadingBytes + 60)
			{
				continue;
			}

			int32_t const shape = IntAt(level, parts[head].at + kHeadingBytes + 56);

			auto const found = numbered.find(shape);

			if (found != numbered.end())
			{
				ShapeExtractPlacement p;
				p.fileName = found->second.name;
				p.shapeAt = found->second.at;
				p.kind = KindOf(level, chunk);
				placements->push_back(p);
			}
		}

		return true;
	}

	ShapeExtractMap ShapeExtract_ScanMap(std::string const &mapName, std::vector<char> const &fileBytes)
	{
		ShapeExtractMap map;
		map.sha256 = ShapeExtract_Sha256(fileBytes);

		ShapeExtractLevel level;
		level.name = mapName;
		level.sha256 = map.sha256;

		// THE MAP'S BYTES AS THE GAME READS THEM (hostile: a stranger's map): plain (REBINFF2) as they are; packed (REBCRIF1) unpacked by the game's own
		// decoder, which refuses a packing that doesn't decode safely and completely; anything else, or too big, is no map
		if (fileBytes.size() > kShapeExtractMostMapBytes)
		{
			map.why = "the file is larger than 16 MB";
			return map;
		}

		if (fileBytes.size() >= kHeadingBytes && NameIs(fileBytes, 0, "REBINFF2"))
		{
			level.bytes = fileBytes;
		}
		else if (fileBytes.size() >= sizeof(HuffmanPackage) && memcmp(fileBytes.data(), COMPRESSED_RIF_IDENTIFIER, 8) == 0)
		{
			int32_t const unpackedSize = IntAt(fileBytes, offsetof(HuffmanPackage, UncompressedDataSize));

			if (unpackedSize < (int32_t)kHeadingBytes || (uint64_t)unpackedSize > 4ull * kShapeExtractMostMapBytes)
			{
				map.why = "packed, but the size it says it unpacks to is not a map's";
				return map;
			}

			// A packed file is its header and then exactly the packed bytes its header counts: all 86 stock files are (measured 2026-11-23). The game's
			// decoder reads zero bits past a package's end, and a complete code makes bytes of them, so a file cut short would "decode" into bytes it
			// never held; it is refused here first, by its length.
			int32_t const packedSize = IntAt(fileBytes, offsetof(HuffmanPackage, CompressedDataSize));

			if (packedSize < 0 || (uint64_t)fileBytes.size() != (uint64_t)sizeof(HuffmanPackage) + (uint64_t)packedSize)
			{
				map.why = "packed, but the file isn't its header and the " + std::to_string(packedSize) + " packed bytes it counts (it has " +
					std::to_string(fileBytes.size()) + " bytes): cut short or added to";
				return map;
			}

			char *const unpacked = HuffmanDecompressChecked((HuffmanPackage const *)fileBytes.data(), (int)fileBytes.size());

			if (!unpacked)
			{
				map.why = "packed, but its packing doesn't decode safely and completely";
				return map;
			}

			level.bytes.assign(unpacked, unpacked + unpackedSize);
			free(unpacked);
		}
		else
		{
			map.why = "not a map: it is neither a plain (REBINFF2) nor a packed (REBCRIF1) level file";
			return map;
		}

		// the work this map asks for, counted before any of it is done (shape_extract.h)
		{
			std::vector<ShapeExtractPlaced> outside;
			std::vector<Span> top;
			size_t total = 0, objects = 0;

			if (!ShapeExtract_Placed(level.bytes, &outside) || !ChunkSizeAt(level.bytes, 0, level.bytes.size(), &total) || !Children(level.bytes, kHeadingBytes, total, &top))
			{
				map.why = "its chunks can't be walked: a size runs past its parent";
				return map;
			}

			for (Span const &chunk : top)
			{
				objects += NameIs(level.bytes, chunk.at, "RBOBJECT") ? 1 : 0;
			}

			if (outside.size() > kShapeExtractMostShapesInMap || objects > kShapeExtractMostObjectsInMap)
			{
				map.why = outside.size() > kShapeExtractMostShapesInMap ? "it holds " + std::to_string(outside.size()) + " outside shapes, more than the " +
					std::to_string(kShapeExtractMostShapesInMap) + " a map may (stock's most is 45)" : "it holds " + std::to_string(objects) + " objects, more than the " +
					std::to_string(kShapeExtractMostObjectsInMap) + " a map may (stock's most is 885)";
				return map;
			}
		}

		std::vector<std::string> refused;
		ShapeExtract_Scan({ level }, &map.shapes, &map.skipped, &refused);

		if (!refused.empty() || !ShapeExtract_Placements(level.bytes, &map.placements))
		{
			map.shapes.clear();
			map.skipped.clear();
			map.placements.clear();
			map.why = "its chunks can't be walked: a size runs past its parent";
			return map;
		}

		map.scanned = true;
		return map;
	}
}
