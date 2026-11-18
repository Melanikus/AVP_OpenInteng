// verify_survey_shared_shapes.cpp: SURVEY_SHARED_SHAPES, a survey and not a check (the "verify_" in the name is only so that build-verifier.sh builds it). Written
// 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng, from Bishop's verify_survey_merge_pairs.cpp: Texture Map's Return asks about a SHARED shape (FUN_0055aa50), and its test
// needs a shape of the level's own (not imported: a write command drops imported polygons) that two or more objects use. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// USAGE: verify_survey_shared_shapes.exe <a folder of level files>
// For every .rif in the folder (by name, A to Z), one line for each object-list row whose shape has two or more users:
//   <level> row=<row> object=<name> users=<n> polygons=<n> textured=<n> first-textured=<polygon or -1> imported=<yes|no> others=<name,name,...>
//   (each shape once: the first row met that uses it)
// then <level> counts: shared-shapes=<n> own=<n>.

//     would refuse (the shape not readable, or the merge data not one number for each polygon); cut: objects whose search hit its bound.
//   <level> not opened: <why>   when the editor's own open of the level refuses it.
// It gives back 0 when every level was opened, 1 otherwise, 2 for a bad call.
//
// WHY THE NUMBERS ARE THE EDITOR'S OWN. Nothing here re-makes a rule; it links the editor's code and calls it:
// - THE ROWS are LevelObjects_Read's (src/host/level_objects.cpp), on the level opened by the editor's own OpenLevelFile: the very list the
//   harness's pick-poly takes its row from (editor_main.cpp, kHarnessPickPoly: everyObject[row].id).
// - THE OBJECT a row stands for is found by its ID, the first object with that ID in the level's order, as Merge finds it (PolyEdit_Merge's
//   ObjectWithId in poly_edit.cpp). A later row with the same ID (two objects of one name) acts on that same first object, so it is not listed.
// - THE POLYGON NUMBERS are the file's, from host::ShapeEdit_Read, the numbers Merge works on; the merge data is read as PolyEdit_Merge reads it
//   (the shape's first SHPMRGDT, through OpenLevelFile::PayloadOf, one number for each polygon or Merge refuses it).
// - JOINABLE is host::PolyEdit_Joinable(shape, a, b, false), as PolyEdit_MergeInModel calls it for a picked first and b second (fake merge data
//   off, the default). The only thing done here is to choose WHICH pairs to ask it about: pairs of triangles that share at least one corner
//   number. Joinable needs exactly two shared corner numbers, so no pair it could take is left out; that choice only saves time.
//
// THE RULES (a level's bytes are hostile): the editor's own open and shape reader check every size and count; here the folder's files are
// at most kMostLevels, a level's listed pairs at most kMostLinesPerLevel, an object's calls of the test at most kMostTestsPerObject (an object
// that hits it is counted as cut), and an object's name is printed only as plain letters, digits and _ . - (any other byte as %XX), at most
// 64 bytes. Nothing is ever written to a level: the open is read only, and nothing is saved.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_objects.h"
#include "object_list.h"
#include "poly_edit.h"
#include "polygon_items.h"
#include "shape_edit.h"
#include "uv_edit.h"

// ---- Stand-ins for the parts of the game the chunk code calls (as tests/host_poly_edit_test.cpp has them): files are read by Windows' own

HANDLE avp_CreateFile(const char *n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE m) { return CreateFileA(n, a, s, sa, c, f, m); }
DWORD avp_GetFileAttributes(const char *n) { return GetFileAttributesA(n); }
void avp_MessageBox(const char *m, int) { printf("# the game's code says: %s\n", m ? m : ""); }
void avp_exit(int c) { printf("# FAILED  the game's code stopped the program (code %d)\n", c); exit(c ? c : 1); }
void LogErrorStringC(const char *) {}
void Sound_ForgetAddressesInside(const void *, size_t) {}

// level_objects.cpp's LevelObjects_Hold asks the game side where it loaded the level from. This survey never calls it (it hands its own open to
// LevelObjects_Use), so the answer is always "no level".
bool LevelView_FilePath(char *path, size_t size)
{
	if (path && size)
	{
		path[0] = '\0';
	}

	return false;
}

