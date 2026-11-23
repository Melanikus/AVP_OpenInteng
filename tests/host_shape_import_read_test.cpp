// The test for a shape's file read on ONE handle (src/host/shape_import.h ShapeImport_Read: Tao 3, for the lookup order of Quick-create). Written 2026-11-23
// by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// It runs in a scratch folder of its own:   <a scratch folder>
//   (1) a plain file is read whole, its name kept as "<name>.rif"; a missing one is NotFound; an unplain name, or no folder, refused as before
//   (2) a file with a second name (a hard link, CreateHardLinkW: no privilege needed) is refused as NotPlain, BOTH its names; once the second name is gone,
//       the first reads again
//   (3) a folder named <name>.rif, and a junction named <name>.rif (mklink /J), are refused as NotPlain: there is something of that name, so it is never
//       NotFound (the lookup would go on to the next folder past it)
//   (4) the sizes: an empty file and one a byte over kShapeImportMostBytes are TooBig; one of exactly kShapeImportMostBytes reads whole
// RED FIRST: on the read before (fopen), (2) and (3) fail: a hard link reads, and a folder or a junction is NotFound.

#include <windows.h>

#include <stdio.h>

#include <string>
#include <vector>

#include "make_junction.h"   // a junction made without a shell
#include "shape_import.h"

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

static bool WriteAll(std::string const &path, std::vector<char> const &bytes)
{
	FILE *file = fopen(path.c_str(), "wb");

	if (!file)
	{
		return false;
	}

	bool const written = bytes.empty() || fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();

	return fclose(file) == 0 && written;
}

static ShapeImportResult Read(std::string const &folder, std::string const &name, std::vector<char> *bytes, std::string *fileName = nullptr)
{
	bytes->assign(5, 'x');
	return ShapeImport_Read(folder.c_str(), name, bytes, fileName);
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("  FAILED  give a scratch folder: nothing was tested\n");
		return 1;
	}

	std::string const folder = std::string(argv[1]) + "\\shape-read";
	bool const made = (CreateDirectoryA(argv[1], nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) && (CreateDirectoryA(folder.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS);

	Check(made, "the scratch folder is made: " + folder);

	std::vector<char> const shape = { 'R', 'E', 'B', 'I', 'N', 'F', 'F', '2', 1, 2, 3, 4 };
	std::vector<char> bytes;
	std::string fileName;

	// ---- (1) plain, missing, names, no folder
	puts("Plain:");
	// the call first, then the check (MSVC builds a check's words before its condition, so the words would show the name from before the call)
	bool const plainRead = WriteAll(folder + "\\plain.rif", shape) && Read(folder, "plain", &bytes, &fileName) == ShapeImportResult::Done;

	Check(plainRead && bytes == shape && fileName == "plain.rif", "(1) a plain file is read whole, its name plain.rif (" + fileName + ")");
	Check(Read(folder, "missing", &bytes) == ShapeImportResult::NotFound && bytes.empty(), "(1) a missing file: NotFound, nothing given");
	Check(Read(folder, "nul", &bytes) == ShapeImportResult::BadName && Read(folder, "a.b", &bytes) == ShapeImportResult::BadName && bytes.empty(),
		"(1) a device's name and a name with a dot: BadName, as before");
	Check(ShapeImport_Read("", "plain", &bytes, nullptr) == ShapeImportResult::NoFolder, "(1) no folder: NoFolder, as before");

	// ---- (2) a second name
	puts("A second name (a hard link):");
	bool const linked = CreateHardLinkA((folder + "\\second.rif").c_str(), (folder + "\\plain.rif").c_str(), nullptr) != 0;

	Check(linked, "(2) second.rif made a second name of plain.rif");
	Check(linked && Read(folder, "second", &bytes) == ShapeImportResult::NotPlain && bytes.empty(), "(2) second.rif: NotPlain, nothing given");
	Check(linked && Read(folder, "plain", &bytes) == ShapeImportResult::NotPlain && bytes.empty(), "(2) plain.rif too, while it has two names: NotPlain");
	DeleteFileA((folder + "\\second.rif").c_str());
	Check(Read(folder, "plain", &bytes) == ShapeImportResult::Done && bytes == shape, "(2) the second name gone: plain.rif reads again");

	// ---- (3) a folder, a junction
	puts("Something of that name that isn't a file:");
	bool const folderMade = CreateDirectoryA((folder + "\\folder.rif").c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;

	Check(folderMade && Read(folder, "folder", &bytes) == ShapeImportResult::NotPlain, "(3) a folder named folder.rif: NotPlain, never NotFound");

	// the junction made without a shell (make_junction.h: no command line, so the scratch folder from argv reaches none)
	std::string const target = folder + "\\target";
	std::string const joined = folder + "\\joined.rif";
	DWORD const before = GetFileAttributesA(joined.c_str());
	bool const junction = (CreateDirectoryA(target.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) &&
		(before != INVALID_FILE_ATTRIBUTES || MakeJunction(joined, target)) && (GetFileAttributesA(joined.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

	Check(junction && Read(folder, "joined", &bytes) == ShapeImportResult::NotPlain, "(3) a junction named joined.rif: NotPlain, never NotFound");

	// ---- (4) sizes
	puts("Sizes:");
	Check(WriteAll(folder + "\\empty.rif", {}) && Read(folder, "empty", &bytes) == ShapeImportResult::TooBig && bytes.empty(), "(4) an empty file: TooBig");
	Check(WriteAll(folder + "\\big.rif", std::vector<char>(kShapeImportMostBytes + 1, 'b')) && Read(folder, "big", &bytes) == ShapeImportResult::TooBig && bytes.empty(),
		"(4) a byte over the most: TooBig");
	Check(WriteAll(folder + "\\most.rif", std::vector<char>(kShapeImportMostBytes, 'm')) && Read(folder, "most", &bytes) == ShapeImportResult::Done &&
		bytes.size() == kShapeImportMostBytes, "(4) exactly the most: read whole");

	// ---- our own files away (RemoveDirectory on a junction takes the junction away, never what it points at)
	for (char const *name : { "plain.rif", "empty.rif", "big.rif", "most.rif" })
	{
		DeleteFileA((folder + "\\" + name).c_str());
	}

	RemoveDirectoryA((folder + "\\joined.rif").c_str());
	RemoveDirectoryA(target.c_str());
	RemoveDirectoryA((folder + "\\folder.rif").c_str());
	RemoveDirectoryA(folder.c_str());

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
