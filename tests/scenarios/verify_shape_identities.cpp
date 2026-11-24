// verify_shape_identities.cpp: makes, and checks, the table of src/host/shape_identity.h (what each stock outside shape IS, known by its SHA-256). Written
// 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USAGE:
//   verify_shape_identities make  <stock copy> [<anchor shape folder>]   prints the table (src/host/shape_identity_data.cpp's text) on its output, and
//                                                                        what it found on its error output
//   verify_shape_identities check <stock copy> [<anchor shape folder>]   says whether the table built into this program is the one "make" would print
//   verify_shape_identities stock <stock copy>                           the editor's own stock check (shape_extract_files.h ShapeExtract_StockCheck,
//                                                                        Drake's FOUND form) on the copy: every file Steam's own (63 levels, 23 HUD
//                                                                        files), none missing, none not own, none refused (Dietrich, 2026-11-23)
//   verify_shape_identities endtoend <stock copy> <new scratch folder>   THE END-TO-END STOCK PROOF (Dietrich, 2026-11-23, Frost's order): the editor's
//                                                                        own one call per map (ShapeExtract_LevelToFolder) over all 86 stock files into a
//                                                                        scratch folder made new, inside AvP-builds-excluded only (never Tools\shape_rifs
//                                                                        or Steam's): every map read, none failed; every file written a row of the table,
//                                                                        by its SHA-256 and with the row's name; shapes.txt a line for each file; and a
//                                                                        second run writes nothing (all already there). It says the counts and the time.
// <stock copy>: a folder holding avp_rifs\*.rif (the 63 levels) and avp_huds\*.rif (the 23 HUD files) as Steam ships them, checked against Steam's depot
// manifests OUTSIDE the repo first (AvP-docs tools/frost/stock-check/verify-stock-shapes.js; the owner, 2026-11-23: "SHA-1 stays out of the repo"); this
// program records only sizes and SHA-256. <anchor shape folder>: Rebellion's own shape files we hold (the old tools' package), on our machine only: a row
// whose geometry is exactly one of them is "anchored". Without it no row is.
//
// WHAT IT DOES, with the editor's own code (shape_extract.h): every file of the two folders read (a plain file, at most 64 MB, unpacked by the game's own
// decoder), hashed as it is on disk, scanned in a fixed order (avp_rifs then avp_huds, each by name in small letters) by ShapeExtract_Scan (so the same copy
// always gives the same rows), and each row's kind counted from the objects placing exactly that variant (ShapeExtract_Placements). It writes no file.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <windows.h>

#include "huffman.hpp"
#include "shape_extract.h"
#include "shape_extract_files.h"
#include "shape_identity.h"
#include "shape_import.h"

