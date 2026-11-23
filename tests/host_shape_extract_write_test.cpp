// The test for the writer of src/host/shape_extract_files.h (ShapeExtract_WriteMapShapes, ShapeExtract_LevelToFolder): a map's shapes written into the
// game's shape_rifs, never over a file, with their lines in shapes.txt. Real folders, in one this test makes in the temp folder and takes away again:
//   - a new folder is made, the shape written, its bytes the scan's, its line in shapes.txt (with the file's heading);
//   - the SAME map again writes nothing ("already there"), and shapes.txt keeps its lines;
//   - THE OWNER'S NAMING RULE across maps: another map's other bytes of the same name are <name>_<map>; a third map holding those very bytes writes nothing;
//   - a folder where the name would go: the name is taken, the next is used;
//   - REFUSED before anything is written, each in its own words: the folder a junction; the folder the old tools' Tools\shape_rifs; shapes.txt a hard link;
//     a map that isn't one;
//   - the one call per map (ShapeExtract_LevelToFolder) on a map's file: read, its SHA-256 the file's, the shape written; a missing map read = false.
// Written 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <new>
#include <string>
#include <vector>

#include <windows.h>

#include "make_junction.h"   // a junction made without a shell (Vasquez)
#include "shape_extract.h"
#include "shape_extract_files.h"
#include "shape_import.h"
#include "shape_extract_fixtures.h"   // PackageFile, LevelOf (the maps both Option C tests build)

// the game's memory code (our_mem.cpp, for the decoder shape_extract.cpp uses) calls these: here they only say what happened
void avp_MessageBox(const char *message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

// THE OUT-OF-MEMORY SWITCH (Vasquez's MUST 2, 11-23): when armed with N, the Nth allocation of at least kBig bytes from then on throws std::bad_alloc, once,
// as a 32-bit editor's would when a map at the limits needs 250-300 MB. Off (0), every allocation is malloc's.
static int failBigAllocationIn = 0;
static size_t const kBig = 64 * 1024;

static void *Allocate(size_t size)
{
	if (failBigAllocationIn > 0 && size >= kBig && --failBigAllocationIn == 0)
	{
		throw std::bad_alloc();
	}

	void *const p = malloc(size ? size : 1);

	if (!p)
	{
		throw std::bad_alloc();
	}

	return p;
}

void *operator new(size_t size)
{
	return Allocate(size);
}

void *operator new[](size_t size)
{
	return Allocate(size);
}

// The ONE delete, giving malloc's memory back to free. The other three (delete[], and the two that are also told the size) are NOT written here: the C++
// standard's own versions of them each call this one (MSVC's vcruntime delete_array.cpp and delete_scalar_size.cpp do just that), so every delete still
// ends here, once. (Written out, each of the three was one more free(p) that Snyk Code read as a possible second free of the same memory:
// cpp/DoubleFree, 3 notes on 59675b87.)
void operator delete(void *p) noexcept
{
	free(p);
}

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static std::wstring Wide(std::string const &narrow)
{
	int const letters = MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), (int)narrow.size(), nullptr, 0);
	std::wstring wide(letters > 0 ? (size_t)letters : 0, L'\0');

	if (letters > 0)
	{
		MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), (int)narrow.size(), &wide[0], letters);
	}

	return wide;
}

static bool Write(std::string const &path, std::vector<char> const &bytes)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	DWORD wrote = 0;
	bool const good = file != INVALID_HANDLE_VALUE && WriteFile(file, bytes.data(), (DWORD)bytes.size(), &wrote, nullptr) && wrote == bytes.size();

	if (file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
	}

	return good;
}

static std::vector<char> ReadAll(std::string const &path)
{
	std::vector<char> bytes;
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		return bytes;
	}

	LARGE_INTEGER size;

	if (GetFileSizeEx(file, &size) && size.QuadPart < 64 * 1024 * 1024)
	{
		bytes.resize((size_t)size.QuadPart);
		DWORD read = 0;

		if (!bytes.empty() && (!ReadFile(file, bytes.data(), (DWORD)bytes.size(), &read, nullptr) || read != bytes.size()))
		{
			bytes.clear();
		}
	}

	CloseHandle(file);
	return bytes;
}

