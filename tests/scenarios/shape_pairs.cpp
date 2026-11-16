// shape_pairs.cpp: a survey for the Copy Texturing > Object scenario: pairs of objects whose shapes differ but have the same number of polygons and, polygon by polygon, the same corner point numbers. Usage: shape_pairs.exe <level>.
// Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
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
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "shape_edit.h"
#include "uv_edit.h"

HANDLE avp_CreateFile(const char *n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE m) { return CreateFileA(n, a, s, sa, c, f, m); }
DWORD avp_GetFileAttributes(const char *n) { return GetFileAttributesA(n); }
void avp_MessageBox(const char *m, int) { printf("  the game's code says: %s\n", m ? m : ""); }
void avp_exit(int c) { printf("  FAILED  the game's code stopped the program (code %d)\n", c); exit(c ? c : 1); }
void LogErrorStringC(const char *) {}
void Sound_ForgetAddressesInside(const void *, size_t) {}

struct Entry
{
	std::string name;
	std::string id;
	model::ShapeModel shape;
	Shape_Chunk *chunk;
	int loc[3];
	int row;
};

int main(int argc, char **argv)
{
	OpenLevelFile file;

	if (argc < 2 || file.Open(argv[1]) != LevelOpenResult::Opened)
	{
		printf("usage: shape_pairs <level>\n");
		return 1;
	}

	List<Object_Chunk *> list;
	file.Level()->list_objects(&list);

	std::vector<Entry> entries;
	std::set<Shape_Chunk *> seen;

	int row = -1;

	for (LIF<Object_Chunk *> each(&list); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

		row++;

		if (!shape || seen.count(shape) || shape->lookup_single_child("SHPEXTFL"))
		{
			continue;
		}

		seen.insert(shape);

		Entry e;
		std::string why;

		e.name = object->object_data.o_name;
		e.id.assign((char const *)&object->object_data.ID, 8);
		e.chunk = shape;
		e.row = row;
		e.loc[0] = object->object_data.location.x;
		e.loc[1] = object->object_data.location.y;
		e.loc[2] = object->object_data.location.z;

		if (host::ShapeEdit_Read(file.Level(), &file, (uint8_t const *)e.id.data(), &e.shape, nullptr, &why) == host::ShapeEditResult::Done && e.shape.polygons.size() >= 4)
		{
			entries.push_back(e);
		}
	}

	if (argc >= 3 && strcmp(argv[2], "plain") == 0)
	{
		std::map<unsigned, size_t> colours;
		std::map<int, size_t> types;

		for (Entry const &e : entries)
		{
			static int trianglesShown = 0;

			for (size_t index = 0; index < e.shape.polygons.size(); index++)
			{
				model::ShapePolygon const &p = e.shape.polygons[index];

				types[p.engineType]++;

				if (!model::EngineTypeIsTextured(p.engineType))
				{
					colours[(unsigned)p.colour]++;

					if (p.engineType == 2 && p.cornerCount == 3 && trianglesShown < 6)
					{
						trianglesShown++;
						printf("a plain triangle: %s (row %d) polygon %zu (shape has %zu polygons)\n", e.name.c_str(), e.row, index, e.shape.polygons.size());
					}
				}
			}
		}

		for (auto const &t : types) printf("type %d: %zu polygons\n", t.first, t.second);

		int n = 0;

		for (auto const &c : colours)
		{
			if (n++ < 24) printf("plain colour word %08x: %zu polygons\n", c.first, c.second);
		}

		printf("%zu distinct plain colour words\n", colours.size());
		return 0;
	}

	if (argc >= 3)
	{
		for (Entry const &e : entries)
		{
			if (e.name != argv[2])
			{
				continue;
			}

			printf("%s (row %d): %zu polygons\n", e.name.c_str(), e.row, e.shape.polygons.size());

			for (size_t p = 0; p < e.shape.polygons.size() && p < 24; p++)
			{
				model::ShapePolygon const &a = e.shape.polygons[p];

				printf("  polygon %zu: %d corners, type %d, neighbours across an edge:", p, a.cornerCount, a.engineType);

				for (size_t q = 0; q < e.shape.polygons.size(); q++)
				{
					model::ShapePolygon const &b = e.shape.polygons[q];
					int shared = 0;

					for (int i = 0; q != p && i < a.cornerCount; i++)
					{
						for (int j = 0; j < b.cornerCount; j++)
						{
							shared += a.corners[i] == b.corners[j] ? 1 : 0;
						}
					}

					if (shared >= 2)
					{
						printf(" %zu", q);
					}
				}

				printf("\n");
			}

			return 0;
		}
	}

	int shown = 0;

	for (size_t a = 0; a < entries.size() && shown < 12; a++)
	{
		for (size_t b = a + 1; b < entries.size() && shown < 12; b++)
		{
			Entry const &x = entries[a], &y = entries[b];

			if (x.shape.polygons.size() != y.shape.polygons.size())
			{
				continue;
			}

			size_t same = 0, textured = 0;

			for (size_t i = 0; i < x.shape.polygons.size(); i++)
			{
				model::ShapePolygon const &p = x.shape.polygons[i], &q = y.shape.polygons[i];
				bool eq = p.cornerCount == q.cornerCount;

				for (int k = 0; eq && k < p.cornerCount; k++)
				{
					eq = p.corners[k] == q.corners[k];
				}

				same += eq ? 1 : 0;
				textured += (eq && model::EngineTypeIsTextured(p.engineType) && model::EngineTypeIsTextured(q.engineType)) ? 1 : 0;
			}

			if (same == x.shape.polygons.size() && textured > 0)
			{
				printf("%s (row %d) at %d %d %d and %s (row %d) at %d %d %d : %zu polygons, all with the same corner points, %zu textured in both\n", x.name.c_str(), x.row, x.loc[0], x.loc[1], x.loc[2], y.name.c_str(), y.row, y.loc[0], y.loc[1], y.loc[2], x.shape.polygons.size(), textured);
				shown++;
			}
		}
	}

	printf("%zu shapes surveyed, %d pairs shown\n", entries.size(), shown);
	return 0;
}
