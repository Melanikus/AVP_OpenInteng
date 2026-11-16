// verify_poly.cpp: the Poly edit scenarios' check. Usage: verify_poly.exe <original level> <saved copy> <delta> [<delta2>]: the game's own loader takes both; EXACTLY ONE shape of the level differs between them (a Delete polys flag taken out
// or a Create poly adds to one shape; with Delete 0-normal the number of shapes that differ is not fixed, give the word "many" instead of a delta); that shape has <delta> more polygons (negative: fewer); every
// polygon of the smaller one is in the bigger one (same corners in space, same type and flags); every other shape is the original's, number for number. Written 2026-11-15 by Melanikus (Hicks) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
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

// every shape of a level by its number
static bool ReadShapes(char const *path, std::map<int32_t, model::ShapeModel> *shapes)
{
	OpenLevelFile level;

	if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
	{
		return false;
	}

	List<Object_Chunk *> all;

	level.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		model::ShapeModel shape;
		std::string why;

		if (object && object->get_assoc_shape() && host::ShapeEdit_Read(level.Level(), &level, (uint8_t const *)&object->object_data.ID, &shape, nullptr, &why) == host::ShapeEditResult::Done)
		{
			(*shapes)[shape.number] = shape;
		}
	}

	return true;
}

static bool SamePolygon(model::ShapeModel const &a, model::ShapePolygon const &pa, model::ShapeModel const &b, model::ShapePolygon const &pb)
{
	if (pa.engineType != pb.engineType || pa.flags != pb.flags || pa.cornerCount != pb.cornerCount)
	{
		return false;
	}

	for (int c = 0; c < pa.cornerCount; c++)
	{
		if (pa.corners[c] < 0 || pb.corners[c] < 0 || (size_t)pa.corners[c] >= a.points.size() || (size_t)pb.corners[c] >= b.points.size() || memcmp(&a.points[(size_t)pa.corners[c]], &b.points[(size_t)pb.corners[c]], sizeof(model::ShapePoint)) != 0)
		{
			return false;
		}
	}

	return true;
}

