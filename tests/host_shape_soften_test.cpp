// The test of Edge > Soft (src/model/shape_soften.*): on a real level every module's shape has its vertex normals made again for each angle 0 to 180 (inteng's Soft, copied from the decompile) and compared with the
// vertex normals the level holds. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_shape_soften_test.exe <a level .rif>. With no argument it says so and passes. The level is only read.
// WHAT IT SAYS: how many shapes some angle reproduces exactly (every point's normal bit for bit), and which angles those are: a FACT about how the stock shapes were made, not a gate. CHECKS: an angle outside 0 to 180 is refused and
// changes nothing; a shape with no vertex normals is refused; Soft of a cube at 0 gives each point the sum of its three faces (a unit vector along the diagonal) only when the faces are cut at an angle above 90 degrees, as the
// threshold works.

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
#include "shape_soften_edit.h"

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

// the largest difference of any part of any point's normal
static double Worst(std::vector<model::ShapeNormal> const &a, std::vector<model::ShapeNormal> const &b)
{
	double worst = 0;

	for (size_t i = 0; i < a.size() && i < b.size(); i++)
	{
		double const d[3] = { (double)a[i].x - b[i].x, (double)a[i].y - b[i].y, (double)a[i].z - b[i].z };

		for (int k = 0; k < 3; k++)
		{
			worst = (d[k] < 0 ? -d[k] : d[k]) > worst ? (d[k] < 0 ? -d[k] : d[k]) : worst;
		}
	}

	return worst;
}

