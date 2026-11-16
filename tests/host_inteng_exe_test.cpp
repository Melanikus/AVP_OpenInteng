// The test of the run-time reader of the user's inteng.exe (inteng_exe.h) and of inteng's built-in shapes read through it (inteng_shapes.h): object-placement.md section 8b V-25 and Melanikus's
// ruling via Bishop 11-16 (inteng's binary data is read at run time, never kept in our source). Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// It makes small 32-bit programs of its own, byte by byte (a DOS header, the PE headers, one section at inteng's .data addresses), so every rule and every refusal is tried on bytes it knows:
// the sha256 against the standard's own examples, every header check, the section table, a read that runs off a section, the loader's refusals (not there, a folder, the wrong hash), and every
// refusal of a shape: a slot outside 0..18, a header outside the file, no points, too many polygons, a corner that is not a point, not a multiple of 3, fewer than 3 corners, no -1.
#include "inteng_exe.h"
#include "inteng_shapes.h"

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

using namespace host;

typedef std::vector<uint8_t> Bytes;

static int checksRun = 0, checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checksRun++;

	if (!good)
	{
		checksFailed++;
		printf("  FAILED  %s\n", what.c_str());
	}
}

static void Put32(Bytes *b, size_t at, uint32_t v) { for (int i = 0; i < 4; i++) (*b)[at + i] = (uint8_t)(v >> (8 * i)); }
static void Put16(Bytes *b, size_t at, uint32_t v) { (*b)[at] = (uint8_t)v; (*b)[at + 1] = (uint8_t)(v >> 8); }

// A program with one section: the image base 0x400000, the section at 0x5c3000 (inteng's .data), `sectionBytes` long, its bytes at 0x400 in the file
static uint32_t const kBase = 0x400000, kSectionVa = 0x5c3000, kRawAt = 0x400;
static uint32_t const kSectionBytes = 0x30000;

static Bytes Program(uint32_t sectionBytes = kSectionBytes)
{
	Bytes b(kRawAt + sectionBytes, 0);

	b[0] = 'M';
	b[1] = 'Z';
	Put32(&b, 0x3c, 0x40);
	memcpy(&b[0x40], "PE\0\0", 4);
	Put16(&b, 0x44, 0x14c);         // i386
	Put16(&b, 0x46, 1);             // one section
	Put16(&b, 0x54, 224);           // the optional header's size
	Put16(&b, 0x58, 0x10b);         // PE32
	Put32(&b, 0x58 + 28, kBase);

	size_t const table = 0x58 + 224;

	memcpy(&b[table], ".data\0\0\0", 8);
	Put32(&b, table + 8, sectionBytes);
	Put32(&b, table + 12, kSectionVa - kBase);
	Put32(&b, table + 16, sectionBytes);
	Put32(&b, table + 20, kRawAt);
	return b;
}

static size_t At(uint32_t va) { return kRawAt + (va - kSectionVa); }

// A shape at `header`: `points` points (x = 100 i, y = 200 i, z = 300 i), and the polygons given as lists of point numbers (each corner written as 3 x its number, then -1), all colour 0x123456.
// The point list, the polygons and the item list are put after the header.
static void PutShape(Bytes *b, uint32_t header, int32_t points, std::vector<std::vector<int32_t>> const &polygons, bool cornersTimesThree = true, bool endWithMinusOne = true)
{
	uint32_t const holder = header + 0x80, list = header + 0x90, items = header + 0x90 + 12 * (uint32_t)(points > 0 ? points : 1);
	uint32_t polygon = items + 4 * (uint32_t)polygons.size();

	Put32(b, At(header), (uint32_t)points);
	Put32(b, At(header + 4), (uint32_t)polygons.size());
	Put32(b, At(header + 12), holder);
	Put32(b, At(header + 16), items);
	Put32(b, At(holder), list);

	for (int32_t i = 0; i < points; i++)
	{
		Put32(b, At(list + 12 * i), (uint32_t)(100 * i));
		Put32(b, At(list + 12 * i + 4), (uint32_t)(200 * i));
		Put32(b, At(list + 12 * i + 8), (uint32_t)(300 * i));
	}

	for (size_t p = 0; p < polygons.size(); p++)
	{
		Put32(b, At(items + 4 * (uint32_t)p), polygon);
		Put32(b, At(polygon + 12), 0xff123456u);

		size_t k = 0;

		for (; k < polygons[p].size(); k++)
		{
			Put32(b, At(polygon + 16 + 4 * (uint32_t)k), (uint32_t)(cornersTimesThree ? 3 * polygons[p][k] : polygons[p][k]));
		}

		if (endWithMinusOne)
		{
			Put32(b, At(polygon + 16 + 4 * (uint32_t)k), 0xffffffffu);
		}
		else
		{
			Put32(b, At(polygon + 16 + 4 * (uint32_t)k), 0);   // a corner 0, and so on: no -1
		}

		polygon += 16 + 4 * (uint32_t)(k + 1);
	}
}