// The bounds (hostile input)
static size_t const kMostLevels = 4096;               // level files read from the folder
static size_t const kMostLinesPerLevel = 1000000;     // pair lines printed for one level
static size_t const kMostTestsPerObject = 50000000;   // calls of PolyEdit_Joinable for one object's shape
static size_t const kMostNameBytes = 64;              // an object's name, as the game keeps it (OBJECT_NAME_BYTES)

// Takes the level's open away from the lookups (LevelObjects_Use(nullptr)) on every way out of a level, before the open itself goes
struct UseGuard
{
	~UseGuard() { LevelObjects_Use(nullptr); }
};

// A name for the output: plain letters, digits and _ . - as they are, every other byte as %XX, at most `most` bytes read and never past a 0
static std::string Plain(char const *text, size_t most)
{
	std::string out;

	if (!text)
	{
		return out;
	}

	for (size_t i = 0; i < most && text[i] != '\0'; i++)
	{
		unsigned char const c = (unsigned char)text[i];

		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')
		{
			out += (char)c;
		}
		else
		{
			char hex[4];

			snprintf(hex, sizeof(hex), "%%%02X", c);
			out += hex;
		}
	}

	return out;
}

static char const *OpenWords(LevelOpenResult result)
{
	switch (result)
	{
	case LevelOpenResult::Opened: return "opened";
	case LevelOpenResult::NoPath: return "no path";
	case LevelOpenResult::CantRead: return "the file could not be read whole, or is bigger than any level the game reads";
	case LevelOpenResult::LoaderRefused: return "the game's own loader refuses it";
	case LevelOpenResult::ChangedWhileOpening: return "the file changed while it was being opened";
	case LevelOpenResult::DoesntPair: return "the loaded chunks do not pair with the file's own";
	case LevelOpenResult::TooManyChunks: return "more chunks than the editor takes";
	case LevelOpenResult::NothingOpen: return "nothing open";
	}

	return "unknown answer";
}

// The first object of the level with this ID, in the level's own order: the one PolyEdit_Merge (and ShapeEdit_Read) find for it
static Object_Chunk *FirstWithId(File_Chunk *level, uint8_t const *id)
{
	List<Object_Chunk *> found;

	level->list_objects(&found);

	for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (object && memcmp(&object->object_data.ID, id, host::kObjectIdBytes) == 0)
		{
			return object;
		}
	}

	return nullptr;
}