int main(int argc, char **argv)
{
	if (argc < 4)
	{
		printf("usage: verify_poly.exe <original> <copy> <delta | many>\n");
		return 2;
	}

	std::map<int32_t, model::ShapeModel> before, after;

	Check(ReadShapes(argv[1], &before) && !before.empty(), "the game's loader takes the original");
	Check(ReadShapes(argv[2], &after) && !after.empty(), "the game's loader takes the saved copy");
	// "split<k>": Split Object As Placed Obj of k polygons: one shape more, exactly one old shape lost k polygons, and the new shape is those k polygons
	if (strncmp(argv[3], "split", 5) == 0)
	{
		long const k = atol(argv[3] + 5);

		Check(after.size() == before.size() + 1, "the copy has one shape more than the original (" + std::to_string(before.size()) + " -> " + std::to_string(after.size()) + ")");

		int32_t cut = -1, added = -1;
		size_t changed = 0;

		for (auto const &one : after)
		{
			auto const old = before.find(one.first);

			if (old == before.end())
			{
				added = one.first;
			}
			else if (old->second.polygons.size() != one.second.polygons.size() || old->second.points.size() != one.second.points.size() ||
				(!old->second.polygons.empty() && memcmp(old->second.polygons.data(), one.second.polygons.data(), old->second.polygons.size() * sizeof(old->second.polygons[0])) != 0))
			{
				cut = one.first;
				changed++;
			}
		}

		Check(added >= 0 && cut >= 0 && changed == 1, "one new shape and exactly one old shape changed");

		if (added >= 0 && cut >= 0)
		{
			model::ShapeModel const &was = before[cut];
			model::ShapeModel const &left = after[cut];
			model::ShapeModel const &fresh = after[added];
			bool inside = true;

			Check((long)fresh.polygons.size() == k && (long)was.polygons.size() - (long)left.polygons.size() == k, "the new shape has " + std::to_string(k) + " polygons, the old one " + std::to_string(k) + " fewer (" + std::to_string(was.polygons.size()) + " -> " + std::to_string(left.polygons.size()) + ")");

			for (model::ShapePolygon const &p : fresh.polygons)
			{
				bool found = false;

				for (model::ShapePolygon const &q : was.polygons)
				{
					found = found || SamePolygon(fresh, p, was, q);
				}

				inside = inside && found;
			}

			Check(inside, "every polygon of the new shape is one of the old shape's (same corners in space, type and flags)");
		}

		// an optional fourth word: the new object's name, which the copy must have exactly once
		if (argc > 4)
		{
			OpenLevelFile level;
			size_t named = 0;

			if (level.Open(argv[2]) == LevelOpenResult::Opened && level.Level())
			{
				List<Object_Chunk *> objects;

				level.Level()->list_objects(&objects);

				for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
				{
					named += each() && strncmp(each()->object_data.o_name, argv[4], sizeof(each()->object_data.o_name)) == 0 ? 1 : 0;
				}
			}

			Check(named == 1, std::string("the copy has exactly one object called ") + argv[4] + " (" + std::to_string(named) + ")");
		}

		if (failed)
		{
			printf("FAILED verify_poly (%d checks failed)\n", failed);
			return 1;
		}

		printf("passed verify_poly\n");
		return 0;
	}

	Check(before.size() == after.size(), "the copy has the original's number of shapes (" + std::to_string(before.size()) + ")");

	bool const any = strcmp(argv[3], "any") == 0;
	bool const many = strcmp(argv[3], "many") == 0 || any;
	long const delta = many ? 0 : atol(argv[3]);
	std::vector<int32_t> differ;

	for (auto const &one : before)
	{
		auto const other = after.find(one.first);

		if (other == after.end())
		{
			differ.push_back(one.first);
			continue;
		}

		model::ShapeModel const &a = one.second;
		model::ShapeModel const &b = other->second;
		bool same = a.points.size() == b.points.size() && a.polygons.size() == b.polygons.size() &&
			(a.points.empty() || memcmp(a.points.data(), b.points.data(), a.points.size() * sizeof(a.points[0])) == 0) &&
			(a.polygons.empty() || memcmp(a.polygons.data(), b.polygons.data(), a.polygons.size() * sizeof(a.polygons[0])) == 0);

		if (!same)
		{
			differ.push_back(one.first);
		}
	}

	if (many)
	{
		Check(any || !differ.empty(), std::to_string(differ.size()) + " shapes differ" + (any ? " (any number is right: Delete 0-normal may find none)" : ""));

		for (int32_t const number : differ)
		{
			Check(after.find(number) != after.end() && after[number].polygons.size() < before[number].polygons.size() && after[number].polygonNormals.size() == after[number].polygons.size(), "shape " + std::to_string(number) + " lost polygons and has one normal for each");
		}
	}
	else
	{
		Check(delta == 0 ? differ.empty() : differ.size() == 1, std::string(delta == 0 ? "no shape differs (" : "exactly one shape differs (") + std::to_string(differ.size()) + ")");

		if (differ.size() == 1)
		{
			model::ShapeModel const &a = before[differ[0]];
			model::ShapeModel const &b = after[differ[0]];
			model::ShapeModel const &fewerShape = delta < 0 ? b : a;
			model::ShapeModel const &moreShape = delta < 0 ? a : b;
			bool contained = true;

			Check((long)b.polygons.size() - (long)a.polygons.size() == delta, "that shape has " + std::to_string(delta) + " more polygons (" + std::to_string(a.polygons.size()) + " -> " + std::to_string(b.polygons.size()) + ")");

			for (model::ShapePolygon const &p : fewerShape.polygons)
			{
				bool found = false;

				for (model::ShapePolygon const &q : moreShape.polygons)
				{
					found = found || SamePolygon(fewerShape, p, moreShape, q);
				}

				contained = contained && found;
			}

			Check(contained, "every polygon of the smaller shape is in the bigger one (same corners in space, type and flags)");
			Check(b.polygonNormals.size() == b.polygons.size(), "one normal for each polygon in the copy");
		}
	}

	if (failed)
	{
		printf("FAILED verify_poly (%d checks failed)\n", failed);
		return 1;
	}

	printf("passed verify_poly\n");
	return 0;
}
