// The test for INTENG'S BUILT-IN SHAPES AND SHAPE NAMES IN OUR SOURCE (Vasquez 2026-11-24; the owner's ruling, reversing 11-16 for them: "the hardcoded icons
// inside inteng are under our right to decompile", "Any 'art' in inteng's disassemble is OURS and protected ... revert the commits, get them as they were",
// "This ONLY applied to rifs"). He tested alpha.6 as a new player, with no inteng.exe, and every marker was a plain box: the shapes were read from his
// inteng.exe at run time, and without it the editor drew boxes and guessed no placed shape's type from its name. They are back as they were before 11-16:
// editor_icon_shapes.inc byte for byte as at f85c0e3c, and simple_object_edit.cpp's shapeRows as before 3b96ab8f. No inteng.exe is needed or looked for.
// This needs no file of Rebellion's and no inteng.exe, so it runs everywhere (on GitHub too):
//   (1) every shape the editor draws is inteng's own: its triangle count, and its first triangle's colour (editor_icons.h);
//   (2) a placed shape's type is guessed from inteng's own name table (simple_object_edit.h SimpleShapeGuess);
//   (3) the shapes' file is the one from before 11-16, byte for byte (its SHA-256, read from the source tree: /FC gives this file's whole path).
// Written 2026-11-24 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>

#include <string>
#include <vector>

#include <windows.h>

#include "editor_icons.h"
#include "simple_object_edit.h"
#include "dll_load.h"

// ---- Stand-ins for the game's own file and message functions the level code links (as host_simple_object_edit_test has them): this test opens no file of a level
HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

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

static int checks = 0, checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// The tree's root from this file's own path (the build gives __FILE__ whole: /FC)
static std::string TreeRoot()
{
	std::string here = __FILE__;
	size_t const tests = here.find_last_of("\\/");
	size_t const root = tests == std::string::npos ? std::string::npos : here.find_last_of("\\/", tests - 1);

	return root == std::string::npos ? std::string(".") : here.substr(0, root);
}

// (1) inteng's own numbers for each shape the editor draws (editor_icons.h's table): its triangles (every polygon fanned from its first corner) and its first
// triangle's colour, 0xRRGGBB. Our old plain boxes had 12 triangles each, in colours of our own.
struct ShapeWant
{
	int shape;
	size_t triangles;
	uint32_t firstColour;
	char const *what;
};

static ShapeWant const kShapes[] = {
	{ 2, 6, 0x0080ff, "the particle generator and the light marker's pyramid" },
	{ 3, 52, 0x00c832, "the AVP generator's green figure" },
	{ 4, 20, 0, "the placed hierarchy" },
	{ 7, 120, 0, "the AVP bad guy's head" },
	{ 8, 60, 0, "the sound's speaker" },
	{ 9, 64, 0, "the player starting position" },
	{ 13, 24, 0, "the virtual object" },
};

static void TestTheShapes()
{
	for (ShapeWant const &want : kShapes)
	{
		Check(host::EditorIcon_Known(want.shape), "shape " + std::to_string(want.shape) + " (" + want.what + ") is one the editor draws");
		Check(host::EditorIcon_TriangleCount(want.shape) == want.triangles, "shape " + std::to_string(want.shape) + " is inteng's own: " + std::to_string(want.triangles) +
			" triangles (it has " + std::to_string(host::EditorIcon_TriangleCount(want.shape)) + ")");

		if (want.firstColour)
		{
			double const place[3] = { 0, 0, 0 };
			std::vector<host::IconWorldTriangle> made;

			host::EditorIcon_Add(want.shape, place, nullptr, 0, &made);
			char hex[16] = "", got[16] = "none";

			_snprintf_s(hex, sizeof(hex), _TRUNCATE, "0x%06x", want.firstColour);

			if (!made.empty())
			{
				_snprintf_s(got, sizeof(got), _TRUNCATE, "0x%06x", made[0].colour & 0xffffffu);
			}

			Check(!made.empty() && (made[0].colour & 0xffffffu) == want.firstColour, std::string("and in inteng's own colour, ") + hex + " (it is " + got + ")");
		}
	}

	Check(!host::EditorIcon_Known(5) && host::EditorIcon_TriangleCount(5) == 0, "a shape the editor doesn't draw (5) has nothing");
}

// (2) inteng's shape-name table (0x5e7b58, 29 rows): the type and extra data a placed shape's outside file name gives it
static void TestTheNames()
{
	struct NameWant
	{
		char const *name;
		int32_t type;
		int32_t extra;
	};

	NameWant const kNames[] = { { "medbox01", 4, 0 }, { "pulse", 2, 0 }, { "sentry", 9, 500 }, { "skeeter", 2, 23 }, { "simple", 16, 0 } };

	for (NameWant const &want : kNames)
	{
		int32_t type = -1, extra = -1;
		bool const found = host::SimpleShapeGuess(want.name, &type, &extra);

		Check(found && type == want.type && extra == want.extra, std::string("a shape named ") + want.name + " is type " + std::to_string(want.type) + ", extra " +
			std::to_string(want.extra) + " (" + (found ? "type " + std::to_string(type) + ", extra " + std::to_string(extra) : std::string("no type guessed")) + ")");
	}

	int32_t type = -1, extra = -1;

	Check(!host::SimpleShapeGuess("notinteng", &type, &extra), "a name not in inteng's table guesses nothing");
}

// (3) editor_icon_shapes.inc as at f85c0e3c (the commit before 11-16's a82f81f6 took it out), byte for byte: blob 106027de, 28841 bytes
static void TestTheFile()
{
	std::string const path = TreeRoot() + "\\src\\host\\editor_icon_shapes.inc";
	std::wstring const wide(path.begin(), path.end());
	std::string hex, why;
	bool const read = host::DllLoad_FileSha256(wide.c_str(), &hex, &why);

	Check(read && hex == "af31ccbeb59355cdffa378b57b1421a4d3fb4a28392c0bb79284d52089fe7279",
		"src/host/editor_icon_shapes.inc is the file from before 11-16, byte for byte (SHA-256 " + (read ? hex : "not read: " + why) + ")");
}

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	printf("inteng's built-in shapes and shape names, in our source:\n");

	TestTheShapes();
	TestTheNames();
	TestTheFile();

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed == 0)
	{
		printf("All checks passed\n");
		return 0;
	}

	printf("SOME CHECKS FAILED\n");
	return 1;
}