static bool SurveyLevel(std::string const &path, std::string const &levelName)
{
	OpenLevelFile open;
	LevelOpenResult const opened = open.Open(path.c_str());

	if (opened != LevelOpenResult::Opened || !open.Level())
	{
		printf("%s not opened: %s\n", levelName.c_str(), OpenWords(opened));
		return false;
	}

	LevelObjects_Use(&open);
	UseGuard letGo;

	std::vector<host::LevelObject> rows;

	if (!LevelObjects_Read(&rows))
	{
		printf("%s not opened: the object list could not be read\n", levelName.c_str());
		return false;
	}

	std::vector<Shape_Chunk *> seen;
	size_t shared = 0, own = 0;

	// list_objects' order (the rows uv_verify takes): each object's name and its place
	std::vector<std::string> listNames;
	{
		List<Object_Chunk *> all;

		open.Level()->list_objects(&all);

		for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
		{
			Object_Chunk *const one = each();

			listNames.push_back(one ? std::string(one->object_data.o_name, strnlen(one->object_data.o_name, sizeof(one->object_data.o_name))) : std::string());
		}
	}

	auto const listRow = [&](std::string const &name) -> long
	{
		for (size_t i = 0; i < listNames.size(); i++)
		{
			if (listNames[i] == name)
			{
				return (long)i;
			}
		}

		return -1;
	};

	for (size_t row = 0; row < rows.size() && row < host::kObjectListMost; row++)
	{
		uint8_t const *const id = rows[row].id;
		Object_Chunk *const object = FirstWithId(open.Level(), id);
		Shape_Chunk *const shapeChunk = object ? object->get_assoc_shape() : nullptr;

		if (!shapeChunk || std::find(seen.begin(), seen.end(), shapeChunk) != seen.end())
		{
			continue;
		}

		seen.push_back(shapeChunk);

		std::vector<std::string> users;

		if (!host::PolyEdit_ShapeUserNames(&open, id, &users) || users.size() < 2)
		{
			continue;
		}

		model::ShapeModel shape;
		std::string why;

		if (host::ShapeEdit_Read(open.Level(), &open, id, &shape, nullptr, &why) != host::ShapeEditResult::Done)
		{
			continue;
		}

		size_t textured = 0;
		int32_t first = -1;

		for (size_t p = 0; p < shape.polygons.size(); p++)
		{
			model::PolygonUv uv;

			if (host::UvEdit_ReadPolygon(&open, open.Level(), id, (int32_t)p, &uv, &why) == host::UvEditResult::Done && uv.textured)
			{
				textured++;
				first = first < 0 ? (int32_t)p : first;
			}
		}

		bool const imported = host::PolygonItems_ShapeHasOwnPictureList(open.Level(), id);
		std::string const name = Plain(rows[row].name, kMostNameBytes);
		std::string others;

		for (std::string const &one : users)
		{
			std::string const plain = Plain(one.c_str(), kMostNameBytes);

			if (plain != name)
			{
				others += (others.empty() ? "" : ",") + plain + "(listrow " + std::to_string(listRow(one)) + ")";
			}
		}

		shared++;
		own += imported ? 0 : 1;
		printf("%s row=%zu listrow=%ld object=%s users=%zu polygons=%zu textured=%zu first-textured=%d imported=%s others=%s\n", levelName.c_str(), row, listRow(std::string(rows[row].name ? rows[row].name : "", rows[row].name ? strnlen(rows[row].name, kMostNameBytes) : 0)), name.c_str(), users.size(),
			shape.polygons.size(), textured, first, imported ? "yes" : "no", others.c_str());
	}

	printf("%s counts: shared-shapes=%zu own=%zu\n", levelName.c_str(), shared, own);
	fflush(stdout);
	return true;
}

static bool EndsWithRif(std::string const &name)
{
	return name.size() > 4 && _stricmp(name.c_str() + name.size() - 4, ".rif") == 0;
}

int main(int argc, char **argv)
{
	if (argc != 2 || !argv[1] || !argv[1][0])
	{
		printf("usage: verify_survey_shared_shapes.exe <a folder of level files>\n");
		return 2;
	}

	std::string folder = argv[1];

	while (!folder.empty() && (folder.back() == '\\' || folder.back() == '/'))
	{
		folder.pop_back();
	}

	if (folder.empty() || folder.size() > MAX_PATH - 16)
	{
		printf("survey_shared_shapes: the folder's path is empty or too long\n");
		return 2;
	}

	// the folder's level files, plain files only, by name A to Z (letters of either case alike), so two runs list them alike
	std::vector<std::string> names;
	WIN32_FIND_DATAA found;
	HANDLE const search = FindFirstFileA((folder + "\\*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
	{
		printf("survey_shared_shapes: the folder could not be read\n");
		return 2;
	}

	do
	{
		std::string const name = found.cFileName;

		if (!(found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)) && EndsWithRif(name) && folder.size() + 1 + name.size() < MAX_PATH)
		{
			if (names.size() >= kMostLevels)
			{
				printf("survey_shared_shapes: more than %zu level files: the rest are left out\n", kMostLevels);
				break;
			}

			names.push_back(name);
		}
	} while (FindNextFileA(search, &found));

	FindClose(search);

	std::sort(names.begin(), names.end(), [](std::string const &x, std::string const &y) { return _stricmp(x.c_str(), y.c_str()) < 0; });

	bool allOpened = true;

	for (std::string const &name : names)
	{
		allOpened = SurveyLevel(folder + "\\" + name, Plain(name.c_str(), MAX_PATH)) && allOpened;
	}

	printf("survey_shared_shapes: %zu level files\n", names.size());
	return allOpened ? 0 : 1;
}
