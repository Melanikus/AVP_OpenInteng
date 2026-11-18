// verify_uv_turn: the check of acceptance-texture-turn-mirror (Texture Map's O and V: FUN_00571310 / FUN_00571910). Written 2026-11-18 by Melanikus (Frost) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_uv_turn.exe <before> <after> <object name> <polygon> <turn|mirror> [<partner>]
//   The object's shape (the first object with that name) in both levels. Alone (no partner given): the polygon's texture coordinates in <after> are those of <before>
//   turned one corner on (turn: corner k takes corner k + 1's, the last the first's) or mirrored (mirror: a triangle keeps corner 0 and swaps 1 and 2). With a partner:
//   the two triangles as ONE quad in inteng's order (model::TextureQuadOf, FUN_0055cdf0): turn = quad corner k takes k + 1's and corner 3 the old 0's; mirror = 0 <-> 3
//   and 1 <-> 2; each corner in both triangles where it is theirs. Every other polygon's coordinates are as they were. Whole numbers are compared.
// Gives back 0 when it holds, 1 when it doesn't, 2 for a bad call.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "shape_edit.h"
#include "texture_quad.h"
#include "uv_edit.h"

#include "arg_number.h"

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

struct Read
{
	OpenLevelFile file;
	model::ShapeModel shape;
	std::vector<model::PolygonUv> uv;
};

static bool ReadLevel(char const *path, char const *name, Read *out)
{
	std::string why;

	if (out->file.Open(path) != LevelOpenResult::Opened)
	{
		return false;
	}

	uint8_t id[8];
	bool have = false;
	List<Object_Chunk *> list;

	out->file.Level()->list_objects(&list);

	for (LIF<Object_Chunk *> each(&list); !each.done() && !have; each.next())
	{
		Object_Chunk *const object = each();

		if (object && strlen(name) < sizeof(object->object_data.o_name) && strncmp(object->object_data.o_name, name, sizeof(object->object_data.o_name)) == 0)
		{
			memcpy(id, &object->object_data.ID, 8);
			have = true;
		}
	}

	return have && host::ShapeEdit_Read(out->file.Level(), &out->file, id, &out->shape, nullptr, &why) == host::ShapeEditResult::Done &&
		host::UvEdit_ReadAllPolygons(&out->file, out->file.Level(), id, &out->uv, &why) == host::UvEditResult::Done && out->uv.size() == out->shape.polygons.size();
}

static bool SameCorner(model::UvCorner const &a, model::UvCorner const &b)
{
	return trunc(a.u) == trunc(b.u) && trunc(a.v) == trunc(b.v);
}

int main(int argc, char **argv)
{
	if (argc < 6 || argc > 7 || strlen(argv[3]) > 200 || (strcmp(argv[5], "turn") != 0 && strcmp(argv[5], "mirror") != 0))
	{
		printf("usage: verify_uv_turn <before> <after> <object name> <polygon> <turn|mirror> [<partner>]\n");
		return 2;
	}

	long polygon = 0, partner = -1;

	if (!ArgNumber("polygon", argv[4], 0, 65535, &polygon) || (argc == 7 && !ArgNumber("partner", argv[6], 0, 65535, &partner)))
	{
		return 2;
	}

	bool const mirror = strcmp(argv[5], "mirror") == 0;
	Read before, after;

	Check(ReadLevel(argv[1], argv[3], &before), "the level before opens and its shape of " + std::string(argv[3]) + " reads");
	Check(ReadLevel(argv[2], argv[3], &after), "the level after opens and its shape reads");

	if (failed)
	{
		return 1;
	}

	size_t const n = before.uv.size();

	Check(after.uv.size() == n && (size_t)polygon < n && (partner < 0 || (size_t)partner < n), "both shapes have the polygons");

	if (failed)
	{
		return 1;
	}

	// what each polygon should hold after
	std::vector<model::PolygonUv> want = before.uv;

	if (partner < 0)
	{
		model::UvList const &was = before.uv[(size_t)polygon].uv;
		model::UvList &next = want[(size_t)polygon].uv;
		int const count = was.cornerCount;

		Check(before.uv[(size_t)polygon].textured && count >= 3, "the polygon is textured, with " + std::to_string(count) + " corners");

		for (int k = 0; k < count && !failed; k++)
		{
			next.corners[k] = mirror ? (count == 3 ? was.corners[k == 0 ? 0 : 3 - k] : was.corners[count - 1 - k]) : was.corners[(k + 1) % count];
		}
	}
	else
	{
		model::TextureQuad quad;

		Check(model::TextureQuadOf(before.shape, (int32_t)polygon, (int32_t)partner, &quad) == model::TextureQuadResult::Ok, "the two are one quad");

		if (failed)
		{
			return 1;
		}

		model::UvList const &a = before.uv[(size_t)polygon].uv;
		model::UvList const &b = before.uv[(size_t)partner].uv;
		model::UvCorner values[4];

		for (int k = 0; k < 4; k++)
		{
			bool const inA = quad.ownCorners[k] >= 0 && quad.ownCorners[k] < 3, inB = quad.partnerCorners[k] >= 0 && quad.partnerCorners[k] < 3;

			values[k] = (mirror && k == 3 ? !inB && inA : inA) ? a.corners[quad.ownCorners[k]] : b.corners[inB ? quad.partnerCorners[k] : 0];
		}

		for (int k = 0; k < 4; k++)
		{
			model::UvCorner const v = mirror ? values[3 - k] : values[(k + 1) % 4];

			if (quad.ownCorners[k] >= 0 && quad.ownCorners[k] < 3)
			{
				want[(size_t)polygon].uv.corners[quad.ownCorners[k]] = v;
			}

			if (quad.partnerCorners[k] >= 0 && quad.partnerCorners[k] < 3)
			{
				want[(size_t)partner].uv.corners[quad.partnerCorners[k]] = v;
			}
		}
	}

	size_t wrong = 0, othersChanged = 0;

	for (size_t p = 0; p < n; p++)
	{
		bool const named = (long)p == polygon || (long)p == partner;
		model::UvList const &got = after.uv[p].uv;
		model::UvList const &should = want[p].uv;
		bool alike = got.cornerCount == should.cornerCount && after.uv[p].textured == want[p].textured;

		for (int k = 0; alike && k < got.cornerCount; k++)
		{
			alike = SameCorner(got.corners[k], should.corners[k]);
		}

		if (named)
		{
			for (int k = 0; k < got.cornerCount && k < should.cornerCount; k++)
			{
				printf("    polygon %zu corner %d: %g %g (wanted %g %g, before %g %g)\n", p, k, trunc(got.corners[k].u), trunc(got.corners[k].v), trunc(should.corners[k].u),
					trunc(should.corners[k].v), trunc(before.uv[p].uv.corners[k].u), trunc(before.uv[p].uv.corners[k].v));
			}

			wrong += alike ? 0 : 1;
		}
		else
		{
			othersChanged += alike ? 0 : 1;
		}
	}

	Check(wrong == 0, std::string("the polygon") + (partner >= 0 ? "s are" : " is") + (mirror ? " mirrored" : " turned one corner on"));
	Check(othersChanged == 0, std::to_string(othersChanged) + " other polygons changed");
	printf(failed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return failed ? 1 : 0;
}