static bool Exists(std::string const &path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// the lines of a text file
static std::vector<std::string> LinesOf(std::string const &path)
{
	std::vector<char> const bytes = ReadAll(path);
	std::vector<std::string> lines;
	std::string line;

	for (char c : bytes)
	{
		if (c == '\n')
		{
			lines.push_back(line);
			line.clear();
		}
		else if (c != '\r')
		{
			line += c;
		}
	}

	return lines;
}

static std::string Joined(std::vector<std::string> const &names)
{
	std::string out;

	for (std::string const &n : names)
	{
		out += (out.empty() ? "" : ",") + n;
	}

	return out;
}

// the test's own folder taken away, a level at a time (only names this test made: files, folders, and junctions taken away as themselves)
static void TakeAway(std::string const &folder)
{
	WIN32_FIND_DATAA found;
	HANDLE const search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search != INVALID_HANDLE_VALUE)
	{
		do
		{
			std::string const name = found.cFileName;

			if (name == "." || name == "..")
			{
				continue;
			}

			std::string const path = folder + "\\" + name;

			if (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
			{
				RemoveDirectoryA(path.c_str());   // a junction: itself, never what it points at
			}
			else if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				TakeAway(path);
			}
			else
			{
				DeleteFileA(path.c_str());
			}
		} while (FindNextFileA(search, &found));

		FindClose(search);
	}

	RemoveDirectoryA(folder.c_str());
}