int main(int argc, char **argv)
{
	// a cube: eight points, six quads
	{
		model::ShapeModel cube;
		int32_t const corner[8][3] = { { -1000, -1000, -1000 }, { 1000, -1000, -1000 }, { 1000, 1000, -1000 }, { -1000, 1000, -1000 }, { -1000, -1000, 1000 }, { 1000, -1000, 1000 }, { 1000, 1000, 1000 }, { -1000, 1000, 1000 } };
		int const faces[6][4] = { { 0, 1, 2, 3 }, { 5, 4, 7, 6 }, { 4, 0, 3, 7 }, { 1, 5, 6, 2 }, { 3, 2, 6, 7 }, { 4, 5, 1, 0 } };
		float const normals[6][3] = { { 0, 0, -1 }, { 0, 0, 1 }, { -1, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 } };

		for (int i = 0; i < 8; i++)
		{
			model::ShapePoint point = { corner[i][0], corner[i][1], corner[i][2] };
			cube.points.push_back(point);
			model::ShapeNormal zero = { 0, 0, 0 };
			cube.pointNormals.push_back(zero);
		}

		for (int f = 0; f < 6; f++)
		{
			model::ShapePolygon polygon = {};
			polygon.cornerCount = 4;

			for (int k = 0; k < 4; k++)
			{
				polygon.corners[k] = faces[f][k];
			}

			cube.polygons.push_back(polygon);
			model::ShapeNormal normal = { normals[f][0], normals[f][1], normals[f][2] };
			cube.polygonNormals.push_back(normal);
		}

		model::ShapeModel flat = cube;
		size_t changed = 99;

		Check(!model::SoftenShape(&flat, 181, 1.0, &changed) && !model::SoftenShape(&flat, -1, 1.0, &changed) && Same(flat.pointNormals, cube.pointNormals), "an angle outside 0 to 180 is refused and changes nothing");

		model::ShapeModel none = cube;
		none.pointNormals.clear();
		Check(!model::SoftenShape(&none, 90, 1.0, &changed), "a shape with no vertex normals is refused");

		// the threshold is the cosine of the typed angle: 0 -> 1, 45 -> 0.7071, 90 -> 0, 180 -> -1
		Check(model::SoftenThreshold(0) == 1.0 && fabs(model::SoftenThreshold(45) - 0.70710678118654752) < 1e-6 && fabs(model::SoftenThreshold(90)) < 1e-6 && fabs(model::SoftenThreshold(180) + 1.0) < 1e-6,
			"the threshold is the cosine of the typed angle (inteng's constants 1/360 and 2 pi)");

		// at 0 every face is cut from the others at a corner: the three faces of a point stay apart, and as they are at one point (the same point joins with no angle test) the point's normal is all three added up
		model::ShapeModel zero = cube;
		Check(model::SoftenShape(&zero, 0, 1.0, &changed) && changed == 8, "Soft at 0 changes all 8 points of the cube");

		double const diagonal = 0.57735026918962576;

		Check(fabs(zero.pointNormals[6].x - diagonal) < 1e-6 && fabs(zero.pointNormals[6].y - diagonal) < 1e-6 && fabs(zero.pointNormals[6].z - diagonal) < 1e-6,
			"a point of the cube is given the unit vector along the sum of its three faces' normals (the same point joins its own group with no angle test)");

		model::ShapeModel wide = cube;
		model::ShapeModel again = cube;
		Check(model::SoftenShape(&wide, 90, 1.0, &changed) && model::SoftenShape(&again, 90, 1.0, &changed) && Same(wide.pointNormals, again.pointNormals), "the same angle gives the same normals every time");
	}

	if (argc < 2)
	{
		printf("host_shape_soften_test: give a level .rif for the oracle; the cube checks are done.\n");
		printf("%s\n", checksFailed ? "FAILED host_shape_soften_test" : "passed host_shape_soften_test");
		return checksFailed ? 1 : 0;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level\n");
		return 1;
	}

	double scale = 1.0;
	{
		// the level's scale: ENVSDSCL, as the lighting reads it
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
				scale = value > 0.0 ? value : 1.0;
			}
		}
	}

	std::set<int32_t> modules;
	host::ModuleIndexes(level.Level(), &modules);

	List<Object_Chunk *> all;
	level.Level()->list_objects(&all);

	size_t shapes = 0, exact = 0, withNormals = 0, almost = 0;
	std::map<int, size_t> byAngle;
	std::set<void *> seen;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !modules.count((int32_t)object->object_data.index_num) || !object->get_assoc_shape() || seen.count(object->get_assoc_shape()))
		{
			continue;
		}

		seen.insert(object->get_assoc_shape());

		model::ShapeModel shape;
		std::string why;

		if (host::ShapeEdit_Read(level.Level(), &level, (uint8_t const *)&object->object_data.ID, &shape, nullptr, &why) != host::ShapeEditResult::Done)
		{
			continue;
		}

		shapes++;

		if (shape.pointNormals.size() != shape.points.size() || shape.pointNormals.empty())
		{
			continue;
		}

		withNormals++;

		bool found = false;
		bool closeEnough = false;

		for (int angle = 0; angle <= 180 && !found; angle++)
		{
			model::ShapeModel trial = shape;

			if (model::SoftenShape(&trial, angle, scale, nullptr))
			{
				if (Same(trial.pointNormals, shape.pointNormals))
				{
					found = true;
					byAngle[angle]++;
				}
				else if (Worst(trial.pointNormals, shape.pointNormals) < 0.001)
				{
					closeEnough = true;
				}
			}
		}

		exact += found ? 1 : 0;
		almost += (!found && closeEnough) ? 1 : 0;
	}

	printf("  info  %zu module shapes, %zu with vertex normals: %zu are reproduced bit for bit by some angle, %zu more to within 0.001\n", shapes, withNormals, exact, almost);

	std::string list;

	for (std::map<int, size_t>::const_iterator at = byAngle.begin(); at != byAngle.end(); ++at)
	{
		list += " " + std::to_string(at->first) + "deg x" + std::to_string(at->second);
	}

	printf("  info  angles that reproduce them:%s\n", list.c_str());

	Check(withNormals > 0, "the level has shapes with vertex normals");

	// The edit path: the angle as inteng reads it, and a shape's normals made again in the open level, through the loader, as a copy
	{
		int angle = -1;

		Check(host::ShapeSoften_ReadAngle("45", &angle) && angle == 45 && host::ShapeSoften_ReadAngle("  0 degrees", &angle) && angle == 0 && host::ShapeSoften_ReadAngle("180", &angle) && angle == 180,
			"the typed angle is read as sscanf %d reads it: a number first, what follows it is not looked at");
		Check(!host::ShapeSoften_ReadAngle("181", &angle) && !host::ShapeSoften_ReadAngle("-1", &angle) && !host::ShapeSoften_ReadAngle("soft", &angle) && !host::ShapeSoften_ReadAngle("", &angle) &&
			!host::ShapeSoften_ReadAngle(std::string(300, '1'), &angle), "181, -1, words, nothing and a text of 300 letters are refused");

		bool edited = false;

		for (LIF<Object_Chunk *> each(&all); !each.done() && !edited; each.next())
		{
			Object_Chunk *const object = each();

			if (!object || !modules.count((int32_t)object->object_data.index_num) || !object->get_assoc_shape())
			{
				continue;
			}

			model::ShapeModel before;
			std::string why;

			if (host::ShapeEdit_Read(level.Level(), &level, (uint8_t const *)&object->object_data.ID, &before, nullptr, &why) != host::ShapeEditResult::Done || before.flags & 0x1000)
			{
				continue;
			}

			model::ShapeModel expected = before;
			size_t expectedChanged = 0;

			if (!model::SoftenShape(&expected, 0, scale, &expectedChanged) || expectedChanged < 4)
			{
				continue;
			}

			size_t changed = 0;
			host::ShapeSoftenResult const result = host::ShapeSoften_ByAngle(&level, level.Level(), (uint8_t const *)&object->object_data.ID, 0, scale, &changed, &why);

			Check(result == host::ShapeSoftenResult::Done && changed == expectedChanged, std::string("Soft at 0 on object '") + object->object_data.o_name + "' is done: " + std::to_string(changed) + " points have a new normal");

			model::ShapeModel after;
			Check(host::ShapeEdit_Read(level.Level(), &level, (uint8_t const *)&object->object_data.ID, &after, nullptr, &why) == host::ShapeEditResult::Done && Same(after.pointNormals, expected.pointNormals),
				"the level holds the made-again normals, every point");
			Check(after.points.size() == before.points.size() && Same(after.polygonNormals, before.polygonNormals), "and the points and the polygon normals are as they were");
			Check(host::ShapeSoften_ByAngle(&level, level.Level(), (uint8_t const *)&object->object_data.ID, 181, scale, nullptr, &why) == host::ShapeSoftenResult::BadAngle, "an angle of 181 is refused");

			std::string const folder = argc > 2 ? argv[2] : "";

			if (!folder.empty())
			{
				std::string const copyPath = folder + (folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + "soften-test-copy.rif";

				CreateDirectoryA(folder.c_str(), NULL);
				DeleteFileA(copyPath.c_str());
				Check(level.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, "the level is saved as a COPY");

				OpenLevelFile reopened;
				model::ShapeModel again;

				Check(reopened.Open(copyPath.c_str()) == LevelOpenResult::Opened && host::ShapeEdit_Read(reopened.Level(), &reopened, (uint8_t const *)&object->object_data.ID, &again, nullptr, &why) == host::ShapeEditResult::Done &&
					Same(again.pointNormals, expected.pointNormals), "the game's loader takes the copy and it holds the made-again normals");
				DeleteFileA(copyPath.c_str());
				DeleteFileA((copyPath + ".bak").c_str());
			}

			edited = true;
		}

		Check(edited, "the level has a shape Soft at 0 changes");
	}

	if (checksFailed)
	{
		printf("FAILED host_shape_soften_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_shape_soften_test\n");
	return 0;
}