static void TestSha256()
{
	Check(IntengExe_Sha256((uint8_t const *)"", 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256 of nothing (FIPS 180-4)");
	Check(IntengExe_Sha256((uint8_t const *)"abc", 3) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256 of abc");

	char const *const two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

	Check(IntengExe_Sha256((uint8_t const *)two, strlen(two)) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "sha256 of the two-block example (56 bytes)");

	std::string million(1000000, 'a');

	Check(IntengExe_Sha256((uint8_t const *)million.data(), million.size()) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "sha256 of a million a's");
}

static void TestParse()
{
	IntengExeImage image;
	IntengExeRefusal why = IntengExeRefusal::None;

	Check(IntengExe_Parse(Program(), &image, &why) && why == IntengExeRefusal::None && image.imageBase == kBase && image.sections.size() == 1 && image.sections[0].va == kSectionVa,
		"a good program: its base and its one section");

	Bytes b = Program();
	b[1] = 'X';
	Check(!IntengExe_Parse(b, &image, &why) && why == IntengExeRefusal::NotAnExe && image.sections.empty(), "REFUSED: no MZ");

	b = Program();
	Put32(&b, 0x3c, (uint32_t)b.size());
	Check(!IntengExe_Parse(b, &image, &why) && why == IntengExeRefusal::NotAnExe, "REFUSED: the PE header's place is past the end of the file");

	b = Program();
	Put32(&b, 0x3c, 0xfffffff0u);
	Check(!IntengExe_Parse(b, &image, &why) && why == IntengExeRefusal::NotAnExe, "REFUSED: a PE header place that would wrap round");

	b = Program();
	b[0x41] = 'X';
	Check(!IntengExe_Parse(b, &image, &why) && why == IntengExeRefusal::NotAnExe, "REFUSED: no PE signature");

	b = Program();
	Put16(&b, 0x44, 0x8664);
	Check(!IntengExe_Parse(b, &image, &why) && why == IntengExeRefusal::NotAnExe, "REFUSED: not an i386 program");

	b = Program();
	Put16(&b, 0x58, 0x20b);
	Check(!IntengExe_Parse(b, &image, &why) && why == IntengExeRefusal::NotAnExe, "REFUSED: a PE32+ (64-bit) optional header");

	b = Program();
	Put16(&b, 0x54, 64);
	Check(!IntengExe_Parse(b, &image, &why) && why == IntengExeRefusal::NotAnExe, "REFUSED: an optional header too short to hold the image base");

	b = Program();
	Put16(&b, 0x46, 0);
	Check(!IntengExe_Parse(b, &image, &why) && why == IntengExeRefusal::BadSections, "REFUSED: no section");

	b = Program();
	Put16(&b, 0x46, kIntengExeMostSections + 1);
	Check(!IntengExe_Parse(b, &image, &why) && why == IntengExeRefusal::BadSections, "REFUSED: more sections than kIntengExeMostSections");

	b = Program();
	Put32(&b, 0x58 + 224 + 16, kSectionBytes + 1);
	Put32(&b, 0x58 + 224 + 8, 0);
	Check(!IntengExe_Parse(b, &image, &why) && why == IntengExeRefusal::BadSections, "REFUSED: a section whose bytes run past the end of the file");

	b = Program();
	Put32(&b, 0x58 + 224 + 20, 0xfffffff0u);
	Check(!IntengExe_Parse(b, &image, &why) && why == IntengExeRefusal::BadSections, "REFUSED: a section whose file place would wrap round");

	Bytes tiny(0x20, 0);
	tiny[0] = 'M';
	tiny[1] = 'Z';
	Check(!IntengExe_Parse(tiny, &image, &why) && why == IntengExeRefusal::NotAnExe, "REFUSED: shorter than a DOS header");
}

static void TestReads()
{
	IntengExeImage image;
	IntengExeRefusal why = IntengExeRefusal::None;
	Bytes b = Program();

	Put32(&b, At(0x5cd958), 0xcafef00du);
	Check(IntengExe_Parse(b, &image, &why), "the program for the reads");

	uint32_t value = 0;
	uint8_t bytes[8];

	Check(IntengExe_ReadUint32(image, 0x5cd958, &value) && value == 0xcafef00du, "a word inside the section");
	Check(!IntengExe_ReadUint32(image, kSectionVa - 4, &value), "REFUSED: a word before the section");
	Check(!IntengExe_ReadUint32(image, kSectionVa + kSectionBytes - 2, &value), "REFUSED: a word that runs off the section's end");
	Check(IntengExe_ReadUint32(image, kSectionVa + kSectionBytes - 4, &value), "the section's last word");
	Check(!IntengExe_ReadBytes(image, 0xfffffffcu, 8, bytes), "REFUSED: a read that would wrap round the address space");
	Check(!IntengExe_ReadBytes(image, kSectionVa, 0, bytes), "REFUSED: a read of nothing");

	// the file's bytes past the section's own size are padding: never read
	Bytes padded = Program();
	Put32(&padded, 0x58 + 224 + 8, 0x100);
	Check(IntengExe_Parse(padded, &image, &why) && !IntengExe_ReadUint32(image, kSectionVa + 0x100, &value) && IntengExe_ReadUint32(image, kSectionVa + 0xfc, &value),
		"a section's file bytes past its own size are not read");
}

static std::string TempPath(char const *name)
{
	char folder[MAX_PATH];
	DWORD const got = GetTempPathA(sizeof(folder), folder);

	return std::string(got > 0 && got < sizeof(folder) ? folder : ".\\") + name;
}

static void TestLoad()
{
	IntengExeImage image;
	IntengExeRefusal why = IntengExeRefusal::None;
	std::string from;

	Check(!IntengExe_Load({ TempPath("host_inteng_exe_test_none\\inteng.exe") }, &image, &from, &why) && why == IntengExeRefusal::NotFound, "REFUSED: no inteng.exe there");

	std::string const folder = TempPath("host_inteng_exe_test_folder");

	CreateDirectoryA(folder.c_str(), nullptr);
	Check(!IntengExe_Load({ folder }, &image, &from, &why) && why == IntengExeRefusal::NotAFile && from == folder, "REFUSED: the name is a folder");
	RemoveDirectoryA(folder.c_str());

	std::string const wrong = TempPath("host_inteng_exe_test_wrong.exe");
	Bytes const program = Program();
	FILE *file = nullptr;

	if (fopen_s(&file, wrong.c_str(), "wb") == 0 && file)
	{
		fwrite(program.data(), 1, program.size(), file);
		fclose(file);
	}

	// a good program that is not the traced one; the first path not there is passed over
	Check(!IntengExe_Load({ TempPath("host_inteng_exe_test_none.exe"), wrong }, &image, &from, &why) && why == IntengExeRefusal::NotTheTracedExe && from == wrong && image.sections.empty(),
		"REFUSED: a program whose sha256 is not the traced inteng.exe's");

	if (fopen_s(&file, wrong.c_str(), "wb") == 0 && file)
	{
		Bytes const big(kIntengExeMostBytes + 1, 0);

		fwrite(big.data(), 1, big.size(), file);
		fclose(file);
	}

	Check(!IntengExe_Load({ wrong }, &image, &from, &why) && why == IntengExeRefusal::TooLong, "REFUSED: a file bigger than kIntengExeMostBytes");
	DeleteFileA(wrong.c_str());
}

static void TestShapes()
{
	IntengExeImage image;
	IntengExeRefusal whyExe = IntengExeRefusal::None;
	IntengShapes shapes;
	IntengShapesRefusal why = IntengShapesRefusal::None;
	uint32_t const header3 = 0x5d0000;   // slot 3 is read through the pointer word at 0x5cd964 (FUN_0042c700 0x42c727)
	Bytes b = Program();

	Put32(&b, At(0x5cd964), header3);
	PutShape(&b, header3, 5, { { 0, 1, 2 }, { 0, 2, 3, 4 } });
	Check(IntengExe_Parse(b, &image, &whyExe), "the program for the shapes");

	uint32_t header = 0;

	Check(IntengShapes_HeaderOf(image, 3, &header) && header == header3, "slot 3's header through its pointer word");
	Check(IntengShapes_HeaderOf(image, 13, &header) && header == 0x5e0730, "slot 13's header is the fixed one (0x42c784)");
	Check(!IntengShapes_HeaderOf(image, 19, &header) && !IntengShapes_HeaderOf(image, -1, &header), "no slot below 0 or above 18");

	int const three[] = { 3 };

	Check(IntengShapes_Read(image, three, 1, &shapes, &why) && shapes.have[3] && !shapes.have[2] && shapes.triangles[3].size() == 3, "a triangle and a quad: 1 + 2 triangles, fanned");

	if (shapes.triangles[3].size() == 3)
	{
		IntengShapeTriangle const &t = shapes.triangles[3][2];   // the quad's second: corners 0, 3, 4

		Check(t.p[0] == 0 && t.p[3] == 300 && t.p[4] == 600 && t.p[5] == 900 && t.p[6] == 400 && t.p[8] == 1200 && t.colour == 0x123456, "the fan's corners and the colour (its low 24 bits)");
	}

	int const bad[] = { 19 };
	Check(!IntengShapes_Read(image, bad, 1, &shapes, &why) && why == IntengShapesRefusal::BadSlot && !shapes.have[3], "REFUSED: slot 19, and nothing is kept");

	// a header outside the file
	Bytes c = b;
	Put32(&c, At(0x5cd964), 0x10000000u);
	IntengExe_Parse(c, &image, &whyExe);
	Check(!IntengShapes_Read(image, three, 1, &shapes, &why) && why == IntengShapesRefusal::BadAddress, "REFUSED: a header outside the exe's bytes");

	// no points; too many polygons
	c = b;
	Put32(&c, At(header3), 0);
	IntengExe_Parse(c, &image, &whyExe);
	Check(!IntengShapes_Read(image, three, 1, &shapes, &why) && why == IntengShapesRefusal::BadCounts, "REFUSED: a shape with no points");

	c = b;
	Put32(&c, At(header3 + 4), (uint32_t)kIntengShapeMostItems + 1);
	IntengExe_Parse(c, &image, &whyExe);
	Check(!IntengShapes_Read(image, three, 1, &shapes, &why) && why == IntengShapesRefusal::BadCounts, "REFUSED: more polygons than kIntengShapeMostItems");

	c = b;
	Put32(&c, At(header3), 0x80000000u);
	IntengExe_Parse(c, &image, &whyExe);
	Check(!IntengShapes_Read(image, three, 1, &shapes, &why) && why == IntengShapesRefusal::BadCounts, "REFUSED: a negative point count");

	// the point list runs off the section
	c = b;
	Put32(&c, At(header3 + 0x80), kSectionVa + kSectionBytes - 8);
	IntengExe_Parse(c, &image, &whyExe);
	Check(!IntengShapes_Read(image, three, 1, &shapes, &why) && why == IntengShapesRefusal::BadAddress, "REFUSED: a point list that runs off the exe's bytes");

	// corners
	auto refusedCorner = [&](std::vector<std::vector<int32_t>> const &polygons, bool timesThree, bool ended, char const *what)
	{
		Bytes d = Program();

		Put32(&d, At(0x5cd964), header3);
		PutShape(&d, header3, 5, polygons, timesThree, ended);
		IntengExe_Parse(d, &image, &whyExe);
		Check(!IntengShapes_Read(image, three, 1, &shapes, &why) && why == IntengShapesRefusal::BadCorner, what);
	};

	refusedCorner({ { 0, 1, 5 } }, true, true, "REFUSED: a corner that is not one of the shape's points");
	refusedCorner({ { 0, 1, 2 } }, false, true, "REFUSED: a corner offset that is not a multiple of 3");
	refusedCorner({ { 0, 1 } }, true, true, "REFUSED: a polygon of 2 corners");
	refusedCorner({ { 0, 1, -7 } }, true, true, "REFUSED: a negative corner");
	refusedCorner({ { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1 } }, true, true, "REFUSED: more than kIntengShapeMostCorners corners");
	refusedCorner({ { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 } }, true, false, "REFUSED: no -1 after the 16th corner");
}

// the shape-name table (0x5e7b58, FUN_004f6a20): rows of a name pointer, a type and an extra, ended by a 0 pointer
static void PutName(Bytes *b, uint32_t row, uint32_t namePointer, char const *name, int32_t type, int32_t extra)
{
	Put32(b, At(kIntengShapeNamesAt + 12 * row), namePointer);
	Put32(b, At(kIntengShapeNamesAt + 12 * row + 4), (uint32_t)type);
	Put32(b, At(kIntengShapeNamesAt + 12 * row + 8), (uint32_t)extra);

	if (name)
	{
		memcpy(&(*b)[At(namePointer)], name, strlen(name) + 1);
	}
}

static void TestShapeNames()
{
	IntengExeImage image;
	IntengExeRefusal whyExe = IntengExeRefusal::None;
	std::vector<IntengShapeName> names;
	IntengNamesRefusal why = IntengNamesRefusal::None;
	uint32_t const text = 0x5e8000;
	Bytes b = Program();

	PutName(&b, 0, text, "sentry", 9, 500);
	PutName(&b, 1, text + 16, "pulse", 2, 0);
	PutName(&b, 2, 0, nullptr, 0, 0);
	IntengExe_Parse(b, &image, &whyExe);
	Check(IntengShapeNames_Read(image, &names, &why) && names.size() == 2 && names[0].name == "sentry" && names[0].type == 9 && names[0].extra == 500 && names[1].name == "pulse" && names[1].type == 2,
		"two rows and the 0 that ends them");

	auto refused = [&](Bytes const &d, IntengNamesRefusal wanted, char const *what)
	{
		IntengExe_Parse(d, &image, &whyExe);
		Check(!IntengShapeNames_Read(image, &names, &why) && why == wanted && names.empty(), what);
	};

	Bytes c = b;
	PutName(&c, 1, 0x10000000u, nullptr, 2, 0);
	refused(c, IntengNamesRefusal::BadAddress, "REFUSED: a name outside the exe's bytes");

	c = Program();
	for (uint32_t row = 0; row <= (uint32_t)kIntengShapeNamesMost; row++)
	{
		PutName(&c, row, text, "pulse", 2, 0);
	}
	refused(c, IntengNamesRefusal::NoEnd, "REFUSED: more than kIntengShapeNamesMost rows with no end");

	c = b;
	PutName(&c, 1, text + 16, "", 2, 0);
	refused(c, IntengNamesRefusal::BadName, "REFUSED: an empty name");

	c = b;
	PutName(&c, 1, text + 16, "a name with spaces", 2, 0);
	refused(c, IntengNamesRefusal::BadName, "REFUSED: a name with a letter outside ! to ~");

	c = b;
	PutName(&c, 1, text + 16, "abcdefghijklmnopqrstuvwxyzabcdefg", 2, 0);
	refused(c, IntengNamesRefusal::BadName, "REFUSED: a name longer than kIntengShapeNameMost");

	c = b;
	PutName(&c, 1, text + 16, "pulse", 32, 0);
	refused(c, IntengNamesRefusal::BadNumber, "REFUSED: a type above 31");

	c = b;
	PutName(&c, 1, text + 16, "pulse", 2, -1);
	refused(c, IntengNamesRefusal::BadNumber, "REFUSED: a negative extra data");

	// a name whose 0 would be past the section's end
	c = Program();
	uint32_t const last = kSectionVa + kSectionBytes - 3;
	PutName(&c, 0, last, nullptr, 2, 0);
	c[At(last)] = 'a';
	c[At(last + 1)] = 'b';
	c[At(last + 2)] = 'c';
	refused(c, IntengNamesRefusal::BadAddress, "REFUSED: a name that runs off the exe's bytes");
}

int main()
{
	setvbuf(stdout, 0, _IONBF, 0);

	TestSha256();
	TestParse();
	TestReads();
	TestLoad();
	TestShapes();
	TestShapeNames();

	printf("%d checks, %d failed\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
