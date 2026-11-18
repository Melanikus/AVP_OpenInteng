// verify_c_file: the check of acceptance-hub-write-c-file (Construction > File > Write C File, 0xc). Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_c_file.exe <level> <object name> <the .c file> <the typed name>
//   The level is read here on its own (the game's library, not the editor's text maker), and the file must say what inteng's FUN_00503da0 says of the object's shape
//   (AvP-docs\engine\inteng\write-c-file-facts.md): it starts "/* object locations:" and names the object at its place times the level's scale; "#define <name>_pscale 1";
//   the header's first three numbers are the shape's point count, polygon count and 0; one "int <name>_item<i>[]" a polygon, no more; with a textured polygon the
//   picture names close the file ("char *<name>_localtextures[]" ... "\t0\n};\n") and each picture is defined. The exact text is c_shape_text_test's.
// Gives back 0 when it holds, 1 when it doesn't, 2 for a bad call.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "environment_chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "shape_edit.h"

HANDLE avp_CreateFile(const char *n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE m) { return CreateFileA(n, a, s, sa, c, f, m); }
DWORD avp_GetFileAttributes(const char *n) { return GetFileAttributesA(n); }
void avp_MessageBox(const char *m, int) { printf("  the game's code says: %s\n", m ? m : ""); }
void avp_exit(int c) { printf("  FAILED  the game's code stopped the program (code %d)\n", c); exit(c ? c : 1); }
void LogErrorStringC(const char *) {}
void Sound_ForgetAddressesInside(const void *, size_t) {}

static int failed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	failed += good ? 0 : 1;
}

static bool Textured(int32_t type)
{
	return type == 5 || type == 6 || type == 7 || (type >= 0x15 && type <= 0x19);
}

int main(int argc, char **argv)
{
	if (argc != 5 || strlen(argv[2]) > 200 || strlen(argv[4]) > 200)
	{
		printf("usage: verify_c_file <level> <object name> <the .c file> <the typed name>\n");
		return 2;
	}

	std::string const objectName = argv[2], name = argv[4];
	OpenLevelFile file;

	Check(file.Open(argv[1]) == LevelOpenResult::Opened, "the level opens");

	std::ifstream in(argv[3], std::ios::binary);
	std::stringstream read;

	read << in.rdbuf();

	std::string const text = read.str();

	Check(in.good() || !text.empty(), std::string("the file ") + argv[3] + " reads (" + std::to_string(text.size()) + " bytes)");

	if (failed)
	{
		return 1;
	}

	Object_Chunk *object = nullptr;
	List<Object_Chunk *> list;

	file.Level()->list_objects(&list);

	for (LIF<Object_Chunk *> each(&list); !each.done() && !object; each.next())
	{
		if (each() && strncmp(each()->object_data.o_name, objectName.c_str(), sizeof(each()->object_data.o_name)) == 0)
		{
			object = each();
		}
	}

	model::ShapeModel shape;
	std::string why;
	uint8_t id[8];

	Check(object != nullptr, "an object is named " + objectName);

	if (failed)
	{
		return 1;
	}

	memcpy(id, &object->object_data.ID, 8);
	Check(host::ShapeEdit_Read(file.Level(), &file, id, &shape, nullptr, &why) == host::ShapeEditResult::Done, "its shape reads");

	if (failed)
	{
		return 1;
	}

	// the level's scale (ENVSDSCL, 1.0 without one), the object's place times it, cut toward zero
	double scale = 1.0;

	if (Chunk_With_Children *const environment = (Chunk_With_Children *)file.Level()->lookup_single_child("REBENVDT"))
	{
		if (Environment_Scale_Chunk *const scaleChunk = dynamic_cast<Environment_Scale_Chunk *>(environment->lookup_single_child("ENVSDSCL")))
		{
			scale = scaleChunk->scale;
		}
	}

	std::string const place = "{" + std::to_string((int32_t)(object->object_data.location.x * scale)) + "," + std::to_string((int32_t)(object->object_data.location.y * scale)) +
		"," + std::to_string((int32_t)(object->object_data.location.z * scale)) + "}  " + objectName + "\n";
	std::string const header = "SHAPEHEADER " + name + "_header=\n{\n\t" + std::to_string(shape.points.size()) + ",\n\t" + std::to_string(shape.polygons.size()) + ",\n\t0,\n";

	Check(text.compare(0, 21, "/* object locations:\n") == 0, "it starts with /* object locations:");
	Check(text.find(place) != std::string::npos, "it names the object at its place times the scale (" + std::to_string(scale) + "): " + place.substr(0, place.size() - 1));
	Check(text.find("#define " + name + "_pscale 1\n") != std::string::npos, "#define " + name + "_pscale 1");
	Check(text.find(header) != std::string::npos, "the header's counts: " + std::to_string(shape.points.size()) + " points, " + std::to_string(shape.polygons.size()) + " polygons");

	size_t items = 0;

	for (size_t i = 0; i <= shape.polygons.size(); i++)
	{
		items += text.find("int " + name + "_item" + std::to_string(i) + "[]=\n{\n") != std::string::npos ? 1 : 0;
	}

	Check(items == shape.polygons.size(), std::to_string(items) + " items, one a polygon");

	bool textured = false;

	for (model::ShapePolygon const &p : shape.polygons)
	{
		textured = textured || Textured(p.engineType);
	}

	if (textured)
	{
		size_t const list = text.find("char *" + name + "_localtextures[]=\n{\n");

		Check(list != std::string::npos && text.size() >= 6 && text.compare(text.size() - 6, 6, "\t0\n};\n") == 0, "the picture list closes the file");
		Check(text.find("char " + name + "_texture0[]={\"") != std::string::npos, "the first picture is defined");
	}

	printf(failed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return failed ? 1 : 0;
}
