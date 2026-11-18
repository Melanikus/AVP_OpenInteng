// verify_nodes.cpp: the check of the Texture Map's O / V (cycle / reverse nodes, TM-C H5) on a saved copy. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_nodes.exe <the level before the step> <the level after it> <cycle|reverse>
// CHECKS: the game's loader takes both; at least one polygon's texture coordinates changed; every polygon whose coordinates changed holds exactly what src/model/uv_nodes.h makes of what it held
// before: a polygon alone (no partner in the shape's merge data) its own nodes turned, a merged pair the two turned as one quad (model::TextureQuadOf), a file quad its four; nothing else of
// any polygon (its picture, flags, type) changed. Each shape is looked at once (by its first object). Gives back 0 only if every check passed.

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
#include "shape_edit.h"
#include "uv_edit.h"
#include "polygon_items.h"
#include "texture_quad.h"
#include "uv_model.h"
#include "uv_nodes.h"

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

static bool SameNodes(model::PolygonUv const &a, model::PolygonUv const &b)
{
	if (a.uv.cornerCount != b.uv.cornerCount)
	{
		return false;
	}

	for (int c = 0; c < a.uv.cornerCount; c++)
	{
		if (a.uv.corners[c].u != b.uv.corners[c].u || a.uv.corners[c].v != b.uv.corners[c].v)
		{
			return false;
		}
	}

	return true;
}

static std::string Nodes(model::PolygonUv const &a)
{
	std::string out;

	for (int c = 0; c < a.uv.cornerCount; c++)
	{
		out += (c ? " " : "") + std::to_string((int)a.uv.corners[c].u) + "," + std::to_string((int)a.uv.corners[c].v);
	}

	return out;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 4 || (strcmp(argv[3], "cycle") != 0 && strcmp(argv[3], "reverse") != 0))
	{
		printf("usage: verify_nodes <the level before> <the level after> <cycle|reverse>\n");
		return 2;
	}

	model::NodeTurn const turn = strcmp(argv[3], "cycle") == 0 ? model::NodeTurn::Cycle : model::NodeTurn::Reverse;
	OpenLevelFile before, after;

	Check(before.Open(argv[1]) == LevelOpenResult::Opened && before.Level(), "the game's loader takes the level before");
	Check(after.Open(argv[2]) == LevelOpenResult::Opened && after.Level(), "the game's loader takes the level after");

	if (!before.Level() || !after.Level())
	{
		return 1;
	}

	List<Object_Chunk *> objects;

	before.Level()->list_objects(&objects);

	std::set<int32_t> seenShapes;
	size_t changed = 0, right = 0, otherChanged = 0;
	std::string firstWrong;

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		uint8_t const *const id = each() ? (uint8_t const *)&each()->object_data.ID : nullptr;
		model::ShapeModel shapeBefore, shapeAfter;
		std::string why;

		if (!id || host::ShapeEdit_Read(before.Level(), &before, id, &shapeBefore, nullptr, &why) != host::ShapeEditResult::Done ||
			host::ShapeEdit_Read(after.Level(), &after, id, &shapeAfter, nullptr, &why) != host::ShapeEditResult::Done || !seenShapes.insert(shapeBefore.number).second ||
			shapeBefore.polygons.size() != shapeAfter.polygons.size())
		{
			continue;
		}

		std::vector<int32_t> merge;
		bool hasMerge = false;

		host::PolygonItems_ReadMergeData(&before, before.Level(), id, &merge, &hasMerge);

		std::vector<model::PolygonUv> was(shapeBefore.polygons.size()), now(shapeBefore.polygons.size());
		std::vector<bool> readable(shapeBefore.polygons.size(), false);

		for (size_t p = 0; p < shapeBefore.polygons.size(); p++)
		{
			std::string a, b;

			readable[p] = host::UvEdit_ReadPolygon(&before, before.Level(), id, (int32_t)p, &was[p], &a) == host::UvEditResult::Done &&
				host::UvEdit_ReadPolygon(&after, after.Level(), id, (int32_t)p, &now[p], &b) == host::UvEditResult::Done;

			if (readable[p] && (was[p].colour != now[p].colour || was[p].flags != now[p].flags || was[p].engineType != now[p].engineType))
			{
				otherChanged++;
			}
		}

		std::vector<bool> checked(shapeBefore.polygons.size(), false);

		for (size_t p = 0; p < shapeBefore.polygons.size(); p++)
		{
			if (!readable[p] || checked[p] || SameNodes(was[p], now[p]))
			{
				continue;
			}

			int32_t const partner = was[p].cornerCount == 3 && hasMerge && p < merge.size() ? merge[p] : -1;
			model::TextureQuad quad = {};
			bool const pair = partner >= 0 && (size_t)partner < was.size() && readable[(size_t)partner] && model::TextureQuadOf(shapeBefore, (int32_t)p, partner, &quad) == model::TextureQuadResult::Ok;

			if (pair)
			{
				size_t const low = (size_t)p < (size_t)partner ? (size_t)p : (size_t)partner;
				size_t const high = low == (size_t)p ? (size_t)partner : (size_t)p;
				model::PolygonUv a = was[low], b = was[high];
				model::TextureQuad lowQuad = {};

				model::TextureQuadOf(shapeBefore, (int32_t)low, (int32_t)high, &lowQuad);
				model::UvNodes_Quad(lowQuad, a.uv.corners, b.uv.corners, turn);
				checked[low] = checked[high] = true;
				changed += 2;

				bool const good = SameNodes(a, now[low]) && SameNodes(b, now[high]);

				right += good ? 2 : 0;

				if (!good && firstWrong.empty())
				{
					firstWrong = "the pair " + std::to_string(low) + " + " + std::to_string(high) + " of shape " + std::to_string(shapeBefore.number) + " holds " + Nodes(now[low]) + " / " +
						Nodes(now[high]) + ", wanted " + Nodes(a) + " / " + Nodes(b);
				}

				continue;
			}

			model::PolygonUv a = was[p];

			model::UvNodes_Alone(a.uv.corners, was[p].cornerCount, turn);
			checked[p] = true;
			changed++;

			bool const good = SameNodes(a, now[p]);

			right += good ? 1 : 0;

			if (!good && firstWrong.empty())
			{
				firstWrong = "polygon " + std::to_string(p) + " of shape " + std::to_string(shapeBefore.number) + " holds " + Nodes(now[p]) + ", wanted " + Nodes(a) + " (it held " + Nodes(was[p]) + ")";
			}
		}
	}

	Check(changed > 0, "some polygon's texture coordinates changed (" + std::to_string(changed) + ")");
	Check(right == changed, std::string("each changed polygon holds what ") + argv[3] + " makes of what it held, alone or as a merged pair (" + std::to_string(right) + " of " +
		std::to_string(changed) + (firstWrong.empty() ? ")" : "; " + firstWrong + ")"));
	Check(otherChanged == 0, "no polygon's picture, flags or type changed (" + std::to_string(otherChanged) + ")");

	if (failed)
	{
		printf("FAILED verify_nodes (%d)\n", failed);
		return 1;
	}

	printf("PASSED verify_nodes\n");
	return 0;
}