int main()
{
	puts("the shape writer");

	char temp[MAX_PATH];
	DWORD const length = GetTempPathA(MAX_PATH, temp);

	if (length == 0 || length >= MAX_PATH)
	{
		printf("  FAILED  no temp folder: nothing was tested\n");
		return 1;
	}

	std::string const root = std::string(temp) + "shape_extract_write_test_" + std::to_string((unsigned long)GetCurrentProcessId());
	Check(CreateDirectoryA(root.c_str(), nullptr) != 0, "the test's folder is made in the temp folder: " + root);

	// THE MAPS: medbox01 as the package holds it, imported into a level (Derelict); another medbox01, one polygon byte changed (Genshd1, and Hangar again)
	Bytes placed;
	ShapeImport_Build(PackageFile(), "medbox01.rif", 12, &placed, nullptr);
	Bytes variant = placed;
	{
		std::vector<Span> p;
		Children(variant, kHeadingBytes, variant.size(), &p);
		variant[p[FirstNamed(variant, p, "SHPPOLYS")].at + kHeadingBytes] ^= 0x40;
	}

	Bytes const derelict = LevelOf({ placed }), genshd1 = LevelOf({ variant });
	ShapeExtractMap const a = ShapeExtract_ScanMap("Derelict.RIF", derelict), b = ShapeExtract_ScanMap("Genshd1.RIF", genshd1), c = ShapeExtract_ScanMap("Hangar.rif", genshd1);
	Check(a.scanned && a.shapes.size() == 1 && b.scanned && b.shapes.size() == 1 && a.shapes[0].file != b.shapes[0].file, "two maps, each with a medbox01 of its own bytes");

	if (!(a.scanned && a.shapes.size() == 1 && b.scanned && b.shapes.size() == 1))
	{
		TakeAway(root);
		printf("SOME CHECKS FAILED\n");
		return 1;
	}

	// A NEW FOLDER: made, the shape written, its line in shapes.txt
	std::string const target = root + "\\shape_rifs";
	ShapeLevelReport r1;
	bool const ok1 = ShapeExtract_WriteMapShapes(a, "Derelict.RIF", target, &r1);
	Check(ok1 && !r1.failed && Joined(r1.written) == "medbox01.rif" && r1.alreadyThere.empty() && r1.sha256 == a.sha256,
		"into a folder that isn't there yet: it is made, and medbox01.rif written (" + Joined(r1.written) + ") " + r1.why);
	Check(ReadAll(target + "\\medbox01.rif") == a.shapes[0].file, "the file on disk is the scan's very bytes");
	std::vector<std::string> const lines = LinesOf(target + "\\shapes.txt");
	std::string const wanted = "medbox01.rif\t" + a.shapes[0].sha256 + "\tunknown\t\tDerelict.RIF\t" + a.sha256 + "\tmedbox01.rif";
	Check(lines.size() == 5 && lines[0].compare(0, 13, "# shapes.txt:") == 0 && lines[4] == wanted,
		"shapes.txt: its 4-line heading, then the file's line: name, SHA-256, \"unknown\" (not a stock shape), the map, the map's SHA-256, the name it gives");

	// THE SAME MAP AGAIN: nothing written, shapes.txt as it was
	ShapeLevelReport r2;
	ShapeExtract_WriteMapShapes(a, "Derelict.RIF", target, &r2);
	Check(!r2.failed && r2.written.empty() && Joined(r2.alreadyThere) == "medbox01.rif" && LinesOf(target + "\\shapes.txt").size() == 5,
		"the same map again: nothing written, medbox01.rif already there, shapes.txt keeps its 5 lines");

	// ANOTHER MAP, OTHER BYTES OF THE SAME NAME: <name>_<map>; a third map with those very bytes: nothing
	ShapeLevelReport r3, r4;
	ShapeExtract_WriteMapShapes(b, "Genshd1.RIF", target, &r3);
	Check(!r3.failed && Joined(r3.written) == "medbox01_genshd1.rif" && ReadAll(target + "\\medbox01_genshd1.rif") == b.shapes[0].file &&
		ReadAll(target + "\\medbox01.rif") == a.shapes[0].file, "Genshd1's other medbox01 is medbox01_genshd1.rif, and medbox01.rif is left as it was");
	ShapeExtract_WriteMapShapes(c, "Hangar.rif", target, &r4);
	Check(!r4.failed && r4.written.empty() && Joined(r4.alreadyThere) == "medbox01_genshd1.rif" && !Exists(target + "\\medbox01_hangar.rif"),
		"Hangar holding Genshd1's very bytes writes nothing: they are there already as medbox01_genshd1.rif (" + Joined(r4.alreadyThere) + ")");

	// A FOLDER WHERE THE NAME WOULD GO: taken, so the next name
	std::string const second = root + "\\second";
	CreateDirectoryA(second.c_str(), nullptr);
	CreateDirectoryA((second + "\\medbox01.rif").c_str(), nullptr);
	ShapeLevelReport r5;
	ShapeExtract_WriteMapShapes(a, "Derelict.RIF", second, &r5);
	Check(!r5.failed && Joined(r5.written) == "medbox01_derelict.rif", "a folder named medbox01.rif there: the name is taken, medbox01_derelict.rif is written (" +
		Joined(r5.written) + ")");

	// REFUSED before anything is written
	std::string const aim = root + "\\aim", junction = root + "\\junction";
	CreateDirectoryA(aim.c_str(), nullptr);
	bool const joined = MakeJunction(Wide(junction), Wide(aim)) && (GetFileAttributesA(junction.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
	ShapeLevelReport r6;
	bool const ok6 = ShapeExtract_WriteMapShapes(a, "Derelict.RIF", junction, &r6);
	Check(joined && !ok6 && r6.failed && r6.why.find("a link or a junction") != std::string::npos && !Exists(aim + "\\medbox01.rif"),
		"the folder a junction: REFUSED, \"" + r6.why + "\", nothing written where it points");

	std::string const tools = root + "\\Tools", package = tools + "\\shape_rifs";
	CreateDirectoryA(tools.c_str(), nullptr);
	CreateDirectoryA(package.c_str(), nullptr);
	ShapeLevelReport r7;
	bool const ok7 = ShapeExtract_WriteMapShapes(a, "Derelict.RIF", package, &r7);
	Check(!ok7 && r7.why.find("old tools' package") != std::string::npos && !Exists(package + "\\medbox01.rif"),
		"the folder Tools\\shape_rifs (the old tools' package, the owner's): REFUSED, \"" + r7.why + "\"");

	std::string const linked = root + "\\linked";
	CreateDirectoryA(linked.c_str(), nullptr);
	bool const twoNames = Write(linked + "\\shapes.txt", { '#', '\r', '\n' }) && CreateHardLinkA((linked + "\\other.txt").c_str(), (linked + "\\shapes.txt").c_str(), nullptr);
	ShapeLevelReport r8;
	bool const ok8 = ShapeExtract_WriteMapShapes(a, "Derelict.RIF", linked, &r8);
	Check(twoNames && !ok8 && r8.why.find("shapes.txt is a link") != std::string::npos && !Exists(linked + "\\medbox01.rif"),
		"shapes.txt one of a hard link's two names: REFUSED before any shape is written, \"" + r8.why + "\"");

	ShapeLevelReport r9;
	ShapeExtractMap const junk = ShapeExtract_ScanMap("x.rif", Filled(400, 50));
	bool const ok9 = ShapeExtract_WriteMapShapes(junk, "x.rif", root + "\\third", &r9);
	Check(!ok9 && r9.failed && r9.why == junk.why && !Exists(root + "\\third"), "a map that isn't one: failed with the scan's words (\"" + r9.why + "\"), no folder made");

	// A PART (Bishop 2's ruling A): written under the game's own name made plain, never the whole file's; the same bytes met later as the whole file count as
	// there already
	std::string const parts = root + "\\parts";
	ShapeExtractMap const asPart = ShapeExtract_ScanMap("Hangar.rif", LevelOf({ AsAPart(placed, "drum") }));
	ShapeLevelReport r12, r13;
	ShapeExtract_WriteMapShapes(asPart, "Hangar.rif", parts, &r12);
	Check(!r12.failed && Joined(r12.written) == "drum_medbox01.rif" && !Exists(parts + "\\medbox01.rif") && ReadAll(parts + "\\drum_medbox01.rif") == a.shapes[0].file,
		"a part (EXTOBJNM drum) is written as drum_medbox01.rif, never medbox01.rif (" + Joined(r12.written) + ")");
	ShapeExtract_WriteMapShapes(a, "Derelict.RIF", parts, &r13);
	Check(!r13.failed && r13.written.empty() && Joined(r13.alreadyThere) == "drum_medbox01.rif", "the same bytes as the whole file, from another map, are there already (" +
		Joined(r13.alreadyThere) + "), nothing written");

	// THE WORK, KEPT IN STEP (Vasquez's MUST, 11-23): a map of 1,024 parts of one file, each other bytes of one size, written into an empty folder, in
	// seconds (the writer before it listed the folder and read back every file it had written for every new shape: about K^2/2 reads)
	{
		std::vector<Bytes> parts1024;

		for (int i = 0; i < 1024; i++)
		{
			Bytes v = placed;
			std::vector<Span> p;
			Children(v, kHeadingBytes, v.size(), &p);
			size_t const polys = p[FirstNamed(v, p, "SHPPOLYS")].at + kHeadingBytes;
			v[polys] = (char)(i & 0xff);
			v[polys + 1] = (char)((i >> 8) & 0xff);
			parts1024.push_back(AsAPart(v, ("obj" + std::to_string(i)).c_str()));
		}

		ShapeExtractMap const many = ShapeExtract_ScanMap("Many.rif", LevelOf(parts1024));
		std::string const manyFolder = root + "\\many";
		ShapeLevelReport r14;
		ULONGLONG const start = GetTickCount64();
		ShapeExtract_WriteMapShapes(many, "Many.rif", manyFolder, &r14);   // (most of its time is the disk: each new file is flushed, about 16 ms here)
		ULONGLONG const ms = GetTickCount64() - start;
		Check(many.scanned && !r14.failed && r14.written.size() == 1024 && ms < 60000, "1,024 parts of one file, all other bytes, written into an empty folder in " +
			std::to_string(ms) + " ms (under 60 s; each file flushed to the disk): " + std::to_string(r14.written.size()) + " written " + r14.why);

		// THE COUNT, which no disk's speed can move (Vasquez's nit 3): into the empty folder no file was read at all; the same map again reads each of its
		// 1,024 files once (each of the shape's size), and writes nothing
		Check(r14.filesRead == 0, "into an empty folder, no file of it read (" + std::to_string(r14.filesRead) + ")");
		ShapeLevelReport r18;
		ShapeExtract_WriteMapShapes(many, "Many.rif", manyFolder, &r18);
		Check(!r18.failed && r18.written.empty() && r18.alreadyThere.size() == 1024 && r18.filesRead == 1024,
			"the same map again: 1,024 already there, each file read once (" + std::to_string(r18.filesRead) + " read)");
	}

	// OUT OF MEMORY (Vasquez's MUST 2): anywhere in the one call per map, it fails the MAP in words, never the editor; report.read says whether the bytes were
	// read. And in the stock check, the same.
	{
		std::vector<Bytes> big;

		for (int i = 0; i < 200; i++)
		{
			Bytes v = placed;
			std::vector<Span> p;
			Children(v, kHeadingBytes, v.size(), &p);
			v[p[FirstNamed(v, p, "SHPPOLYS")].at + kHeadingBytes] = (char)i;
			big.push_back(v);
		}

		Bytes const bigMap = LevelOf(big);
		std::string const bigFile = root + "\\BigMap.rif", memoryFolder = root + "\\memory";
		Write(bigFile, bigMap);
		bool escaped = false;

		for (int nth : { 1, 2 })
		{
			ShapeLevelReport r;
			bool ok = true;

			try
			{
				failBigAllocationIn = nth;   // 1: the read's buffer; 2: the map's copy for the scan, after the read
				ok = ShapeExtract_LevelToFolder(bigFile, memoryFolder, &r);
			}
			catch (std::bad_alloc const &)
			{
				escaped = true;
			}

			failBigAllocationIn = 0;
			Check(!escaped && !ok && r.failed && r.why.find("not enough memory") != std::string::npos && r.read == (nth == 2),
				std::string("out of memory ") + (nth == 1 ? "in the read" : "after the read, in the scan") + ": the map fails in words (\"" + r.why + "\"), read " +
				(r.read ? "true" : "false") + (escaped ? "; THE EXCEPTION ESCAPED (it would end the editor)" : ""));
			escaped = false;
		}

		// the same from bytes read already (ShapeExtract_BytesToFolder, shapes part 3): out of memory in its scan fails the map in words too
		{
			ShapeLevelReport r;
			bool ok = true;

			try
			{
				failBigAllocationIn = 1;   // the map's copy for the scan: the first big block it asks for
				ok = ShapeExtract_BytesToFolder("BigMap.rif", bigMap, memoryFolder, &r);
			}
			catch (std::bad_alloc const &)
			{
				escaped = true;
			}

			failBigAllocationIn = 0;
			Check(!escaped && !ok && r.failed && r.why.find("not enough memory") != std::string::npos && r.read, std::string("ShapeExtract_BytesToFolder out of ") +
				"memory in the scan: the map fails in words (\"" + r.why + "\")" + (escaped ? "; THE EXCEPTION ESCAPED (it would end the editor)" : ""));
			escaped = false;
		}

		std::string const data = root + "\\memorydata";
		CreateDirectoryA(data.c_str(), nullptr);
		CreateDirectoryA((data + "\\avp_rifs").c_str(), nullptr);
		Write(data + "\\avp_rifs\\Derelict.RIF", bigMap);
		ShapeStockCheck check;

		try
		{
			failBigAllocationIn = 1;
			check = ShapeExtract_StockCheck(data);
		}
		catch (std::bad_alloc const &)
		{
			escaped = true;
		}

		failBigAllocationIn = 0;
		bool said = false;

		for (std::string const &r : check.refused)
		{
			said = said || r.find("not enough memory") != std::string::npos;
		}

		Check(!escaped && said, std::string("out of memory in the stock check: said in its refusals, never the end of the editor") +
			(escaped ? "; THE EXCEPTION ESCAPED" : ""));
	}

	// shapes.txt's fields can't be split by a map's own bytes: a TAB in a part's name (a stranger's map) is a space in its field
	{
		std::string const tabs = root + "\\tabs";
		ShapeExtractMap const tabbed = ShapeExtract_ScanMap("Tab.rif", LevelOf({ AsAPart(placed, "Ob\tject") }));
		ShapeLevelReport r15;
		ShapeExtract_WriteMapShapes(tabbed, "Tab.rif", tabs, &r15);
		std::vector<std::string> const lines15 = LinesOf(tabs + "\\shapes.txt");
		size_t const fields = lines15.size() == 5 ? (size_t)std::count(lines15[4].begin(), lines15[4].end(), '\t') + 1 : 0;
		Check(!r15.failed && fields == 7 && lines15[4].find("Ob ject@medbox01.rif") != std::string::npos,
			"a part named with a TAB: its shapes.txt line still has its 7 fields, the tab a space (" + std::to_string(fields) + " fields)");
	}

	// the old tools' package, named another way Windows takes for the same folder (a trailing dot): still REFUSED, by the folder's own final path
	{
		ShapeLevelReport r16;
		bool const ok16 = ShapeExtract_WriteMapShapes(a, "Derelict.RIF", package + ".", &r16);
		Check(!ok16 && r16.why.find("old tools' package") != std::string::npos && !Exists(package + "\\medbox01.rif"),
			"Tools\\shape_rifs named with a trailing dot: REFUSED by the folder's final path (\"" + r16.why + "\")");
	}

	// WHOLE OR NOT AT ALL (Drake's question, 11-23): a shape is written into a ".part" file and renamed to its name only once it is whole and flushed, so an
	// editor killed part way leaves no half <name>.rif. After a write no .part is left; one left by a killed editor (not a .rif) blocks nothing and is
	// never read as a shape.
	{
		std::string const whole = root + "\\whole";
		CreateDirectoryA(whole.c_str(), nullptr);
		Write(whole + "\\~1-1.part", { 'h', 'a', 'l', 'f' });   // as a killed editor would leave it
		ShapeLevelReport r17;
		ShapeExtract_WriteMapShapes(a, "Derelict.RIF", whole, &r17);
		WIN32_FIND_DATAA found;
		HANDLE const search = FindFirstFileA((whole + "\\*.part").c_str(), &found);
		int parts = 0;

		if (search != INVALID_HANDLE_VALUE)
		{
			do
			{
				parts++;
			} while (FindNextFileA(search, &found));

			FindClose(search);
		}

		Check(!r17.failed && Joined(r17.written) == "medbox01.rif" && ReadAll(whole + "\\medbox01.rif") == a.shapes[0].file && parts == 1 &&
			ReadAll(whole + "\\~1-1.part") == std::vector<char>({ 'h', 'a', 'l', 'f' }),
			"written whole under its name; the only .part left is the killed editor's, untouched and not read (" + std::to_string(parts) + " .part)");
	}

	// THE ONE CALL PER MAP, on a map's file
	std::string const mapFile = root + "\\Derelict.RIF", fourth = root + "\\fourth";
	ShapeLevelReport r10, r11;
	bool const ok10 = Write(mapFile, derelict) && ShapeExtract_LevelToFolder(mapFile, fourth, &r10);
	Check(ok10 && r10.read && r10.sha256 == ShapeExtract_Sha256(derelict) && Joined(r10.written) == "medbox01.rif" && ReadAll(fourth + "\\medbox01.rif") == a.shapes[0].file,
		"ShapeExtract_LevelToFolder: the map's file read, its SHA-256 the file's, medbox01.rif written (" + Joined(r10.written) + ") " + r10.why);
	bool const ok11 = ShapeExtract_LevelToFolder(root + "\\Missing.rif", fourth, &r11);
	Check(!ok11 && !r11.read && r11.failed && r11.why.find("missing") != std::string::npos, "a map that isn't there: not read, failed, \"" + r11.why + "\"");

	// THE SAME FROM BYTES READ ALREADY (ShapeExtract_BytesToFolder, shapes part 3): the bytes the caller hashed are the ones searched; no file is read
	{
		std::string const fifth = root + "\\fifth";
		ShapeLevelReport r18;
		bool const ok18 = ShapeExtract_BytesToFolder("Derelict.RIF", derelict, fifth, &r18);
		std::vector<std::string> const lines18 = LinesOf(fifth + "\\shapes.txt");
		Check(ok18 && r18.read && r18.sha256 == ShapeExtract_Sha256(derelict) && Joined(r18.written) == "medbox01.rif" &&
			ReadAll(fifth + "\\medbox01.rif") == a.shapes[0].file && lines18.size() == 5 && lines18[4].find(ShapeExtract_Sha256(derelict)) != std::string::npos &&
			lines18[4].find("Derelict.RIF") != std::string::npos, "ShapeExtract_BytesToFolder: medbox01.rif written from the bytes given, its shapes.txt line naming "
			"Derelict.RIF and those bytes' SHA-256 (" + Joined(r18.written) + ") " + r18.why);
	}

	TakeAway(root);
	Check(!Exists(root), "and the test's folder is taken away");
	printf(checksFailed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return checksFailed ? 1 : 0;
}
