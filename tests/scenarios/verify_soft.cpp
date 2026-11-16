// verify_soft: the check of the Edge > Soft scenario (tests/scenarios/light-soft.txt): the level the hidden editor run saved as a copy, read with the game's own loader. Written 2026-11-15 by Melanikus (Drake)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_soft.exe <the level the editor opened> <the copy it saved> <the angle typed>
// CHECKS: both levels are read by the game's loader; of the module shapes, at least one has other vertex normals in the copy than in the original, and every shape that has is exactly what Soft (model::SoftenShape,
// the angle typed) makes of the original's: its points and polygon normals are the original's; every other shape is byte for byte the original's. Gives back 0 only if every check passed.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "shape_edit.h"
#include "shape_soften.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

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

static bool Same(std::vector<model::ShapeNormal> const &a, std::vector<model::ShapeNormal> const &b)
{
	if (a.size() != b.size())
	{
		return false;
	}

	for (size_t i = 0; i < a.size(); i++)
	{
		if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].z != b[i].z)
		{
			return false;
		}
	}

	return true;
}

static double ScaleOf(OpenLevelFile &level)
{
	Chunk *const env = level.Level()->lookup_single_child("REBENVDT");
	Chunk_With_Children *const holder = env && !env->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(env) : nullptr;
	std::vector<unsigned char> bytes;

	if (holder)
	{
		Chunk *const s = holder->lookup_single_child("ENVSDSCL");

		if (s && level.PayloadOf(s, &bytes) && bytes.size() >= 8)
		{
			double value;
			memcpy(&value, bytes.data(), 8);
			return value > 0.0 ? value : 1.0;
		}
	}

	return 1.0;
}

int main(int argc, char **argv)
{
	if (argc < 4)
	{
		printf("verify_soft: give the level, its copy and the angle typed\n");
		return 2;
	}

	int const angle = atoi(argv[3]);
	OpenLevelFile original, copy;

	Check(original.Open(argv[1]) == LevelOpenResult::Opened && original.Level(), "the game's loader takes the original level");
	Check(copy.Open(argv[2]) == LevelOpenResult::Opened && copy.Level(), "the game's loader takes the copy");

	if (!original.Level() || !copy.Level())
	{
		return 1;
	}

	double const scale = ScaleOf(original);
	std::set<int32_t> modules;
	host::ModuleIndexes(original.Level(), &modules);

	List<Object_Chunk *> all;
	original.Level()->list_objects(&all);

	size_t shapes = 0, changedShapes = 0, wrong = 0, geometryMoved = 0;
	std::set<void *> seen;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !modules.count((int32_t)object->object_data.index_num) || !object->get_assoc_shape() || seen.count(object->get_assoc_shape()))
		{
			continue;
		}

		seen.insert(object->get_assoc_shape());

		model::ShapeModel was, now;
		std::string why;

		if (host::ShapeEdit_Read(original.Level(), &original, (uint8_t const *)&object->object_data.ID, &was, nullptr, &why) != host::ShapeEditResult::Done ||
			host::ShapeEdit_Read(copy.Level(), &copy, (uint8_t const *)&object->object_data.ID, &now, nullptr, &why) != host::ShapeEditResult::Done)
		{
			continue;
		}

		shapes++;

		if (was.points.size() != now.points.size() || Same(now.polygonNormals, was.polygonNormals) == false)
		{
			geometryMoved++;
			continue;
		}

		for (size_t v = 0; v < was.points.size(); v++)
		{
			geometryMoved += (was.points[v].x != now.points[v].x || was.points[v].y != now.points[v].y || was.points[v].z != now.points[v].z) ? 1 : 0;
		}

		if (!Same(now.pointNormals, was.pointNormals))
		{
			changedShapes++;

			model::ShapeModel expected = was;

			if (!model::SoftenShape(&expected, angle, scale, nullptr) || !Same(now.pointNormals, expected.pointNormals))
			{
				wrong++;
			}
		}
	}

	printf("  info  %zu module shapes read, %zu have other vertex normals in the copy\n", shapes, changedShapes);
	Check(changedShapes >= 1, "at least one shape has been made soft in the copy");
	Check(wrong == 0, "every shape that changed is exactly what Soft at " + std::to_string(angle) + " degrees makes of the original's");
	Check(geometryMoved == 0, "no point and no polygon normal of any shape moved");

	if (checksFailed)
	{
		printf("FAILED verify_soft (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed verify_soft\n");
	return 0;
}