// the game's memory code (our_mem.cpp, for the decoder) calls these: here they only say what happened
void avp_MessageBox(const char *message, int)
{
	fprintf(stderr, "the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	fprintf(stderr, "FAILED: the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

using namespace host;
using namespace host::shape_chunks;
namespace fs = std::filesystem;

static uint64_t const kMostBytes = 64ull * 1024 * 1024;

static std::string Lower(std::string s)
{
	for (char &c : s)
	{
		c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
	}

	return s;
}

// A folder from the command line, made whole (GetFullPathNameA) and checked before anything is read under it: it must exist, be a folder, and not be a
// link or a junction; one with ".." or longer than a path can be is refused. What is used is the whole path, not the argument (as verify_level_same and
// verify_all_stock_maps do: Snyk Code, a command line's path used unchecked).
static bool WholeFolder(char const *given, fs::path *out)
{
	if (!given || !given[0] || strlen(given) >= MAX_PATH || strstr(given, "..") != nullptr)
	{
		return false;
	}

	char full[MAX_PATH] = "";
	DWORD const length = GetFullPathNameA(given, MAX_PATH, full, nullptr);

	if (length == 0 || length >= MAX_PATH)
	{
		return false;
	}

	DWORD const kind = GetFileAttributesA(full);

	if (kind == INVALID_FILE_ATTRIBUTES || (kind & FILE_ATTRIBUTE_REPARSE_POINT) != 0 || (kind & FILE_ATTRIBUTE_DIRECTORY) == 0)
	{
		return false;
	}

	*out = fs::path(std::string(full));
	return true;
}

// a file's bytes, whole, or false (none, not a plain file, or over the cap)
static bool ReadAll(fs::path const &file, Bytes *bytes)
{
	std::error_code e;

	if (!fs::is_regular_file(fs::symlink_status(file, e)) || e || fs::file_size(file, e) > kMostBytes || e)
	{
		return false;
	}

	std::ifstream in(file, std::ios::binary);
	bytes->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return !in.bad();
}

// A level's bytes as the game reads them: a packed one (REBCRIF1) unpacked by the game's own decoder, with its own checks
static bool Unpacked(Bytes const &raw, Bytes *out)
{
	if (raw.size() >= 8 && memcmp(raw.data(), "REBINFF2", 8) == 0)
	{
		*out = raw;
		return true;
	}

	if (raw.size() < sizeof(HuffmanPackage) || memcmp(raw.data(), COMPRESSED_RIF_IDENTIFIER, 8) != 0)
	{
		return false;
	}

	int size = 0;
	memcpy(&size, raw.data() + offsetof(HuffmanPackage, UncompressedDataSize), sizeof(size));

	if (size < 12 || (uint64_t)size > 4 * kMostBytes)
	{
		return false;
	}

	std::unique_ptr<char, void (*)(void *)> const unpacked(HuffmanDecompressChecked((HuffmanPackage const *)raw.data(), (int)raw.size()), free);

	if (!unpacked)
	{
		return false;
	}

	out->assign(unpacked.get(), unpacked.get() + size);
	return true;
}

// The parts of a file's first shape that make its GEOMETRY: the points, the vertex and polygon normals, the polygons and the texture coordinates
// (shape_import.h's proof). The centre is left out: 5 stock copies hold one the package's file lacks. For a file of one shape, that shape; for one of
// several (a fragment file), the shape the object `objectName` uses. Empty when there is no such one shape.
static std::map<std::string, Bytes> GeometryOf(Bytes const &file, std::string const &objectName = std::string())
{
	std::map<std::string, Bytes> out;
	std::vector<Span> top, parts;
	size_t total = 0, shapes = 0, shape = 0;

	if (file.size() < kHeadingBytes || !ChunkSizeAt(file, 0, file.size(), &total) || !Children(file, kHeadingBytes, total, &top))
	{
		return out;
	}

	for (size_t i = 0; i < top.size(); i++)
	{
		if (NameIs(file, top[i].at, "REBSHAPE"))
		{
			shapes++;
			shape = i;
		}
	}

	// ONE PART of a file with several shapes (a fragment file): the shape the named object uses, found as the game finds it: the object's OBJHEAD1 name
	// (+60 on) and its shape number (+56), which is that shape's SHPHEAD1 number (+20) (chunk_object.cpp:211)
	if (!objectName.empty() && shapes > 1)
	{
		int32_t number = -1;

		for (Span const &t : top)
		{
			std::vector<Span> op;

			if (NameIs(file, t.at, "RBOBJECT") && Children(file, t.at + kHeadingBytes, t.at + t.size, &op) && !op.empty() && NameIs(file, op[0].at, "OBJHEAD1") &&
				op[0].size >= kHeadingBytes + 61)
			{
				size_t const from = op[0].at + kHeadingBytes + 60, end = op[0].at + op[0].size;
				size_t z = from;

				while (z < end && file[z] != 0)
				{
					z++;
				}

				if (Lower(std::string(file.data() + from, file.data() + z)) == Lower(objectName))
				{
					number = IntAt(file, op[0].at + kHeadingBytes + 56);
				}
			}
		}

		shapes = 0;

		for (size_t i = 0; number >= 0 && i < top.size(); i++)
		{
			std::vector<Span> sp;

			if (NameIs(file, top[i].at, "REBSHAPE") && Children(file, top[i].at + kHeadingBytes, top[i].at + top[i].size, &sp))
			{
				size_t const head = FirstNamed(file, sp, "SHPHEAD1");

				if (head != sp.size() && sp[head].size >= kHeadingBytes + 24 && IntAt(file, sp[head].at + kHeadingBytes + 20) == number)
				{
					shapes++;
					shape = i;
				}
			}
		}
	}

	if (shapes != 1 || !Children(file, top[shape].at + kHeadingBytes, top[shape].at + top[shape].size, &parts))
	{
		return out;
	}

	for (char const *id : { "SHPRAWVT", "SHPVNORM", "SHPPNORM", "SHPPOLYS", "SHPUVCRD" })
	{
		size_t const i = FirstNamed(file, parts, id);

		if (i != parts.size())
		{
			out[id] = Bytes(file.begin() + (std::ptrdiff_t)(parts[i].at + kHeadingBytes), file.begin() + (std::ptrdiff_t)(parts[i].at + parts[i].size));
		}
	}

	return out;
}

// A C string's text: only the letters a stock name or kind has; a backslash or quote is written escaped, anything else refused
static bool Quoted(std::string const &text, std::string *out)
{
	*out = "\"";

	for (char c : text)
	{
		if (c == '\\' || c == '"')
		{
			*out += '\\';
		}
		else if (c < 32 || c > 126)
		{
			return false;
		}

		*out += c;
	}

	*out += '"';
	return true;
}

struct Made
{
	std::vector<ShapeIdentityStockFile> stock;   // (the strings live in the vectors below)
	std::vector<std::string> stockNames, stockSums, stockFolders;
	struct Row
	{
		std::string sha256, fileName, kind, evidence, levels;
		bool anchored = false;
		ShapeIdentityPlacement placement = {};   // its commonest stock placement (shape_identity.h; Dietrich 2026-11-24)
	};
	std::vector<Row> rows;
};

// Everything "make" prints and "check" compares, from the copy (and the anchors, when given). False, with the reason on the error output, when it can't.
static bool MakeTable(fs::path const &stock, fs::path const *anchors, Made *made)
{
	std::vector<ShapeExtractLevel> levels;
	std::map<std::string, std::string> folderOf;   // a level's file name -> "avp_rifs" or "avp_huds"
	std::error_code e;

	for (char const *folder : { "avp_rifs", "avp_huds" })
	{
		std::vector<fs::path> files;

		for (fs::directory_iterator it(stock / folder, e), end; !e && it != end; it.increment(e))
		{
			if (Lower(it->path().extension().string()) == ".rif" && fs::is_regular_file(fs::symlink_status(it->path(), e)))
			{
				files.push_back(it->path());
			}
		}

		if (e || files.empty())
		{
			fprintf(stderr, "FAILED: %s\\%s can't be read, or holds no .rif\n", stock.string().c_str(), folder);
			return false;
		}

		std::sort(files.begin(), files.end(), [](fs::path const &x, fs::path const &y) { return Lower(x.filename().string()) < Lower(y.filename().string()); });

		for (fs::path const &file : files)
		{
			Bytes raw;
			ShapeExtractLevel level;

			if (!ReadAll(file, &raw) || !Unpacked(raw, &level.bytes))
			{
				fprintf(stderr, "FAILED: %s can't be read or unpacked\n", file.string().c_str());
				return false;
			}

			level.name = file.filename().string();
			level.sha256 = ShapeExtract_Sha256(raw);
			folderOf[level.name] = folder;
			made->stockFolders.push_back(folder);
			made->stockNames.push_back(Lower(level.name));
			made->stockSums.push_back(level.sha256);
			made->stock.push_back({ nullptr, nullptr, (uint64_t)raw.size(), nullptr });
			levels.push_back(level);
		}
	}

	std::vector<ShapeExtractShape> shapes;
	std::vector<ShapeExtractSkipped> skipped;
	std::vector<std::string> refused;
	ShapeExtract_Scan(levels, &shapes, &skipped, &refused);

	if (!refused.empty())
	{
		fprintf(stderr, "FAILED: a stock file's chunks can't be walked: %s\n", refused[0].c_str());
		return false;
	}

	// every level's placing objects
	std::map<std::string, std::vector<ShapeExtractPlacement>> placements;

	for (ShapeExtractLevel const &level : levels)
	{
		ShapeExtract_Placements(level.bytes, &placements[level.name]);
	}

	size_t anchored = 0, withKind = 0, disagreeing = 0;
	std::map<std::string, std::set<std::string>> fileSums, geometrySums;   // per original name: its files' SHA-256s, and its geometries'

	for (ShapeExtractShape const &shape : shapes)
	{
		Made::Row row;
		row.sha256 = shape.sha256;
		row.fileName = shape.fileName;
		std::string const original = shape.conflictOf.empty() ? shape.fileName : shape.conflictOf;

		// the kinds of the objects placing exactly this variant: those whose (level, shape place) is one of its copies
		std::map<std::string, size_t> kinds;
		std::vector<ShapeExtractPlacement const *> placing;   // (their own fields, for the row's stock placement below)
		size_t count = 0;

		for (auto const &[levelName, at] : shape.copies)
		{
			for (ShapeExtractPlacement const &p : placements[levelName])
			{
				if (p.shapeAt == at)
				{
					kinds[p.kind]++;
					placing.push_back(&p);
					count++;
				}
			}
		}

		if (count == 0)
		{
			row.evidence = "no object places it";
		}
		else
		{
			std::vector<std::pair<size_t, std::string>> ranked;

			for (auto const &[kind, n] : kinds)
			{
				ranked.push_back({ n, kind });
			}

			std::stable_sort(ranked.begin(), ranked.end(), [](auto const &x, auto const &y) { return x.first > y.first; });
			row.kind = ranked[0].second;
			row.evidence = std::to_string(ranked[0].first) + " of " + std::to_string(count) + " placements";

			for (size_t i = 1; i < ranked.size(); i++)
			{
				row.evidence += "; " + ranked[i].second + " " + std::to_string(ranked[i].first);
			}

			withKind++;
			disagreeing += ranked.size() > 1 ? 1 : 0;

			// ITS STOCK PLACEMENT (Dietrich 2026-11-24, shape_identity.h ShapeIdentityPlacement): among the objects of the row's kind placing this variant, the
			// commonest WHOLE set of fields of those with a simple kind 62 strategy and no target; a tie goes to the smallest set in the map's order, so the same
			// copy always gives the same row
			std::map<std::tuple<int32_t, int32_t, int32_t, uint32_t, uint32_t>, uint32_t> sets;
			uint32_t ofKind = 0;

			for (ShapeExtractPlacement const *p : placing)
			{
				if (p->kind != row.kind)
				{
					continue;
				}

				ofKind++;

				if (p->simple && !p->targeted)
				{
					sets[std::make_tuple(p->type, p->extra, p->mass, p->integrity, p->flags)]++;
				}
			}

			for (auto const &[set, n] : sets)
			{
				if (n > row.placement.same)
				{
					row.placement.known = true;
					row.placement.type = std::get<0>(set);
					row.placement.extra = std::get<1>(set);
					row.placement.mass = std::get<2>(set);
					row.placement.integrity = std::get<3>(set);
					row.placement.flags = std::get<4>(set);
					row.placement.same = n;
				}
			}

			row.placement.of = row.placement.known ? ofKind : 0;
		}

		for (std::string const &l : shape.levels)
		{
			row.levels += (row.levels.empty() ? "" : ";") + folderOf[l] + "/" + l;
		}

		// ANCHORED: the geometry exactly that of Rebellion's own FILE it came from (shape.fileOf): its one shape, or for a part of a fragment file the very shape
		// its object (the part's EXTOBJNM, the name before the '@' in the game's name) uses there
		std::map<std::string, Bytes> const mine = GeometryOf(shape.file);

		if (anchors)
		{
			Bytes raw, unpacked;
			fs::path const file = *anchors / (shape.fileOf.empty() ? original : shape.fileOf);
			size_t const at = shape.givenName.find('@');
			std::string const object = at == std::string::npos ? std::string() : shape.givenName.substr(0, at);

			if (ReadAll(file, &raw) && Unpacked(raw, &unpacked))
			{
				std::map<std::string, Bytes> const theirs = GeometryOf(unpacked, object);
				row.anchored = !mine.empty() && mine.count("SHPRAWVT") == 1 && mine == theirs;
				anchored += row.anchored ? 1 : 0;
			}
		}

		Bytes geometry;

		for (auto const &[id, payload] : mine)
		{
			geometry.insert(geometry.end(), id.begin(), id.end());
			geometry.insert(geometry.end(), payload.begin(), payload.end());
		}

		fileSums[original].insert(shape.sha256);
		geometrySums[original].insert(ShapeExtract_Sha256(geometry));
		made->rows.push_back(row);
	}

	size_t variedNames = 0, variedByGeometry = 0;

	for (auto const &[name, sums] : fileSums)
	{
		variedNames += sums.size() > 1 ? 1 : 0;
		variedByGeometry += geometrySums[name].size() > 1 ? 1 : 0;
	}

	fprintf(stderr, "made: %zu stock files (%zu levels read); %zu rows from %zu names (%zu copies skipped: rescaled or not plain); %zu rows with a kind (%zu of them with "
		"objects disagreeing), %zu placed by no object; %s %zu rows anchored\n", made->stock.size(), levels.size(), made->rows.size(), fileSums.size(), skipped.size(),
		withKind, disagreeing, made->rows.size() - withKind, anchors ? "anchors given:" : "no anchors:", anchored);
	fprintf(stderr, "the near-match key (a SHA-256 of the geometry only): names with more than one file %zu, with more than one geometry %zu\n", variedNames, variedByGeometry);
	return true;
}

// Adds printf-style text to a string
static void Out(std::string *text, char const *format, ...)
{
	// (however long: a row's list of levels can be long, so it is measured first, then written)
	va_list args, again;
	va_start(args, format);
	va_copy(again, args);
	int const n = vsnprintf(nullptr, 0, format, args);
	va_end(args);

	if (n > 0)
	{
		std::vector<char> line((size_t)n + 1);
		vsnprintf(line.data(), line.size(), format, again);
		text->append(line.data(), (size_t)n);
	}

	va_end(again);
}

static int Usage()
{
	fprintf(stderr, "usage: verify_shape_identities make|check <stock copy holding avp_rifs and avp_huds> [<anchor shape folder>], stock <stock copy>, or endtoend <stock copy> <new scratch folder>\n");
	return 2;
}

// The scratch folder for "endtoend", made whole and checked BEFORE anything is made: no "..", not there yet (a NEW folder: nothing in it is ours
// to judge), its folder above there and plain (no link or junction), inside AvP-builds-excluded (a part of its path by that name), and never a
// shape_rifs folder or anything under Steam's steamapps (the game's own). Made here, and only then used.
static bool NewScratchFolder(char const *given, fs::path *out, std::string *why)
{
	char full[MAX_PATH] = "";
	DWORD const length = given && given[0] && strlen(given) < MAX_PATH && !strstr(given, "..") ? GetFullPathNameA(given, MAX_PATH, full, nullptr) : 0;

	if (length == 0 || length >= MAX_PATH)
	{
		*why = "the scratch folder's path is refused (empty, too long, or with \"..\")";
		return false;
	}

	std::string const whole = full, lower = Lower(whole);
	size_t const slash = whole.find_last_of('\\');
	std::string const above = slash == std::string::npos ? std::string() : whole.substr(0, slash);
	DWORD const aboveKind = above.empty() ? INVALID_FILE_ATTRIBUTES : GetFileAttributesA(above.c_str());

	if (lower.find("\\avp-builds-excluded\\") == std::string::npos || lower.find("\\steamapps\\") != std::string::npos ||
		lower.find("shape_rifs") != std::string::npos)
	{
		*why = whole + ": the scratch folder must be inside AvP-builds-excluded, and never a shape_rifs folder or under Steam's steamapps";
		return false;
	}

	if (GetFileAttributesA(full) != INVALID_FILE_ATTRIBUTES)
	{
		*why = whole + " is there already: the proof writes only into a folder it makes new";
		return false;
	}

	if (aboveKind == INVALID_FILE_ATTRIBUTES || (aboveKind & FILE_ATTRIBUTE_REPARSE_POINT) || !(aboveKind & FILE_ATTRIBUTE_DIRECTORY) || !CreateDirectoryA(full, nullptr))
	{
		*why = whole + " can't be made: the folder above it must be a plain folder that is there";
		return false;
	}

	*out = fs::path(whole);
	return true;
}

// One pass of the one call per map over the stock copy, in the table's fixed order (avp_rifs then avp_huds, each by name in small letters), as the
// files are named on the disk
struct Pass
{
	size_t maps = 0, read = 0, failed = 0;
	std::vector<std::string> written, alreadyThere, skipped, failures;
	unsigned long ms = 0;
};

static Pass RunPass(fs::path const &stock, fs::path const &scratch)
{
	Pass pass;
	DWORD const start = GetTickCount();

	for (char const *folder : { "avp_rifs", "avp_huds" })
	{
		std::vector<fs::path> files;
		std::error_code e;

		for (fs::directory_iterator it(stock / folder, e), end; !e && it != end; it.increment(e))
		{
			if (Lower(it->path().extension().string()) == ".rif" && fs::is_regular_file(fs::symlink_status(it->path(), e)))
			{
				files.push_back(it->path());
			}
		}

		std::sort(files.begin(), files.end(), [](fs::path const &a, fs::path const &b) { return Lower(a.filename().string()) < Lower(b.filename().string()); });

		for (fs::path const &file : files)
		{
			ShapeLevelReport report;
			bool const done = ShapeExtract_LevelToFolder(file.string(), scratch.string(), &report);

			pass.maps++;
			pass.read += report.read ? 1 : 0;

			if (!done || report.failed)
			{
				pass.failed++;
				pass.failures.push_back(std::string(folder) + "/" + file.filename().string() + ": " + report.why);
			}

			pass.written.insert(pass.written.end(), report.written.begin(), report.written.end());
			pass.alreadyThere.insert(pass.alreadyThere.end(), report.alreadyThere.begin(), report.alreadyThere.end());

			for (std::string const &s : report.skipped)
			{
				pass.skipped.push_back(std::string(folder) + "/" + file.filename().string() + ": " + s);
			}
		}
	}

	pass.ms = GetTickCount() - start;
	return pass;
}

// "endtoend": THE END-TO-END STOCK PROOF
static int EndToEndMode(fs::path const &stock, char const *scratchGiven)
{
	fs::path scratch;
	std::string why;
	int wrong = 0;
	auto say = [&wrong](bool ok, std::string const &what)
	{
		printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
		wrong += ok ? 0 : 1;
	};
	auto first = [](std::vector<std::string> const &list)
	{
		return list.empty() ? std::string("none") : list[0] + (list.size() > 1 ? " and " + std::to_string(list.size() - 1) + " more" : std::string());
	};

	if (!NewScratchFolder(scratchGiven, &scratch, &why))
	{
		say(false, "the scratch folder: " + why);
		return 1;
	}

	printf("  the scratch folder, made new: %s\n", scratch.string().c_str());

	// FIRST RUN: every map read, none failed
	Pass const one = RunPass(stock, scratch);

	say(one.maps == 86 && one.read == 86 && one.failed == 0, "first run: " + std::to_string(one.maps) + " maps, " + std::to_string(one.read) + " read, " +
		std::to_string(one.failed) + " failed (" + first(one.failures) + "), in " + std::to_string(one.ms) + " ms");
	printf("  first run: %zu shape files written, %zu already there (the same bytes from an earlier map), %zu skipped (%s)\n", one.written.size(),
		one.alreadyThere.size(), one.skipped.size(), first(one.skipped).c_str());

	// every written file: a row of the table, by its SHA-256, with the row's name
	std::vector<std::string> notRows, otherName;
	std::set<std::string> rowsWritten;

	for (std::string const &name : one.written)
	{
		std::vector<char> bytes;
		std::string sha256;
		std::string const readWhy = ShapeExtract_ReadMapFile((scratch / name).string(), &bytes, &sha256);
		ShapeIdentityRow const *const row = readWhy.empty() ? ShapeIdentity_Find(sha256) : nullptr;

		if (!row)
		{
			notRows.push_back(name + (readWhy.empty() ? " (" + sha256.substr(0, 12) + "...)" : ": " + readWhy));
		}
		else if (Lower(row->fileName) != Lower(name))
		{
			otherName.push_back(name + " (the row says " + row->fileName + ")");
		}
		else
		{
			rowsWritten.insert(row->sha256);
		}
	}

	say(!one.written.empty() && notRows.empty(), "every one of the " + std::to_string(one.written.size()) + " files written is a row of the table, by its SHA-256 (not: " +
		first(notRows) + ")");
	say(otherName.empty(), "and each has the row's name (other: " + first(otherName) + ")");
	printf("  the table has %zu rows; %zu of them written by this run\n", kShapeIdentityRowCount, rowsWritten.size());

	// shapes.txt: a line for each file written, and for no other
	std::ifstream in(scratch / "shapes.txt", std::ios::binary);
	std::string line;
	std::multiset<std::string> lined;

	while (std::getline(in, line))
	{
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}

		if (!line.empty() && line[0] != '#')
		{
			lined.insert(line.substr(0, line.find('\t')));
		}
	}

	std::multiset<std::string> const wrote(one.written.begin(), one.written.end());

	say(lined == wrote, "shapes.txt has one line for each file written (" + std::to_string(lined.size()) + " lines, " + std::to_string(wrote.size()) + " files)");

	// SECOND RUN: nothing written, every shape already there
	Pass const two = RunPass(stock, scratch);

	say(two.read == 86 && two.failed == 0 && two.written.empty(), "second run: " + std::to_string(two.read) + " maps read, " + std::to_string(two.failed) +
		" failed, " + std::to_string(two.written.size()) + " written (" + first(two.written) + "), in " + std::to_string(two.ms) + " ms");
	say(two.alreadyThere.size() == one.written.size() + one.alreadyThere.size() && two.skipped.size() == one.skipped.size(), "and every shape already there: " +
		std::to_string(two.alreadyThere.size()) + " (the first run's " + std::to_string(one.written.size()) + " written and " + std::to_string(one.alreadyThere.size()) +
		" already there), the same " + std::to_string(two.skipped.size()) + " skipped");
	printf(wrong ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return wrong ? 1 : 0;
}

// "stock": the editor's own stock check on the stock copy must find every file Steam's own, and nothing else
static int StockMode(fs::path const &stock)
{
	ShapeStockCheck const check = ShapeExtract_StockCheck(stock.string());
	int wrong = 0;
	auto say = [&wrong](bool ok, std::string const &what)
	{
		printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
		wrong += ok ? 0 : 1;
	};
	auto first = [](std::vector<std::string> const &list)
	{
		return list.empty() ? std::string("none") : list[0] + (list.size() > 1 ? " and " + std::to_string(list.size() - 1) + " more" : std::string());
	};

	say(check.levels == 63 && check.levelsOwn == 63, "avp_rifs: 63 levels read, all 63 Steam's own (" + std::to_string(check.levels) + " read, " +
		std::to_string(check.levelsOwn) + " own)");
	say(check.huds == 23 && check.hudsOwn == 23, "avp_huds: 23 HUD files read, all 23 Steam's own (" + std::to_string(check.huds) + " read, " +
		std::to_string(check.hudsOwn) + " own)");
	say(check.missing.empty(), "no stock file missing (" + first(check.missing) + ")");
	say(check.notOwn.empty(), "none named as a stock file but of other bytes (" + first(check.notOwn) + ")");
	say(check.refused.empty(), "none refused (" + first(check.refused) + ")");
	printf(wrong ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return wrong ? 1 : 0;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc != 3 && argc != 4)
	{
		return Usage();
	}

	std::string const mode = argv[1];
	fs::path stock, anchors;

	if (mode == "endtoend" && argc == 4)
	{
		return WholeFolder(argv[2], &stock) ? EndToEndMode(stock, argv[3]) : Usage();
	}

	if ((mode != "make" && mode != "check" && !(mode == "stock" && argc == 3)) || !WholeFolder(argv[2], &stock) || (argc == 4 && !WholeFolder(argv[3], &anchors)))
	{
		fprintf(stderr, "refused: the mode is make, check or stock, and each folder must exist, be a folder, not a link, with no \"..\"\n");
		return Usage();
	}

	if (mode == "stock")
	{
		return StockMode(stock);
	}

	Made made;

	if (!MakeTable(stock, argc == 4 ? &anchors : nullptr, &made))
	{
		return 1;
	}

	if (mode == "check")
	{
		// the table built into this program against the one made now, row by row
		int wrong = 0;
		bool const sameCounts = made.stock.size() == kShapeIdentityStockFileCount && made.rows.size() == kShapeIdentityRowCount;

		if (!sameCounts)
		{
			printf("  FAILED  the table built in has %zu stock files and %zu rows; made now: %zu and %zu\n", kShapeIdentityStockFileCount, kShapeIdentityRowCount,
				made.stock.size(), made.rows.size());
			wrong++;
		}

		for (size_t i = 0; sameCounts && i < made.stock.size(); i++)
		{
			ShapeIdentityStockFile const &s = kShapeIdentityStockFiles[i];
			bool const same = made.stockFolders[i] == s.folder && made.stockNames[i] == s.fileName && made.stock[i].size == s.size && made.stockSums[i] == s.sha256;

			if (!same && wrong++ < 5)
			{
				printf("  FAILED  stock file %zu: built in %s/%s, made now %s/%s\n", i, s.folder, s.fileName, made.stockFolders[i].c_str(), made.stockNames[i].c_str());
			}
		}

		for (size_t i = 0; sameCounts && i < made.rows.size(); i++)
		{
			ShapeIdentityRow const &r = kShapeIdentityRows[i];
			Made::Row const &m = made.rows[i];
			ShapeIdentityPlacement const &mp = m.placement, &rp = r.placement;
			bool const samePlacement = mp.known == rp.known && mp.type == rp.type && mp.extra == rp.extra && mp.mass == rp.mass && mp.integrity == rp.integrity &&
				mp.flags == rp.flags && mp.same == rp.same && mp.of == rp.of;
			bool const same = m.sha256 == r.sha256 && m.fileName == r.fileName && m.kind == r.kind && m.evidence == r.evidence && m.anchored == r.anchored && m.levels == r.levels &&
				samePlacement;

			if (!same && wrong++ < 5)
			{
				printf("  FAILED  row %zu: built in %s %s, made now %s %s\n", i, r.fileName, r.sha256, m.fileName.c_str(), m.sha256.c_str());
			}
		}

		printf("  %s  the table built in is the one the stock copy makes (%zu stock files, %zu rows)\n", wrong ? "FAILED" : "ok", made.stock.size(), made.rows.size());

		// THE OWNER'S MEDPACK, end to end: medbox01 made from stock Derelict.RIF on its own (as the editor will, on a player's machine), hashed, and looked up
		// in the table built in: it must be known, a health pickup, anchored on Rebellion's own medbox01.rif. The same file with one byte changed is UNKNOWN
		// (exact matches only: "we don't want to say 'Oh doh, your medpack is this chair'").
		Bytes raw, level, file;
		std::vector<ShapeExtractPlaced> placed;
		std::string name;
		bool made1 = ReadAll(stock / "avp_rifs" / "Derelict.RIF", &raw) && Unpacked(raw, &level) && ShapeExtract_Placed(level, &placed);
		bool extracted = false;

		for (ShapeExtractPlaced const &p : placed)
		{
			if (!extracted && made1 && Lower(p.fileName) == "medbox01.rif" && p.rescale == 1.0)
			{
				extracted = ShapeExtract_File(level, p, &file, &name, nullptr) == ShapeExtractResult::Done;
			}
		}

		ShapeIdentityRow const *row = extracted ? ShapeIdentity_Find(ShapeExtract_Sha256(file)) : nullptr;
		bool const medpack = row && std::string(row->fileName) == "medbox01.rif" && std::string(row->kind) == "health" && row->anchored;
		printf("  %s  medbox01 made from stock Derelict.RIF on its own is known: %s\n", medpack ? "ok" : "FAILED",
			row ? (std::string(row->fileName) + ", " + row->kind + " (" + row->evidence + ")" + (row->anchored ? ", anchored" : ", not anchored")).c_str() :
			extracted ? "its SHA-256 is in no row" : "it couldn't be made");

		if (!file.empty())
		{
			file[file.size() / 2] ^= 1;
		}

		bool const unknown = !file.empty() && ShapeIdentity_Find(ShapeExtract_Sha256(file)) == nullptr;
		printf("  %s  the same file with one byte changed is unknown\n", unknown ? "ok" : "FAILED");
		wrong += medpack ? 0 : 1;
		wrong += unknown ? 0 : 1;

		// THE ONE-MAP CALL ON EVERY STOCK FILE (the owner's rule: each map scanned as its file holds it): every one taken (packed, its length exact), and each
		// of its shapes one of the table's rows, as the whole scan made them
		size_t taken = 0, known = 0, shapes = 0;
		std::string firstRefused;
		std::error_code e;

		for (char const *folder : { "avp_rifs", "avp_huds" })
		{
			for (fs::directory_iterator it(stock / folder, e), end; !e && it != end; it.increment(e))
			{
				Bytes bytes;

				if (Lower(it->path().extension().string()) != ".rif" || !ReadAll(it->path(), &bytes))
				{
					continue;
				}

				ShapeExtractMap const map = ShapeExtract_ScanMap(it->path().filename().string(), bytes);
				taken += map.scanned ? 1 : 0;
				firstRefused = !map.scanned && firstRefused.empty() ? it->path().filename().string() + ": " + map.why : firstRefused;

				for (ShapeExtractShape const &s : map.shapes)
				{
					shapes++;
					known += ShapeIdentity_Find(s.sha256) ? 1 : 0;
				}
			}
		}

		bool const allTaken = taken == made.stock.size() && known == shapes && shapes > 0;
		printf("  %s  the one-map call takes every stock file (%zu of %zu%s), and every shape it gives is a row of the table (%zu of %zu)\n", allTaken ? "ok" : "FAILED", taken,
			made.stock.size(), firstRefused.empty() ? "" : ("; first refused " + firstRefused).c_str(), known, shapes);
		wrong += allTaken ? 0 : 1;
		return wrong ? 1 : 0;
	}

	// MAKE: the table's source text
	// (built whole, then written in one go with WriteFile: the C library's text mode would end each line with CR LF, and the repository keeps plain line
	// ends, .gitattributes; avpx's own win95/io.h hides the C library's _setmode from this build)
	std::string text;
	Out(&text, "// shape_identity_data.cpp: the table of shape_identity.h. MADE by tests/scenarios/verify_shape_identities make; never edit it by hand (verify_shape_identities\n");
	Out(&text, "// check says whether it is still what the stock copy makes). From a copy of Steam's avp_rifs and avp_huds checked against Steam's depot manifests\n");
	Out(&text, "// outside the repo (AvP-docs tools/frost/stock-check/verify-stock-shapes.js; the copy's PROVENANCE.txt names it, the manifests and the date). Sizes and\n");
	Out(&text, "// SHA-256 only: SHA-1 stays out of the repo (the owner, 2026-11-23). Each row: the extracted file's SHA-256, its name, the commonest kind of the stock\n");
	Out(&text, "// objects placing exactly that variant with its evidence, whether its geometry is exactly one of Rebellion's own files' (anchored), its levels,\n");
	Out(&text, "// and its commonest stock placement: known, Type, ExtraData, mass, integrity word, flags word, how many have that set, of how many of its kind.\n");
	Out(&text, "// A RESCALED copy has no row (its points are the file's scaled, so it isn't the file: Bishop 2's ruling, 11-23): the names placed only rescaled, and\n");
	Out(&text, "// parts such as office.rif's Game and Mousemat of offbits.rif (rescale 0.7), are unknown, so an object of one shows as unknown (Vasquez's question).\n");
	Out(&text, "\n#include \"shape_identity.h\"\n\nnamespace host\n{\n\tShapeIdentityStockFile const kShapeIdentityStockFiles[] =\n\t{\n");

	for (size_t i = 0; i < made.stock.size(); i++)
	{
		std::string name;

		if (!Quoted(made.stockNames[i], &name))
		{
			fprintf(stderr, "FAILED: a stock file's name has a letter a table can't hold: %zu\n", i);
			return 1;
		}

		Out(&text, "\t\t{ \"%s\", %s, %llu, \"%s\" },\n", made.stockFolders[i].c_str(), name.c_str(), (unsigned long long)made.stock[i].size, made.stockSums[i].c_str());
	}

	Out(&text, "\t};\n\n\tsize_t const kShapeIdentityStockFileCount = sizeof(kShapeIdentityStockFiles) / sizeof(kShapeIdentityStockFiles[0]);\n\n");
	Out(&text, "\tShapeIdentityRow const kShapeIdentityRows[] =\n\t{\n");

	for (Made::Row const &row : made.rows)
	{
		std::string name, kind, evidence, levels;

		if (!Quoted(row.fileName, &name) || !Quoted(row.kind, &kind) || !Quoted(row.evidence, &evidence) || !Quoted(row.levels, &levels))
		{
			fprintf(stderr, "FAILED: a row has a letter a table can't hold: %s\n", row.fileName.c_str());
			return 1;
		}

		ShapeIdentityPlacement const &p = row.placement;

		Out(&text, "\t\t{ \"%s\", %s, %s, %s, %s, %s, { %s, %d, %d, %d, 0x%08xu, 0x%xu, %uu, %uu } },\n", row.sha256.c_str(), name.c_str(), kind.c_str(), evidence.c_str(),
			row.anchored ? "true" : "false", levels.c_str(), p.known ? "true" : "false", p.type, p.extra, p.mass, p.integrity, p.flags, p.same, p.of);
	}

	Out(&text, "\t};\n\n\tsize_t const kShapeIdentityRowCount = sizeof(kShapeIdentityRows) / sizeof(kShapeIdentityRows[0]);\n}\n");
	DWORD wrote = 0;
	bool const written = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text.data(), (DWORD)text.size(), &wrote, nullptr) && wrote == text.size();
	return written ? 0 : 1;
}
