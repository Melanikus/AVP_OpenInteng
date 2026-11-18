// The test for the Poly edit group's first piece: Select As > Vertex and Create poly (src/host/poly_edit.*). Written 2026-11-15 by Melanikus (Hicks) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_poly_edit_test.exe <stock level .rif> ... <a folder it may write its copies into (made if it isn't there)>. With no arguments it says so and passes.
//
// A. THE NUMBERS (no level; references worked by hand from inteng's FUN_00564dc0 / FUN_00565310 / FUN_00564fc0 / FUN_00565460, AvP-docs\engine\inteng\vertex-mode-facts.md):
//  1. NearestCorner takes the smallest squared distance, the first of equals, skips corners that are not on the screen, and none farther than 1e9 squared;
//  2. QuadOrder: a scrambled flat square comes back going round (no two neighbours on a diagonal); a corner off the plane, three in a line, and a corner inside the triangle of
//     the other three are refused;
//  3. VertexClick: a plain click takes, again does nothing, another shape does nothing, Shift takes out and closes the gap, Shift on a corner not held does nothing, the fourth
//     corner that makes no quad is taken back, a fourth that does is ordered, a fifth click does nothing;
//  4. CreateInModel: three corners make one plain-colour triangle (type 2, colour 0x7fff, flags 0, normal number = its own number) with a unit normal that is square to both
//     edges; four make (0,1,3) and (1,2,3) and, with merge data, they name each other; a camera looking along the normal turns the polygon round (corners 2 and 3 swapped, normal
//     negated), one looking against it does not; three in a line, a corner twice, a corner the shape lacks and two corners are refused and change nothing.
// B. ON EACH STOCK LEVEL (the level is opened, a baseline copy saved with NO edit, the edit made, the edited copy saved, both copies loaded again with the game's own loader):
//  5. Create poly of a triangle and of a quad (the scrambled corners of a real quad, or of two paired triangles) says Done;
//  6. the edited copy is bigger than the baseline by exactly 36 + 12 (+ 4 with merge data) bytes for each new polygon, and the game's loader takes it;
//  7. the shape has the old polygons unchanged, then the new ones as in 4, and its points, point normals, extents, radius and number are as they were; with merge data the
//     numbers are the old ones then the new (-1, or the pair); the header's polygon count is the new count;
//  8. every other shape of the level is, number for number, the baseline's; the object count is the baseline's;
//  9. refusals on the level (an ID no object has, a corner the shape lacks, two corners) leave the level exactly as the baseline (saved again, byte for byte);
//  10. the opened file is byte for byte what it was at the end.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "shape_edit.h"
#include "poly_edit.h"
#include "anim_edit.h"
#include "uv_model.h"
#include "shape_maths.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls: files are read by Windows' own

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

static bool ReadWholeFile(std::string const &path, std::vector<char> *out)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	LARGE_INTEGER size;
	bool good = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1LL << 31);

	if (good)
	{
		out->assign((size_t)size.QuadPart, 0);
		DWORD got = 0;
		good = ReadFile(file, out->data(), (DWORD)out->size(), &got, 0) && got == out->size();
	}

	CloseHandle(file);
	return good;
}

static std::string CopyPathIn(std::string const &folder, char const *name)
{
	return folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + name;
}

static std::string BaseName(std::string const &path)
{
	size_t const at = path.find_last_of("\\/");
	std::string name = at == std::string::npos ? path : path.substr(at + 1);
	size_t const dot = name.find_last_of('.');

	return dot == std::string::npos ? name : name.substr(0, dot);
}

static Object_Chunk *FindObject(OpenLevelFile const &open, uint8_t const *id)
{
	List<Object_Chunk *> all;

	open.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
		{
			return each();
		}
	}

	return nullptr;
}

static size_t ObjectCount(OpenLevelFile const &open)
{
	List<Object_Chunk *> all;

	open.Level()->list_objects(&all);

	size_t n = 0;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		n++;
	}

	return n;
}

static bool SameShape(model::ShapeModel const &a, model::ShapeModel const &b)
{
	if (a.flags != b.flags || a.number != b.number || a.radius != b.radius || memcmp(&a.extents, &b.extents, sizeof(a.extents)) != 0 ||
		a.points.size() != b.points.size() || a.polygons.size() != b.polygons.size() || a.polygonNormals.size() != b.polygonNormals.size() || a.pointNormals.size() != b.pointNormals.size())
	{
		return false;
	}

	return (a.points.empty() || memcmp(a.points.data(), b.points.data(), a.points.size() * sizeof(a.points[0])) == 0) &&
		(a.polygons.empty() || memcmp(a.polygons.data(), b.polygons.data(), a.polygons.size() * sizeof(a.polygons[0])) == 0) &&
		(a.polygonNormals.empty() || memcmp(a.polygonNormals.data(), b.polygonNormals.data(), a.polygonNormals.size() * sizeof(a.polygonNormals[0])) == 0) &&
		(a.pointNormals.empty() || memcmp(a.pointNormals.data(), b.pointNormals.data(), a.pointNormals.size() * sizeof(a.pointNormals[0])) == 0);
}

// ================================================================= A. the numbers

static model::ShapeModel SquareShape()
{
	model::ShapeModel shape;

	memset(&shape.extents, 0, sizeof(shape.extents));
	shape.flags = 0;
	shape.number = 1;
	shape.radius = 0;

	int32_t const xyz[][3] = { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 }, { 0, 0, 500 }, { 2000, 0, 0 }, { 600, 300, 0 } };

	for (auto const &p : xyz)
	{
		model::ShapePoint point = { p[0], p[1], p[2] };
		shape.points.push_back(point);
	}

	return shape;
}

static bool OnADiagonal(int32_t a, int32_t b)
{
	return (a == 0 && b == 2) || (a == 2 && b == 0) || (a == 1 && b == 3) || (a == 3 && b == 1);
}

static void Numbers()
{
	printf("THE NUMBERS\n");

	// 1. the nearest corner
	{
		host::PolyCornerSpot const spots[] = { { 10, true, 100, 100 }, { 11, true, 110, 100 }, { 12, false, 101, 100 }, { 13, true, 100, 120 } };
		int32_t point = -1;

		Check(host::PolyEdit_NearestCorner(spots, 4, 101, 100, &point) && point == 10, "the nearest corner is the one at the smallest distance (a corner that is not on the screen is skipped)");
		Check(host::PolyEdit_NearestCorner(spots, 4, 105, 100, &point) && point == 10, "two at the same distance: the first");

		host::PolyCornerSpot const farOne[] = { { 5, true, 1e6, 1e6 } };

		Check(!host::PolyEdit_NearestCorner(farOne, 1, 0, 0, &point), "a corner farther than 1e9 squared is not taken");
		Check(!host::PolyEdit_NearestCorner(nullptr, 0, 0, 0, &point), "no corners: none");
	}

	model::ShapeModel shape = SquareShape();

	// 2. the quad order
	{
		int32_t const scrambled[4] = { 0, 2, 1, 3 };
		int32_t order[4] = { -1, -1, -1, -1 };

		Check(host::PolyEdit_QuadOrder(shape.points, scrambled, order), "a scrambled flat square is accepted");

		bool goesRound = true, allThere = true;

		for (int i = 0; i < 4; i++)
		{
			goesRound = goesRound && !OnADiagonal(order[i], order[(i + 1) & 3]);
		}

		for (int32_t want = 0; want < 4; want++)
		{
			bool found = false;

			for (int i = 0; i < 4; i++)
			{
				found = found || order[i] == want;
			}

			allThere = allThere && found;
		}

		Check(goesRound && allThere, "the order goes round the square (no two neighbours on a diagonal) and holds the same four corners");

		int32_t const already[4] = { 0, 1, 2, 3 };

		Check(host::PolyEdit_QuadOrder(shape.points, already, order) && order[0] == 0 && order[1] == 1 && order[2] == 2 && order[3] == 3, "an order that already goes round is kept");

		int32_t const offPlane[4] = { 0, 1, 2, 4 };
		int32_t const inLine[4] = { 0, 1, 5, 2 };
		int32_t const inside[4] = { 0, 1, 2, 6 };

		Check(!host::PolyEdit_QuadOrder(shape.points, offPlane, order), "a corner off the plane is refused");
		Check(!host::PolyEdit_QuadOrder(shape.points, inLine, order), "three corners in a line are refused");
		Check(!host::PolyEdit_QuadOrder(shape.points, inside, order), "a corner inside the triangle of the other three is refused (no convex quad)");

		int32_t const twice[4] = { 0, 1, 2, 2 };
		int32_t const missing[4] = { 0, 1, 2, 99 };

		Check(!host::PolyEdit_QuadOrder(shape.points, twice, order), "a corner twice is refused");
		Check(!host::PolyEdit_QuadOrder(shape.points, missing, order), "a corner the shape lacks is refused");
	}

	// 3. the list of held corners
	{
		host::PolyVertexList list;
		uint8_t const idA[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
		uint8_t const idB[8] = { 9, 2, 3, 4, 5, 6, 7, 8 };

		host::PolyEdit_VertexClear(&list);

		Check(host::PolyEdit_VertexClick(&list, idA, 0, false, shape.points) == host::PolyVertexClick::Added && list.count == 1, "a plain click takes a corner");
		Check(host::PolyEdit_VertexClick(&list, idA, 0, false, shape.points) == host::PolyVertexClick::AlreadyHeld && list.count == 1, "the same corner again does nothing");
		Check(host::PolyEdit_VertexClick(&list, idB, 1, false, shape.points) == host::PolyVertexClick::OtherShape && list.count == 1, "a corner of another shape does nothing");
		Check(host::PolyEdit_VertexClick(&list, idA, 2, false, shape.points) == host::PolyVertexClick::Added && list.count == 2, "a second corner is taken");
		Check(host::PolyEdit_VertexClick(&list, idA, 3, true, shape.points) == host::PolyVertexClick::NotHeld && list.count == 2, "Shift on a corner not held does nothing");
		Check(host::PolyEdit_VertexClick(&list, idA, 0, true, shape.points) == host::PolyVertexClick::Removed && list.count == 1 && list.point[0] == 2, "Shift takes a held corner out and closes the gap");
		Check(host::PolyEdit_VertexClick(&list, idA, 99, false, shape.points) == host::PolyVertexClick::BadCall && list.count == 1, "a point the shape lacks is refused");

		host::PolyEdit_VertexClear(&list);
		host::PolyEdit_VertexClick(&list, idA, 0, false, shape.points);
		host::PolyEdit_VertexClick(&list, idA, 1, false, shape.points);
		host::PolyEdit_VertexClick(&list, idA, 2, false, shape.points);
		Check(host::PolyEdit_VertexClick(&list, idA, 4, false, shape.points) == host::PolyVertexClick::NotAQuad && list.count == 3, "a fourth corner that makes no flat quad is taken back");
		Check(host::PolyEdit_VertexClick(&list, idA, 3, false, shape.points) == host::PolyVertexClick::AddedAndOrdered && list.count == 4, "a fourth that does is taken");
		Check(host::PolyEdit_VertexClick(&list, idA, 5, false, shape.points) == host::PolyVertexClick::ListFull && list.count == 4, "a fifth click does nothing");
	}

	// 4. Create poly on the numbers
	{
		double const along[3] = { 0, 0, 1 };      // the camera looks along +z: a normal that points to +z is facing away from it
		double const against[3] = { 0, 0, -1 };
		int32_t const three[3] = { 0, 1, 2 };     // (0,0,0) (1000,0,0) (1000,1000,0): the normal is +z
		size_t first = 99;

		model::ShapeModel one = SquareShape();
		one.polygons.clear();

		Check(host::PolyEdit_CreateInModel(&one, nullptr, three, 3, against, &first) == host::PolyCreateResult::Done && first == 0 && one.polygons.size() == 1 && one.polygonNormals.size() == 1,
			"three corners make one polygon");

		model::ShapePolygon const &p = one.polygons[0];

		Check(p.engineType == 2 && p.colour == 0x7fff && p.flags == 0 && p.normalIndex == 0 && p.cornerCount == 3 && p.corners[0] == 0 && p.corners[1] == 1 && p.corners[2] == 2 && p.corners[3] == -1 && p.corners[4] == -1,
			"it is a plain-colour triangle with its own number as its normal number");
		Check(fabs(one.polygonNormals[0].x) < 1e-6 && fabs(one.polygonNormals[0].y) < 1e-6 && fabs(one.polygonNormals[0].z - 1.0) < 1e-6, "its normal is the unit vector square to both edges: +z");

		model::ShapeModel turned = SquareShape();
		turned.polygons.clear();

		Check(host::PolyEdit_CreateInModel(&turned, nullptr, three, 3, along, nullptr) == host::PolyCreateResult::Done, "a camera looking along the normal still makes the polygon");
		Check(turned.polygons[0].corners[1] == 2 && turned.polygons[0].corners[2] == 1 && fabs(turned.polygonNormals[0].z + 1.0) < 1e-6, "and turns it round: corners 2 and 3 swapped, normal negated");

		model::ShapeModel flat = SquareShape();
		flat.polygons.clear();

		Check(host::PolyEdit_CreateInModel(&flat, nullptr, three, 3, nullptr, nullptr) == host::PolyCreateResult::Done && flat.polygons[0].corners[1] == 1 && fabs(flat.polygonNormals[0].z - 1.0) < 1e-6, "no view direction: no turning");

		model::ShapeModel quad = SquareShape();
		std::vector<int32_t> merge;
		int32_t const four[4] = { 0, 1, 2, 3 };

		quad.polygons.clear();
		Check(host::PolyEdit_CreateInModel(&quad, &merge, four, 4, against, &first) == host::PolyCreateResult::Done && first == 0 && quad.polygons.size() == 2 && merge.size() == 2, "four corners make two polygons");
		Check(quad.polygons[0].corners[0] == 0 && quad.polygons[0].corners[1] == 1 && quad.polygons[0].corners[2] == 3 && quad.polygons[1].corners[0] == 1 && quad.polygons[1].corners[1] == 2 && quad.polygons[1].corners[2] == 3,
			"they are (0,1,3) and (1,2,3)");
		Check(quad.polygons[0].normalIndex == 0 && quad.polygons[1].normalIndex == 1 && quad.polygonNormals.size() == 2, "each has its own normal number");
		Check(merge[0] == 1 && merge[1] == 0, "with merge data they name each other");

		// refusals change nothing
		model::ShapeModel base = SquareShape();
		model::ShapeModel work = SquareShape();
		int32_t const line[3] = { 0, 1, 5 };
		int32_t const twice[3] = { 0, 1, 1 };
		int32_t const missing[3] = { 0, 1, 99 };
		int32_t const two[2] = { 0, 1 };

		Check(host::PolyEdit_CreateInModel(&work, nullptr, line, 3, nullptr, nullptr) == host::PolyCreateResult::Degenerate && SameShape(base, work), "three corners in a line are refused and change nothing");
		Check(host::PolyEdit_CreateInModel(&work, nullptr, twice, 3, nullptr, nullptr) == host::PolyCreateResult::BadCall && SameShape(base, work), "a corner twice is refused");
		Check(host::PolyEdit_CreateInModel(&work, nullptr, missing, 3, nullptr, nullptr) == host::PolyCreateResult::BadCall && SameShape(base, work), "a corner the shape lacks is refused");
		Check(host::PolyEdit_CreateInModel(&work, nullptr, two, 2, nullptr, nullptr) == host::PolyCreateResult::BadCall && SameShape(base, work), "two corners are refused");
	}
}

// ================================================================= B. on a stock level

struct Candidate
{
	uint8_t id[8];
	bool have;
	model::ShapeModel shape;
};

static bool HasChunk(Object_Chunk *object, char const *name)
{
	Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

	return shape && shape->lookup_single_child(name) != nullptr;
}

static size_t MergeCount(OpenLevelFile const &open, Object_Chunk *object)
{
	Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;
	Chunk *const chunk = shape ? shape->lookup_single_child("SHPMRGDT") : nullptr;
	std::vector<unsigned char> bytes;

	return chunk && open.PayloadOf(chunk, &bytes) ? bytes.size() / 4 : (size_t)-1;
}

static bool MergeNumbers(OpenLevelFile const &open, Object_Chunk *object, std::vector<int32_t> *numbers)
{
	Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;
	Chunk *const chunk = shape ? shape->lookup_single_child("SHPMRGDT") : nullptr;
	std::vector<unsigned char> bytes;

	if (!chunk || !open.PayloadOf(chunk, &bytes) || bytes.size() % 4 != 0)
	{
		return false;
	}

	numbers->resize(bytes.size() / 4);

	if (!bytes.empty())
	{
		memcpy(numbers->data(), bytes.data(), bytes.size());
	}

	return true;
}

// The corners of the first polygon of the shape with exactly `want` corners, or false
static bool FirstWithCorners(model::ShapeModel const &shape, int want, int32_t *corners)
{
	for (model::ShapePolygon const &p : shape.polygons)
	{
		if (p.cornerCount == want)
		{
			for (int i = 0; i < want; i++)
			{
				corners[i] = p.corners[i];
			}

			return true;
		}
	}

	return false;
}

// Whether three corners of the shape are not in a line
static bool NotInALine(model::ShapeModel const &shape, int32_t a, int32_t b, int32_t c)
{
	double const ab[3] = { (double)shape.points[(size_t)b].x - shape.points[(size_t)a].x, (double)shape.points[(size_t)b].y - shape.points[(size_t)a].y, (double)shape.points[(size_t)b].z - shape.points[(size_t)a].z };
	double const ac[3] = { (double)shape.points[(size_t)c].x - shape.points[(size_t)a].x, (double)shape.points[(size_t)c].y - shape.points[(size_t)a].y, (double)shape.points[(size_t)c].z - shape.points[(size_t)a].z };
	double const cx = ab[1] * ac[2] - ab[2] * ac[1], cy = ab[2] * ac[0] - ab[0] * ac[2], cz = ab[0] * ac[1] - ab[1] * ac[0];

	return sqrt(cx * cx + cy * cy + cz * cz) > 1.0;
}

static void CheckAgainstBaseline(OpenLevelFile &edited, OpenLevelFile &baseline, uint8_t const *id, model::ShapeModel const &original, std::vector<int32_t> const &originalMerge, bool hadMerge,
	size_t added, size_t first, std::string const &label)
{
	model::ShapeModel now;
	std::string why;

	Check(host::ShapeEdit_Read(edited.Level(), &edited, id, &now, nullptr, &why) == host::ShapeEditResult::Done, label + ": the edited shape is read");
	Check(now.polygons.size() == original.polygons.size() + added && now.polygonNormals.size() == original.polygonNormals.size() + added, label + ": the shape has " + std::to_string(added) + " more polygons and normals");
	Check(first == original.polygons.size(), label + ": the new polygons start after the old ones");

	bool oldSame = now.polygons.size() >= original.polygons.size();

	for (size_t i = 0; oldSame && i < original.polygons.size(); i++)
	{
		oldSame = memcmp(&now.polygons[i], &original.polygons[i], sizeof(original.polygons[i])) == 0 && memcmp(&now.polygonNormals[i], &original.polygonNormals[i], sizeof(original.polygonNormals[i])) == 0;
	}

	Check(oldSame, label + ": the old polygons and their normals are as they were");
	Check(now.points.size() == original.points.size() && memcmp(now.points.data(), original.points.data(), original.points.size() * sizeof(original.points[0])) == 0 &&
		now.pointNormals.size() == original.pointNormals.size() && (original.pointNormals.empty() || memcmp(now.pointNormals.data(), original.pointNormals.data(), original.pointNormals.size() * sizeof(original.pointNormals[0])) == 0) &&
		memcmp(&now.extents, &original.extents, sizeof(now.extents)) == 0 && now.radius == original.radius && now.number == original.number && now.flags == original.flags,
		label + ": points, point normals, extents, radius, number and flags are as they were");

	bool newOk = now.polygons.size() == original.polygons.size() + added;

	for (size_t k = 0; newOk && k < added; k++)
	{
		model::ShapePolygon const &p = now.polygons[original.polygons.size() + k];
		model::ShapeNormal const &n = now.polygonNormals[original.polygons.size() + k];
		double const length = sqrt((double)n.x * n.x + (double)n.y * n.y + (double)n.z * n.z);

		newOk = p.engineType == 2 && p.colour == 0x7fff && p.flags == 0 && p.cornerCount == 3 && p.normalIndex == (int32_t)(original.polygons.size() + k) && fabs(length - 1.0) < 1e-5;

		// the normal is square to both edges of the polygon
		if (newOk)
		{
			model::ShapePoint const &a = now.points[(size_t)p.corners[0]], &b = now.points[(size_t)p.corners[1]], &c = now.points[(size_t)p.corners[2]];
			double const ab[3] = { (double)b.x - a.x, (double)b.y - a.y, (double)b.z - a.z };
			double const ac[3] = { (double)c.x - a.x, (double)c.y - a.y, (double)c.z - a.z };
			double const d1 = (ab[0] * n.x + ab[1] * n.y + ab[2] * n.z) / sqrt(ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2]);
			double const d2 = (ac[0] * n.x + ac[1] * n.y + ac[2] * n.z) / sqrt(ac[0] * ac[0] + ac[1] * ac[1] + ac[2] * ac[2]);

			newOk = fabs(d1) < 1e-4 && fabs(d2) < 1e-4;
		}
	}

	Check(newOk, label + ": each new polygon is a plain-colour triangle whose unit normal is square to its edges");

	Object_Chunk *const object = FindObject(edited, id);

	if (hadMerge)
	{
		std::vector<int32_t> merge;
		bool mergeOk = MergeNumbers(edited, object, &merge) && merge.size() == original.polygons.size() + added;

		for (size_t i = 0; mergeOk && i < originalMerge.size(); i++)
		{
			mergeOk = merge[i] == originalMerge[i];
		}

		for (size_t k = 0; mergeOk && k < added; k++)
		{
			size_t const n = original.polygons.size() + k;

			mergeOk = merge[n] == (added == 2 ? (int32_t)(original.polygons.size() + (1 - k)) : -1);
		}

		Check(mergeOk, label + ": the merge data is the old numbers, then -1 (or the pair naming each other)");
	}
	else
	{
		Check(!HasChunk(object, "SHPMRGDT"), label + ": a shape with no merge data still has none");
	}

	// every other shape is the baseline's
	List<Object_Chunk *> all;

	baseline.Level()->list_objects(&all);

	size_t others = 0, differ = 0;

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const o = each();

		if (!o || !o->get_assoc_shape() || memcmp(&o->object_data.ID, id, 8) == 0)
		{
			continue;
		}

		model::ShapeModel a, b;
		std::string ignore;
		uint8_t const *const otherId = (uint8_t const *)&o->object_data.ID;
		bool const readA = host::ShapeEdit_Read(baseline.Level(), &baseline, otherId, &a, nullptr, &ignore) == host::ShapeEditResult::Done;
		bool const readB = host::ShapeEdit_Read(edited.Level(), &edited, otherId, &b, nullptr, &ignore) == host::ShapeEditResult::Done;

		if (readA && a.number == original.number)
		{
			continue;   // another object that uses the SAME shape: it has the new polygons too
		}

		if (readA != readB || (readA && !SameShape(a, b)))
		{
			differ++;
		}

		others++;
	}

	Check(others > 0 && differ == 0, label + ": the other " + std::to_string(others) + " shapes are the baseline's, number for number");
	Check(ObjectCount(edited) == ObjectCount(baseline), label + ": the level has the baseline's number of objects");
}

static void OneLevel(std::string const &levelPath, std::string const &copyFolder, std::vector<char> const &original)
{
	std::string const level = BaseName(levelPath);

	printf("LEVEL %s\n", level.c_str());

	OpenLevelFile baseline;

	if (baseline.Open(levelPath.c_str()) != LevelOpenResult::Opened || !baseline.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	CreateDirectoryA(copyFolder.c_str(), NULL);

	std::string const baselinePath = CopyPathIn(copyFolder, (level + "-poly-baseline.rif").c_str());

	DeleteFileA(baselinePath.c_str());
	Check(baseline.SaveAs(baselinePath.c_str()) == LevelSaveResult::Saved, "the level is saved as a baseline COPY with no edit");

	std::vector<char> baselineBytes;

	if (!ReadWholeFile(baselinePath, &baselineBytes))
	{
		Check(false, "the baseline copy can be read");
		return;
	}

	// choose the objects: one with a real quad (4 corners), one with paired triangles (merge data), one with a plain triangle, none with a SHPMORPH
	Candidate quad = { { 0 }, false }, triangle = { { 0 }, false }, paired = { { 0 }, false };
	int32_t quadCorners[4] = { 0 };
	int32_t tri[3] = { 0 };
	int32_t pairCorners[4] = { 0 };
	std::vector<int32_t> pairMerge;
	List<Object_Chunk *> all;

	baseline.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !object->get_assoc_shape() || HasChunk(object, "SHPMORPH"))
		{
			continue;
		}

		model::ShapeModel shape;
		std::string why;
		uint8_t const *const id = (uint8_t const *)&object->object_data.ID;

		if (host::ShapeEdit_Read(baseline.Level(), &baseline, id, &shape, nullptr, &why) != host::ShapeEditResult::Done || shape.polygons.empty() || shape.polygons.size() > 20000)
		{
			continue;
		}

		bool const merged = HasChunk(object, "SHPMRGDT");

		if (!quad.have && !merged && FirstWithCorners(shape, 4, quadCorners) && NotInALine(shape, quadCorners[0], quadCorners[1], quadCorners[2]))
		{
			memcpy(quad.id, id, 8);
			quad.shape = shape;
			quad.have = true;
		}

		if (!triangle.have && FirstWithCorners(shape, 3, tri) && NotInALine(shape, tri[0], tri[1], tri[2]))
		{
			memcpy(triangle.id, id, 8);
			triangle.shape = shape;
			triangle.have = true;
		}

		if (!paired.have && merged)
		{
			std::vector<int32_t> merge;

			if (MergeNumbers(baseline, object, &merge) && merge.size() == shape.polygons.size())
			{
				for (size_t i = 0; i < merge.size(); i++)
				{
					int32_t const j = merge[i];

					if (j > (int32_t)i && (size_t)j < shape.polygons.size() && shape.polygons[i].cornerCount == 3 && shape.polygons[(size_t)j].cornerCount == 3)
					{
						// the four corners of the two paired triangles: the first one's three and the second's corner that is not among them
						int32_t four[4] = { shape.polygons[i].corners[0], shape.polygons[i].corners[1], shape.polygons[i].corners[2], -1 };

						for (int c = 0; c < 3 && four[3] < 0; c++)
						{
							int32_t const corner = shape.polygons[(size_t)j].corners[c];

							if (corner != four[0] && corner != four[1] && corner != four[2])
							{
								four[3] = corner;
							}
						}

						int32_t order[4];

						if (four[3] >= 0 && host::PolyEdit_QuadOrder(shape.points, four, order))
						{
							memcpy(pairCorners, order, sizeof(order));
							memcpy(paired.id, id, 8);
							paired.shape = shape;
							paired.have = true;
							pairMerge = merge;
							break;
						}
					}
				}
			}
		}
	}

	if (!triangle.have && !quad.have && !paired.have)
	{
		Check(false, "the level has a shape to edit");
		return;
	}

	// run one case: the edit on a fresh open, a saved copy, both copies loaded again
	auto Case = [&](char const *name, Candidate const &c, int32_t const *corners, size_t count, bool scramble)
	{
		std::string const label = level + " " + name;

		if (!c.have)
		{
			printf("  --  %s: this level has no such shape\n", label.c_str());
			return;
		}

		OpenLevelFile work;

		if (work.Open(levelPath.c_str()) != LevelOpenResult::Opened)
		{
			Check(false, label + ": the level is opened again");
			return;
		}

		Object_Chunk *const object = FindObject(work, c.id);
		bool const hadMerge = HasChunk(object, "SHPMRGDT");
		std::vector<int32_t> mergeBefore;

		if (hadMerge)
		{
			MergeNumbers(work, object, &mergeBefore);
		}

		int32_t use[4] = { corners[0], corners[1], corners[2], count == 4 ? corners[3] : 0 };

		if (scramble && count == 4)
		{
			int32_t const shuffled[4] = { corners[2], corners[0], corners[3], corners[1] };
			int32_t order[4];

			Check(host::PolyEdit_QuadOrder(c.shape.points, shuffled, order), label + ": the scrambled corners of a real quad are accepted");
			memcpy(use, order, sizeof(order));
		}

		size_t first = 0, added = 0;
		std::string why;
		double const view[3] = { 0.3, -0.5, 0.8 };
		host::PolyCreateResult const result = host::PolyEdit_Create(&work, c.id, use, count, view, &first, &added, &why);

		Check(result == host::PolyCreateResult::Done, label + ": Create poly is done" + (result == host::PolyCreateResult::Done ? "" : std::string(": ") + host::PolyCreateWords(result) + " " + why));

		if (result != host::PolyCreateResult::Done)
		{
			return;
		}

		Check(added == count - 2, label + ": " + std::to_string(count - 2) + " polygon(s) added");

		std::string const copyPath = CopyPathIn(copyFolder, (level + "-poly-" + name + ".rif").c_str());

		DeleteFileA(copyPath.c_str());
		Check(work.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, label + ": the edited level is saved as a COPY");

		std::vector<char> copyBytes;

		// the saved file is packed (the game's own compressing), so its size is not exact: the unpacked image of the level, as the next save would write it, is
		std::vector<char> imageBefore, imageAfter;
		size_t const wantedMore = added * (model::kShapePolygonBytes + model::kShapeNormalBytes + (hadMerge ? 4 : 0));

		Check(baseline.BuildLevel(imageBefore) && work.BuildLevel(imageAfter) && imageAfter.size() == imageBefore.size() + wantedMore,
			label + ": the level image is bigger than the baseline's by exactly the new polygons' bytes (" + std::to_string(wantedMore) + ")");
		Check(ReadWholeFile(copyPath, &copyBytes) && copyBytes.size() > baselineBytes.size(), label + ": the saved copy is bigger than the baseline copy");

		OpenLevelFile edited;

		Check(edited.Open(copyPath.c_str()) == LevelOpenResult::Opened && edited.Level(), label + ": the game's loader takes the copy");

		if (edited.Level())
		{
			CheckAgainstBaseline(edited, baseline, c.id, c.shape, mergeBefore, hadMerge, added, first, label);
		}
	};

	Case("triangle", triangle, tri, 3, false);
	Case("quad", quad, quadCorners, 4, true);
	Case("paired-quad", paired, pairCorners, 4, false);

	// refusals on the level change nothing
	{
		OpenLevelFile work;

		if (work.Open(levelPath.c_str()) == LevelOpenResult::Opened)
		{
			Candidate const &use = triangle.have ? triangle : (quad.have ? quad : paired);
			uint8_t const nobody[8] = { 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef };
			int32_t const three[3] = { 0, 1, 2 };
			int32_t const lacks[3] = { 0, 1, 0x7fffffff };
			int32_t const twice[3] = { 0, 1, 1 };
			std::string why;

			Check(host::PolyEdit_Create(&work, nobody, three, 3, nullptr, nullptr, nullptr, &why) == host::PolyCreateResult::NoObject, level + ": an ID no object has is refused");
			Check(host::PolyEdit_Create(&work, use.id, lacks, 3, nullptr, nullptr, nullptr, &why) == host::PolyCreateResult::BadCall, level + ": a corner the shape lacks is refused");
			Check(host::PolyEdit_Create(&work, use.id, twice, 3, nullptr, nullptr, nullptr, &why) == host::PolyCreateResult::BadCall, level + ": a corner twice is refused");
			Check(host::PolyEdit_Create(&work, use.id, three, 2, nullptr, nullptr, nullptr, &why) == host::PolyCreateResult::BadCall, level + ": two corners are refused");

			std::string const again = CopyPathIn(copyFolder, (level + "-poly-refused.rif").c_str());
			std::vector<char> againBytes;

			DeleteFileA(again.c_str());
			Check(work.SaveAs(again.c_str()) == LevelSaveResult::Saved && ReadWholeFile(again, &againBytes) && againBytes == baselineBytes, level + ": after the refusals the level saves byte for byte as the baseline");
		}
	}

	std::vector<char> originalEnd;

	Check(ReadWholeFile(levelPath, &originalEnd) && originalEnd == original, level + ": the opened file is still byte for byte what it was");
}

// ================================================================= C. polygon type changes (MakePlain, MakeTextured) on a stock level

static bool ReadLists(OpenLevelFile const &open, Object_Chunk *object, std::vector<model::UvList> *lists)
{
	Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;
	Chunk *const chunk = shape ? shape->lookup_single_child("SHPUVCRD") : nullptr;
	std::vector<unsigned char> bytes;

	return chunk && open.PayloadOf(chunk, &bytes) && model::ReadUvLists(bytes.data(), bytes.size(), lists) == model::UvModelResult::Ok;
}

static bool SameList(model::UvList const &a, model::UvList const &b)
{
	if (a.cornerCount != b.cornerCount)
	{
		return false;
	}

	for (int i = 0; i < a.cornerCount; i++)
	{
		if (a.corners[i].u != b.corners[i].u || a.corners[i].v != b.corners[i].v)
		{
			return false;
		}
	}

	return true;
}

struct TypeCandidate
{
	uint8_t id[8];
	bool have;
	int32_t polygon;
	model::ShapeModel shape;
	std::vector<model::UvList> lists;
};

static void TypeChanges(std::string const &levelPath, std::string const &copyFolder)
{
	std::string const level = BaseName(levelPath);

	printf("TYPE CHANGES %s\n", level.c_str());

	OpenLevelFile baseline;

	if (baseline.Open(levelPath.c_str()) != LevelOpenResult::Opened || !baseline.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	TypeCandidate textured = { { 0 }, false, -1 }, plain = { { 0 }, false, -1 };
	List<Object_Chunk *> all;

	baseline.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if ((textured.have && plain.have) || !object || !object->get_assoc_shape())
		{
			continue;
		}

		model::ShapeModel shape;
		std::vector<model::UvList> lists;
		std::string why;
		uint8_t const *const id = (uint8_t const *)&object->object_data.ID;

		if (host::ShapeEdit_Read(baseline.Level(), &baseline, id, &shape, nullptr, &why) != host::ShapeEditResult::Done || shape.polygons.empty() || !ReadLists(baseline, object, &lists))
		{
			continue;
		}

		int32_t shapeNumber = 0;
		model::TextureAnim anim;

		if (host::AnimEdit_Read(&baseline, id, &shapeNumber, &anim) != host::AnimEditResult::Done)
		{
			continue;
		}

		for (size_t p = 0; p < shape.polygons.size(); p++)
		{
			model::ShapePolygon const &polygon = shape.polygons[p];
			bool animated = false;

			for (model::AnimPolygon const &a : anim.polygons)
			{
				animated = animated || a.polygon == (int32_t)p;
			}

			if (!textured.have && model::EngineTypeIsTextured(polygon.engineType) && !(polygon.colour & 0x8000) && !animated)
			{
				int32_t const list = model::PolygonUvListNumber(polygon.colour);
				size_t users = 0;

				for (model::ShapePolygon const &other : shape.polygons)
				{
					users += model::EngineTypeIsTextured(other.engineType) && model::PolygonUvListNumber(other.colour) == list ? 1 : 0;
				}

				if (list >= 0 && (size_t)list < lists.size() && users == 1 && lists[(size_t)list].cornerCount > 0)
				{
					memcpy(textured.id, id, 8);
					textured.have = true;
					textured.polygon = (int32_t)p;
					textured.shape = shape;
					textured.lists = lists;
				}
			}

			if (!plain.have && !model::EngineTypeIsTextured(polygon.engineType) && polygon.cornerCount <= 4)
			{
				memcpy(plain.id, id, 8);
				plain.have = true;
				plain.polygon = (int32_t)p;
				plain.shape = shape;
				plain.lists = lists;
			}
		}
	}

	std::vector<char> imageBefore;

	baseline.BuildLevel(imageBefore);

	// ---- MakePlain
	if (!textured.have)
	{
		printf("  --  %s: no textured polygon with a list of its own outside an animation\n", level.c_str());
	}
	else
	{
		OpenLevelFile work;
		std::string const label = level + " MakePlain";
		int32_t const chosen[1] = { textured.polygon };
		size_t changed = 0;
		std::string why;

		Check(work.Open(levelPath.c_str()) == LevelOpenResult::Opened, label + ": the level is opened again");

		// refusals first: each leaves the level as it was
		int32_t const outside[1] = { 0x7fffff };
		int32_t const twice[2] = { textured.polygon, textured.polygon };
		std::vector<char> now;

		Check(host::PolyEdit_MakePlain(&work, textured.id, outside, 1, 0x1234, nullptr, &why) == host::PolyTypeResult::NoPolygon, label + ": a polygon the shape lacks is refused");
		Check(host::PolyEdit_MakePlain(&work, textured.id, twice, 2, 0x1234, nullptr, &why) == host::PolyTypeResult::BadCall, label + ": a polygon named twice is refused");
		Check(host::PolyEdit_MakePlain(&work, textured.id, chosen, 1, -5, nullptr, &why) == host::PolyTypeResult::BadCall, label + ": a negative colour is refused");
		Check(work.BuildLevel(now) && now == imageBefore, label + ": after the refusals the level is as it was");

		host::PolyTypeResult const result = host::PolyEdit_MakePlain(&work, textured.id, chosen, 1, 0x1234, &changed, &why);

		Check(result == host::PolyTypeResult::Done && changed == 1, label + ": a textured polygon is made plain" + (result == host::PolyTypeResult::Done ? "" : std::string(": ") + host::PolyTypeWords(result) + " " + why));

		if (result == host::PolyTypeResult::Done)
		{
			std::string const copyPath = CopyPathIn(copyFolder, (level + "-poly-plain.rif").c_str());

			DeleteFileA(copyPath.c_str());
			Check(work.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, label + ": the level is saved as a COPY");

			OpenLevelFile edited;

			Check(edited.Open(copyPath.c_str()) == LevelOpenResult::Opened && edited.Level(), label + ": the game's loader takes the copy");

			if (edited.Level())
			{
				model::ShapeModel now2;
				std::vector<model::UvList> lists;
				std::string ignore;
				Object_Chunk *const object = FindObject(edited, textured.id);
				size_t const freedBytes = 4 + 8 * (size_t)textured.lists[(size_t)model::PolygonUvListNumber(textured.shape.polygons[(size_t)textured.polygon].colour)].cornerCount;
				std::vector<char> imageAfter;

				Check(host::ShapeEdit_Read(edited.Level(), &edited, textured.id, &now2, nullptr, &ignore) == host::ShapeEditResult::Done && ReadLists(edited, object, &lists), label + ": the edited shape and its lists are read");
				Check(edited.BuildLevel(imageAfter) && imageAfter.size() + freedBytes == imageBefore.size(), label + ": the level image is smaller by exactly the freed list (" + std::to_string(freedBytes) + " bytes)");
				Check(now2.polygons.size() == textured.shape.polygons.size() && lists.size() + 1 == textured.lists.size(), label + ": one polygon list less, the same polygons");

				model::ShapePolygon const &p = now2.polygons[(size_t)textured.polygon];
				model::ShapePolygon const &was = textured.shape.polygons[(size_t)textured.polygon];

				Check(p.engineType == 2 && p.colour == 0x1234 && p.flags == was.flags && p.cornerCount == was.cornerCount && memcmp(p.corners, was.corners, sizeof(p.corners)) == 0 && p.normalIndex == was.normalIndex,
					label + ": it is type 2 with the colour, and its corners, flags and normal number are as they were");

				// every other polygon is as it was: the same type and picture, and its list (found by the renumbered number) holds the same coordinates
				bool others = true;

				for (size_t i = 0; i < now2.polygons.size() && others; i++)
				{
					if ((int32_t)i == textured.polygon)
					{
						continue;
					}

					model::ShapePolygon const &a = textured.shape.polygons[i];
					model::ShapePolygon const &b = now2.polygons[i];

					others = a.engineType == b.engineType && a.flags == b.flags && a.cornerCount == b.cornerCount && memcmp(a.corners, b.corners, sizeof(a.corners)) == 0;

					if (others && model::EngineTypeIsTextured(a.engineType))
					{
						int32_t const la = model::PolygonUvListNumber(a.colour), lb = model::PolygonUvListNumber(b.colour);

						others = model::PolygonPictureNumber(a.colour) == model::PolygonPictureNumber(b.colour) && la >= 0 && lb >= 0 && (size_t)la < textured.lists.size() && (size_t)lb < lists.size() && SameList(textured.lists[(size_t)la], lists[(size_t)lb]);
					}
					else if (others)
					{
						others = a.colour == b.colour;
					}
				}

				Check(others, label + ": every other polygon keeps its type, picture and coordinates (through the renumbered lists)");
			}
		}
	}

	// ---- MakeTextured
	if (!plain.have)
	{
		printf("  --  %s: no plain polygon in a shape with pictures\n", level.c_str());
	}
	else
	{
		OpenLevelFile work;
		std::string const label = level + " MakeTextured";
		int32_t const chosen[1] = { plain.polygon };
		size_t changed = 0;
		std::string why;

		Check(work.Open(levelPath.c_str()) == LevelOpenResult::Opened, label + ": the level is opened again");

		host::PolyTypeResult const result = host::PolyEdit_MakeTextured(&work, plain.id, chosen, 1, -1, &changed, &why);

		Check(result == host::PolyTypeResult::Done && changed == 1, label + ": a plain polygon is made textured" + (result == host::PolyTypeResult::Done ? "" : std::string(": ") + host::PolyTypeWords(result) + " " + why));

		if (result == host::PolyTypeResult::Done)
		{
			std::string const copyPath = CopyPathIn(copyFolder, (level + "-poly-textured.rif").c_str());

			DeleteFileA(copyPath.c_str());
			Check(work.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, label + ": the level is saved as a COPY");

			OpenLevelFile edited;

			Check(edited.Open(copyPath.c_str()) == LevelOpenResult::Opened && edited.Level(), label + ": the game's loader takes the copy");

			if (edited.Level())
			{
				model::ShapeModel now2;
				std::vector<model::UvList> lists;
				std::string ignore;
				Object_Chunk *const object = FindObject(edited, plain.id);
				model::ShapePolygon const &was = plain.shape.polygons[(size_t)plain.polygon];
				size_t const addedBytes = 4 + 8 * (size_t)was.cornerCount;
				std::vector<char> imageAfter;

				Check(host::ShapeEdit_Read(edited.Level(), &edited, plain.id, &now2, nullptr, &ignore) == host::ShapeEditResult::Done && ReadLists(edited, object, &lists), label + ": the edited shape and its lists are read");
				Check(edited.BuildLevel(imageAfter) && imageAfter.size() == imageBefore.size() + addedBytes, label + ": the level image is bigger by exactly the new list (" + std::to_string(addedBytes) + " bytes)");
				Check(lists.size() == plain.lists.size() + 1 && now2.polygons.size() == plain.shape.polygons.size(), label + ": one list more, the same polygons");

				model::ShapePolygon const &p = now2.polygons[(size_t)plain.polygon];
				bool listOk = p.engineType == 7 && model::PolygonPictureNumber(p.colour) == 0 && (size_t)model::PolygonUvListNumber(p.colour) == plain.lists.size() && (size_t)model::PolygonUvListNumber(p.colour) < lists.size();

				if (listOk)
				{
					model::UvList const &list = lists[(size_t)model::PolygonUvListNumber(p.colour)];

					listOk = list.cornerCount == was.cornerCount;

					for (int i = 0; listOk && i < list.cornerCount; i++)
					{
						listOk = list.corners[i].u == 0.0f && list.corners[i].v == 0.0f;
					}
				}

				Check(listOk, label + ": it is type 7 showing picture 0 with a new list of the polygon's corner count, every corner at (0, 0)");
				Check(p.flags == was.flags && p.cornerCount == was.cornerCount && memcmp(p.corners, was.corners, sizeof(p.corners)) == 0 && p.normalIndex == was.normalIndex, label + ": its corners, flags and normal number are as they were");

				bool others = true;

				for (size_t i = 0; i < now2.polygons.size() && others; i++)
				{
					others = (int32_t)i == plain.polygon || memcmp(&now2.polygons[i], &plain.shape.polygons[i], sizeof(now2.polygons[i])) == 0;
				}

				Check(others, label + ": every other polygon is byte for byte as it was");

				bool listsKept = true;

				for (size_t i = 0; i < plain.lists.size() && listsKept; i++)
				{
					listsKept = SameList(plain.lists[i], lists[i]);
				}

				Check(listsKept, label + ": the old lists are as they were");
			}

			// a textured polygon made textured again changes nothing
			size_t again = 99;
			int32_t const same[1] = { plain.polygon };
			std::vector<char> beforeAgain, afterAgain;

			Check(work.BuildLevel(beforeAgain) && host::PolyEdit_MakeTextured(&work, plain.id, same, 1, 0, &again, &why) == host::PolyTypeResult::Done && again == 0 && work.BuildLevel(afterAgain) && afterAgain == beforeAgain,
				label + ": a polygon that is textured already is left alone (0 changed, the level as it was)");
		}
	}
}

// ================================================================= D. taking polygons out (PolyEdit_RemoveInModel, PolyEdit_Remove) on a stock level

static void RemoveInNumbers()
{
	printf("REMOVE, THE NUMBERS\n");

	model::ShapeModel shape = SquareShape();
	int32_t const quads[3][4] = { { 0, 1, 2, 3 }, { 0, 1, 5, 2 }, { 4, 1, 2, 6 } };

	for (auto const &c : quads)
	{
		model::ShapePolygon polygon = {};
		model::ShapeNormal normal = { 0, 0, (float)shape.polygons.size() + 1 };

		polygon.engineType = 2;
		polygon.normalIndex = (int32_t)shape.polygons.size();
		polygon.colour = 0x7fff;
		polygon.cornerCount = 4;
		memcpy(polygon.corners, c, sizeof(c));
		polygon.corners[4] = -1;
		shape.polygons.push_back(polygon);
		shape.polygonNormals.push_back(normal);
	}

	for (size_t p = 0; p < shape.points.size(); p++)
	{
		model::ShapeNormal normal = { (float)p, 0, 1 };

		shape.pointNormals.push_back(normal);
	}

	std::vector<int32_t> merge = { 1, 0, -1 };
	model::ShapeModel base = shape;
	std::vector<int32_t> baseMerge = merge;
	int32_t const first[1] = { 0 };
	host::PolyRemoveMap map;

	Check(host::PolyEdit_RemoveInModel(&shape, &merge, first, 1, &map) == host::PolyRemoveResult::Done && shape.polygons.size() == 2, "taking out polygon 0 leaves two");
	Check(map.polygon.size() == 3 && map.polygon[0] == -1 && map.polygon[1] == 0 && map.polygon[2] == 1, "the polygon map: 0 gone, 1 -> 0, 2 -> 1");
	Check(shape.polygons[0].normalIndex == 0 && shape.polygons[1].normalIndex == 1 && shape.polygonNormals.size() == 2 && shape.polygonNormals[0].z == 2.0f && shape.polygonNormals[1].z == 3.0f,
		"each kept polygon keeps its normal and its normal number is its own new number");
	Check(merge.size() == 2 && merge[0] == -1 && merge[1] == -1, "the partner of a polygon taken out becomes -1");

	// polygons 1 and 2 use points 0,1,5,2 and 4,1,2,6: points 3 (only polygon 0 used it) is gone
	Check(map.point.size() == 7 && map.point[3] == -1 && map.point[0] == 0 && map.point[1] == 1 && map.point[2] == 2 && map.point[4] == 3 && map.point[5] == 4 && map.point[6] == 5, "a point only the polygon taken out used is gone, the others keep their order");
	Check(shape.points.size() == 6 && shape.pointNormals.size() == 6 && shape.pointNormals[3].x == 4.0f, "points and point normals follow");
	Check(shape.polygons[0].corners[0] == 0 && shape.polygons[0].corners[2] == 4 && shape.polygons[1].corners[0] == 3, "corners are renumbered through the point map");

	model::ShapeExtents want = {};

	Check(model::ShapeExtentsOf(shape.points, &want) == model::ShapeMathsResult::Ok && memcmp(&want, &shape.extents, sizeof(want)) == 0 && shape.radius == model::ShapeRadiusOf(shape.points), "the extents and the radius are worked out again from the points left");

	// refusals change nothing
	model::ShapeModel work = base;
	std::vector<int32_t> workMerge = baseMerge;
	int32_t const all[3] = { 0, 1, 2 };
	int32_t const twice[2] = { 1, 1 };
	int32_t const outside[1] = { 9 };

	Check(host::PolyEdit_RemoveInModel(&work, &workMerge, all, 3, nullptr) == host::PolyRemoveResult::AllPolygons && SameShape(base, work) && workMerge == baseMerge, "every polygon is refused");
	Check(host::PolyEdit_RemoveInModel(&work, &workMerge, twice, 2, nullptr) == host::PolyRemoveResult::BadCall && SameShape(base, work), "a polygon twice is refused");
	Check(host::PolyEdit_RemoveInModel(&work, &workMerge, outside, 1, nullptr) == host::PolyRemoveResult::NoPolygon && SameShape(base, work), "a polygon the shape lacks is refused");
	Check(host::PolyEdit_RemoveInModel(&work, &workMerge, nullptr, 0, nullptr) == host::PolyRemoveResult::BadCall && SameShape(base, work), "nothing named is refused");
}

static void RemoveOnLevel(std::string const &levelPath, std::string const &copyFolder)
{
	std::string const level = BaseName(levelPath);

	printf("REMOVE %s\n", level.c_str());

	OpenLevelFile baseline;

	if (baseline.Open(levelPath.c_str()) != LevelOpenResult::Opened || !baseline.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	// a shape with enough polygons, no chunk this refuses, preferably with merge data, texture lists and a SHPCENTR; the polygons to take out: one with a partner and one textured
	uint8_t id[8] = { 0 };
	bool have = false;
	int score = -1;
	model::ShapeModel original;
	std::vector<model::UvList> originalLists;
	std::vector<int32_t> originalMerge;
	model::TextureAnim originalAnim;
	std::vector<int32_t> take;
	List<Object_Chunk *> all;

	baseline.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !object->get_assoc_shape() || HasChunk(object, "SHPMORPH") || HasChunk(object, "ANIMSEQU") || HasChunk(object, "SHPPCINF") || HasChunk(object, "SHPFRAGS") || HasChunk(object, "SHPFRGTP"))
		{
			continue;
		}

		model::ShapeModel shape;
		std::vector<model::UvList> lists;
		std::vector<int32_t> merge;
		std::string why;
		uint8_t const *const thisId = (uint8_t const *)&object->object_data.ID;

		if (host::ShapeEdit_Read(baseline.Level(), &baseline, thisId, &shape, nullptr, &why) != host::ShapeEditResult::Done || shape.polygons.size() < 8 || shape.polygons.size() > 6000)
		{
			continue;
		}

		bool const merged = HasChunk(object, "SHPMRGDT") && MergeNumbers(baseline, object, &merge) && merge.size() == shape.polygons.size();
		bool const listed = ReadLists(baseline, object, &lists) && !lists.empty();
		int32_t partnered = -1, texturedOne = -1;

		for (size_t p = 0; p < shape.polygons.size(); p++)
		{
			if (partnered < 0 && merged && merge[p] >= 0)
			{
				partnered = (int32_t)p;
			}

			if (texturedOne < 0 && listed && model::EngineTypeIsTextured(shape.polygons[p].engineType) && !(shape.polygons[p].colour & 0x8000) && model::PolygonUvListNumber(shape.polygons[p].colour) < (int32_t)lists.size())
			{
				texturedOne = (int32_t)p;
			}
		}

		// an animated polygon to take out as well, when the shape has a texture animation
		int32_t animShape = 0;
		model::TextureAnim anim;
		int32_t animated = -1;

		if (host::AnimEdit_Read(&baseline, thisId, &animShape, &anim) == host::AnimEditResult::Done)
		{
			for (model::AnimPolygon const &one : anim.polygons)
			{
				if (animated < 0 && one.polygon >= 0 && (size_t)one.polygon < shape.polygons.size())
				{
					animated = one.polygon;
				}
			}
		}

		int const here = (merged ? 1 : 0) + (listed ? 1 : 0) + (HasChunk(object, "SHPCENTR") ? 1 : 0) + (texturedOne >= 0 ? 1 : 0) + (partnered >= 0 ? 1 : 0) + (animated >= 0 ? 4 : 0);

		if (here > score)
		{
			score = here;
			have = true;
			memcpy(id, thisId, 8);
			original = shape;
			originalLists = lists;
			originalMerge = merge;
			originalAnim = anim;
			take.clear();
			take.push_back(partnered >= 0 ? partnered : 1);

			if (texturedOne >= 0 && texturedOne != take[0])
			{
				take.push_back(texturedOne);
			}

			if (animated >= 0)
			{
				take.push_back(animated);
			}

			take.push_back(2 == take[0] ? 3 : 2);
		}

		if (score >= 9)
		{
			break;
		}
	}

	if (!have)
	{
		printf("  --  %s: no shape to take polygons out of\n", level.c_str());
		return;
	}

	std::sort(take.begin(), take.end());
	take.erase(std::unique(take.begin(), take.end()), take.end());

	std::vector<char> imageBefore;

	baseline.BuildLevel(imageBefore);

	OpenLevelFile work;
	std::string const label = level + " Remove";

	Check(work.Open(levelPath.c_str()) == LevelOpenResult::Opened, label + ": the level is opened again");

	// refusals first
	{
		std::vector<int32_t> every(original.polygons.size());
		std::string why;
		std::vector<char> now;

		for (size_t i = 0; i < every.size(); i++)
		{
			every[i] = (int32_t)i;
		}

		uint8_t const nobody[8] = { 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef };

		Check(host::PolyEdit_Remove(&work, id, every.data(), every.size(), nullptr, &why) == host::PolyRemoveResult::AllPolygons, label + ": every polygon is refused");
		Check(host::PolyEdit_Remove(&work, nobody, take.data(), take.size(), nullptr, &why) == host::PolyRemoveResult::NoObject, label + ": an ID no object has is refused");
		Check(work.BuildLevel(now) && now == imageBefore, label + ": after the refusals the level is as it was");
	}

	host::PolyRemoveMap map;
	std::string why;
	host::PolyRemoveResult const result = host::PolyEdit_Remove(&work, id, take.data(), take.size(), &map, &why);

	Check(result == host::PolyRemoveResult::Done, label + ": " + std::to_string(take.size()) + " polygons are taken out" + (result == host::PolyRemoveResult::Done ? "" : std::string(": ") + host::PolyRemoveWords(result) + " " + why));

	if (result != host::PolyRemoveResult::Done)
	{
		return;
	}

	std::string const copyPath = CopyPathIn(copyFolder, (level + "-poly-removed.rif").c_str());

	DeleteFileA(copyPath.c_str());
	Check(work.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, label + ": the level is saved as a COPY");

	OpenLevelFile edited;

	Check(edited.Open(copyPath.c_str()) == LevelOpenResult::Opened && edited.Level(), label + ": the game's loader takes the copy");

	if (!edited.Level())
	{
		return;
	}

	model::ShapeModel now;
	std::vector<model::UvList> lists;
	std::string ignore;
	Object_Chunk *const object = FindObject(edited, id);

	Check(host::ShapeEdit_Read(edited.Level(), &edited, id, &now, nullptr, &ignore) == host::ShapeEditResult::Done, label + ": the edited shape is read");
	Check(now.polygons.size() + take.size() == original.polygons.size() && now.polygonNormals.size() == now.polygons.size(), label + ": " + std::to_string(take.size()) + " fewer polygons, one normal for each");
	Check(map.polygon.size() == original.polygons.size() && map.point.size() == original.points.size(), label + ": the map is the old shape's size");

	// every kept polygon is the old one: same corners in space, same type, flags and normal, same picture and coordinates
	bool same = now.polygons.size() + take.size() == original.polygons.size();
	size_t checked = 0;

	if (ReadLists(edited, object, &lists) || originalLists.empty())
	{
		for (size_t i = 0; i < original.polygons.size() && same; i++)
		{
			if (map.polygon[i] < 0)
			{
				continue;
			}

			model::ShapePolygon const &a = original.polygons[i];
			model::ShapePolygon const &b = now.polygons[(size_t)map.polygon[i]];

			same = a.engineType == b.engineType && a.flags == b.flags && a.cornerCount == b.cornerCount && b.normalIndex == map.polygon[i];

			for (int c = 0; same && c < a.cornerCount; c++)
			{
				model::ShapePoint const &pa = original.points[(size_t)a.corners[c]];
				model::ShapePoint const &pb = now.points[(size_t)b.corners[c]];

				same = memcmp(&pa, &pb, sizeof(pa)) == 0;
			}

			model::ShapeNormal const &na = original.polygonNormals[(size_t)a.normalIndex];
			model::ShapeNormal const &nb = now.polygonNormals[(size_t)b.normalIndex];

			same = same && memcmp(&na, &nb, sizeof(na)) == 0;

			if (same && model::EngineTypeIsTextured(a.engineType) && !(a.colour & 0x8000))
			{
				int32_t const la = model::PolygonUvListNumber(a.colour), lb = model::PolygonUvListNumber(b.colour);

				same = model::PolygonPictureNumber(a.colour) == model::PolygonPictureNumber(b.colour) && la >= 0 && lb >= 0 && (size_t)la < originalLists.size() && (size_t)lb < lists.size() && SameList(originalLists[(size_t)la], lists[(size_t)lb]);
			}
			else if (same)
			{
				same = a.colour == b.colour;
			}

			checked++;
		}
	}

	Check(same && checked == now.polygons.size(), label + ": all " + std::to_string(checked) + " kept polygons are the old ones (corners in space, type, flags, normal, picture and coordinates)");

	size_t used = 0;

	for (int32_t const p : map.point)
	{
		used += p >= 0 ? 1 : 0;
	}

	model::ShapeExtents extents = {};

	Check(now.points.size() == used && now.pointNormals.size() == (original.pointNormals.empty() ? 0 : used), label + ": the points kept are exactly the ones a kept polygon uses (" + std::to_string(used) + " of " + std::to_string(original.points.size()) + ")");
	Check(model::ShapeExtentsOf(now.points, &extents) == model::ShapeMathsResult::Ok && memcmp(&extents, &now.extents, sizeof(extents)) == 0 && now.radius == model::ShapeRadiusOf(now.points), label + ": the extents and the radius are those of the points left");

	if (!originalMerge.empty())
	{
		std::vector<int32_t> merge;
		bool mergeOk = MergeNumbers(edited, object, &merge) && merge.size() == now.polygons.size();

		for (size_t i = 0; mergeOk && i < originalMerge.size(); i++)
		{
			if (map.polygon[i] >= 0)
			{
				int32_t const partner = originalMerge[i];

				mergeOk = merge[(size_t)map.polygon[i]] == (partner >= 0 ? map.polygon[(size_t)partner] : -1);
			}
		}

		Check(mergeOk, label + ": the merge data is renumbered (a partner taken out becomes -1)");
	}

	Chunk *const centre = object && object->get_assoc_shape() ? object->get_assoc_shape()->lookup_single_child("SHPCENTR") : nullptr;
	std::vector<unsigned char> centreBytes;

	if (centre && edited.PayloadOf(centre, &centreBytes) && centreBytes.size() == 16)
	{
		int32_t mid[3];
		float radius;

		memcpy(mid, centreBytes.data(), 12);
		memcpy(&radius, centreBytes.data() + 12, 4);
		int32_t const want[3] = { (int32_t)(((int64_t)now.extents.maxX + now.extents.minX) / 2), (int32_t)(((int64_t)now.extents.maxY + now.extents.minY) / 2),
			(int32_t)(((int64_t)now.extents.maxZ + now.extents.minZ) / 2) };
		float const wantRadius = model::ShapeRadiusAboutCentre(now);

		Check(mid[0] == want[0] && mid[1] == want[1] && mid[2] == want[2] && radius == wantRadius, label + ": SHPCENTR is worked out again from the new extents (it holds " + std::to_string(mid[0]) + " " +
			std::to_string(mid[1]) + " " + std::to_string(mid[2]) + " r " + std::to_string(radius) + "; the extents give " + std::to_string(want[0]) + " " + std::to_string(want[1]) + " " +
			std::to_string(want[2]) + " r " + std::to_string(wantRadius) + "; " + std::to_string(now.points.size()) + " points now, " + std::to_string(original.points.size()) + " before; over the points before " + std::to_string([&]() { model::ShapeModel old = original; old.extents = now.extents; return model::ShapeRadiusAboutCentre(old); }()) + ")");
	}

	// every other shape is the baseline's
	List<Object_Chunk *> others;
	size_t compared = 0, differ = 0;

	baseline.Level()->list_objects(&others);

	for (LIF<Object_Chunk *> each(&others); !each.done(); each.next())
	{
		Object_Chunk *const o = each();

		if (!o || !o->get_assoc_shape())
		{
			continue;
		}

		model::ShapeModel a, b;
		std::string skip;
		uint8_t const *const otherId = (uint8_t const *)&o->object_data.ID;
		bool const readA = host::ShapeEdit_Read(baseline.Level(), &baseline, otherId, &a, nullptr, &skip) == host::ShapeEditResult::Done;
		bool const readB = host::ShapeEdit_Read(edited.Level(), &edited, otherId, &b, nullptr, &skip) == host::ShapeEditResult::Done;

		if (readA && a.number == original.number)
		{
			continue;
		}

		differ += (readA != readB || (readA && !SameShape(a, b))) ? 1 : 0;
		compared++;
	}

	// the texture animation: the records of the polygons taken out are gone, the others have their new polygon number and the rest of their record as it was
	{
		int32_t shapeNumber = 0;
		model::TextureAnim anim;

		Check(host::AnimEdit_Read(&edited, id, &shapeNumber, &anim) == host::AnimEditResult::Done, label + ": the animation of the edited shape is read");

		size_t expected = 0;
		bool animOk = true;

		for (model::AnimPolygon const &one : originalAnim.polygons)
		{
			if (one.polygon < 0 || (size_t)one.polygon >= map.polygon.size() || map.polygon[(size_t)one.polygon] < 0)
			{
				continue;
			}

			bool found = false;

			for (model::AnimPolygon const &now3 : anim.polygons)
			{
				if (now3.polygon == map.polygon[(size_t)one.polygon])
				{
					found = now3.group == one.group && now3.flags == one.flags && now3.identifier == one.identifier && now3.sequences.size() == one.sequences.size();
				}
			}

			animOk = animOk && found;
			expected++;
		}

		if (originalAnim.polygons.empty())
		{
			printf("  --  %s: this shape has no texture animation\n", label.c_str());
		}
		else
		{
			Check(animOk && anim.polygons.size() == expected, label + ": the animation keeps the " + std::to_string(expected) + " records of the polygons left, renumbered, and has none for those taken out");
		}
	}

	Check(compared > 0 && differ == 0, label + ": the other " + std::to_string(compared) + " shapes are the baseline's, number for number");
	Check(ObjectCount(edited) == ObjectCount(baseline), label + ": the level has the baseline's number of objects");
}

// ================================================================= E. Unmerge selected polygons (PolyEdit_Unmerge) on a stock level

static void UnmergeOnLevel(std::string const &levelPath, std::string const &copyFolder)
{
	std::string const level = BaseName(levelPath);

	printf("UNMERGE %s\n", level.c_str());

	OpenLevelFile baseline;

	if (baseline.Open(levelPath.c_str()) != LevelOpenResult::Opened || !baseline.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	uint8_t id[8] = { 0 };
	uint8_t plainId[8] = { 0 };
	bool have = false, havePlain = false;
	std::vector<int32_t> merge;
	model::ShapeModel original;
	int32_t pairedPolygon = -1, loner = -1;
	List<Object_Chunk *> all;

	baseline.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !object->get_assoc_shape())
		{
			continue;
		}

		uint8_t const *const thisId = (uint8_t const *)&object->object_data.ID;
		model::ShapeModel shape;
		std::string why;

		if (host::ShapeEdit_Read(baseline.Level(), &baseline, thisId, &shape, nullptr, &why) != host::ShapeEditResult::Done)
		{
			continue;
		}

		if (!HasChunk(object, "SHPMRGDT"))
		{
			if (!havePlain)
			{
				memcpy(plainId, thisId, 8);
				havePlain = true;
			}

			continue;
		}

		std::vector<int32_t> numbers;

		if (have || !MergeNumbers(baseline, object, &numbers) || numbers.size() != shape.polygons.size())
		{
			continue;
		}

		int32_t paired = -1, alone = -1;

		for (size_t p = 0; p < numbers.size(); p++)
		{
			if (paired < 0 && numbers[p] >= 0 && (size_t)numbers[p] < numbers.size() && numbers[(size_t)numbers[p]] == (int32_t)p)
			{
				paired = (int32_t)p;
			}

			if (alone < 0 && numbers[p] < 0)
			{
				alone = (int32_t)p;
			}
		}

		if (paired >= 0)
		{
			memcpy(id, thisId, 8);
			have = true;
			merge = numbers;
			original = shape;
			pairedPolygon = paired;
			loner = alone;
		}
	}

	if (!have)
	{
		printf("  --  %s: no shape with a merged pair\n", level.c_str());
		return;
	}

	std::string const label = level + " Unmerge";
	std::vector<char> imageBefore, now;
	std::string why;
	OpenLevelFile work;

	baseline.BuildLevel(imageBefore);
	Check(work.Open(levelPath.c_str()) == LevelOpenResult::Opened, label + ": the level is opened again");

	int32_t const outside[1] = { 0x7fffff };
	int32_t const one[1] = { pairedPolygon };
	size_t parted = 99;

	Check(host::PolyEdit_Unmerge(&work, id, outside, 1, &parted, &why) == host::PolyUnmergeResult::NoPolygon, label + ": a polygon the shape lacks is refused");

	if (havePlain)
	{
		Check(host::PolyEdit_Unmerge(&work, plainId, one, 1, &parted, &why) == host::PolyUnmergeResult::NoMergeData, label + ": a shape with no merge data is refused");
	}

	Check(work.BuildLevel(now) && now == imageBefore, label + ": after the refusals the level is as it was");

	if (loner >= 0)
	{
		int32_t const alone[1] = { loner };

		Check(host::PolyEdit_Unmerge(&work, id, alone, 1, &parted, &why) == host::PolyUnmergeResult::Done && parted == 0 && work.BuildLevel(now) && now == imageBefore, label + ": a polygon with no partner changes nothing");
	}

	Check(host::PolyEdit_Unmerge(&work, id, one, 1, &parted, &why) == host::PolyUnmergeResult::Done && parted == 1, label + ": a merged polygon is parted from its partner");

	std::string const copyPath = CopyPathIn(copyFolder, (level + "-poly-unmerged.rif").c_str());

	DeleteFileA(copyPath.c_str());
	Check(work.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, label + ": the level is saved as a COPY");

	OpenLevelFile edited;

	Check(edited.Open(copyPath.c_str()) == LevelOpenResult::Opened && edited.Level(), label + ": the game's loader takes the copy");

	if (edited.Level())
	{
		std::vector<int32_t> after;
		Object_Chunk *const object = FindObject(edited, id);
		model::ShapeModel shape;
		std::string ignore;
		int32_t const partner = merge[(size_t)pairedPolygon];
		bool others = MergeNumbers(edited, object, &after) && after.size() == merge.size();

		for (size_t i = 0; others && i < merge.size(); i++)
		{
			others = (int32_t)i == pairedPolygon || (int32_t)i == partner ? after[i] == -1 : after[i] == merge[i];
		}

		Check(others, label + ": both numbers are -1 and every other number is as it was");
		Check(host::ShapeEdit_Read(edited.Level(), &edited, id, &shape, nullptr, &ignore) == host::ShapeEditResult::Done && SameShape(shape, original), label + ": the shape's polygons and points are as they were");
	}

	// MERGE: the two polygons just parted are paired again by inteng's own joinability test (they were a quad the loader made, so they are flat and convex)
	{
		int32_t const both[2] = { pairedPolygon, merge[(size_t)pairedPolygon] };
		size_t joined = 99;
		std::string mergeWhy;
		host::PolyMergeResult const result = host::PolyEdit_Merge(&work, id, both, 2, &joined, &mergeWhy);

		Check(result == host::PolyMergeResult::Done, level + " Merge: the parted polygons are merged" + (result == host::PolyMergeResult::Done ? "" : std::string(": ") + host::PolyMergeWords(result) + " " + mergeWhy));
		Check(joined == 1, level + " Merge: one pair is made (" + std::to_string(joined) + ")");

		std::vector<int32_t> again;
		Object_Chunk *const object = FindObject(work, id);

		Check(MergeNumbers(work, object, &again) && again == merge, level + " Merge: the merge data is the original's again");

		int32_t const outside2[1] = { 0x7fffff };

		Check(host::PolyEdit_Merge(&work, id, outside2, 1, &joined, &mergeWhy) == host::PolyMergeResult::NoPolygon, level + " Merge: a polygon the shape lacks is refused");
	}
}

static void MergeInNumbers()
{
	printf("MERGE, THE NUMBERS\n");

	// the square's points 0..3 (z = 0), point 4 above the corner 0, point 6 inside; triangles made by Create poly so their normals are real
	model::ShapeModel shape = SquareShape();
	int32_t const t0[3] = { 0, 1, 2 }, t1[3] = { 0, 2, 3 }, t2[3] = { 0, 2, 4 };
	double const noView[3] = { 0, 0, 0 };

	shape.polygons.clear();
	host::PolyEdit_CreateInModel(&shape, nullptr, t0, 3, noView, nullptr);
	host::PolyEdit_CreateInModel(&shape, nullptr, t1, 3, noView, nullptr);
	host::PolyEdit_CreateInModel(&shape, nullptr, t2, 3, noView, nullptr);

	double score = -9;

	Check(host::PolyEdit_Joinable(shape, 0, 1, false, &score) && score > -0.001 && score < 0.001, "two flat triangles that share an edge and make a convex quad are joinable");
	Check(!host::PolyEdit_Joinable(shape, 0, 2, false, nullptr), "a triangle whose free corner is 500 off the plane is not joinable");
	Check(!host::PolyEdit_Joinable(shape, 0, 0, false, nullptr) && !host::PolyEdit_Joinable(shape, 0, 9, false, nullptr), "a polygon with itself, or one the shape lacks, is not joinable");

	std::vector<int32_t> merge = { -1, -1, -1 };
	int32_t const pair[2] = { 0, 1 };

	Check(host::PolyEdit_MergeInModel(shape, &merge, pair, 2, false) == 1 && merge[0] == 1 && merge[1] == 0 && merge[2] == -1, "Merge pairs them, each naming the other");
	Check(host::PolyEdit_MergeInModel(shape, &merge, pair, 2, false) == 0 && merge[0] == 1 && merge[1] == 0, "a pair that is paired already is left alone");

	std::vector<int32_t> three = { -1, -1, -1 };
	int32_t const every[3] = { 0, 1, 2 };

	Check(host::PolyEdit_MergeInModel(shape, &three, every, 3, false) == 1 && three[2] == -1 && three[0] == 1 && three[1] == 0, "three polygons, only the joinable two are paired (the lowest score first)");

	std::vector<int32_t> wrong = { -1 };

	Check(host::PolyEdit_MergeInModel(shape, &wrong, every, 3, false) == 0, "merge data that is not one number for each polygon makes no pair");
}

// TM-B 2e (Dietrich, 2026-11-18): FUN_0055d500, Merge's save checking its pairs (reassess/tm-b-merge-map.md). The pair: polygon 1 (corners 0, 2, 3) with polygon 0 (corners 0, 1, 2), sharing
// the points 0 and 2; both textured (type 5, picture 3), their lists matching at the shared corners.
struct CheckedPair
{
	model::ShapeModel shape;
	std::vector<model::UvList> lists;
	std::vector<int32_t> merge;
	bool changed = false;
	size_t parted = 0;
};

static CheckedPair MatchingPair()
{
	CheckedPair pair;
	model::ShapePolygon p0 = {}, p1 = {};

	pair.shape = SquareShape();
	pair.shape.polygons.clear();
	p0.engineType = 5;
	p0.flags = 0;
	p0.colour = model::MakePolygonColour(3, 0);
	p0.corners[0] = 0; p0.corners[1] = 1; p0.corners[2] = 2; p0.corners[3] = -1; p0.corners[4] = -1;
	p1 = p0;
	p1.colour = model::MakePolygonColour(3, 1);
	p1.corners[0] = 0; p1.corners[1] = 2; p1.corners[2] = 3;
	pair.shape.polygons.push_back(p0);
	pair.shape.polygons.push_back(p1);

	model::UvList l0 = {}, l1 = {};

	l0.cornerCount = 3;
	l0.corners[0] = { 0, 0 }; l0.corners[1] = { 64, 0 }; l0.corners[2] = { 64, 64 };
	l1.cornerCount = 3;
	l1.corners[0] = { 0, 0 }; l1.corners[1] = { 64, 64 }; l1.corners[2] = { 0, 64 };
	pair.lists.push_back(l0);
	pair.lists.push_back(l1);
	pair.merge = { 1, 0 };

	return pair;
}

static void Run(CheckedPair *pair)
{
	pair->parted = host::PolyEdit_CheckMergedPairs(&pair->shape, pair->lists, &pair->merge, &pair->changed);
}

static bool Kept(CheckedPair const &pair)
{
	return pair.parted == 0 && pair.merge[0] == 1 && pair.merge[1] == 0;
}

static bool Parted(CheckedPair const &pair)
{
	return pair.parted == 1 && pair.merge[0] == -1 && pair.merge[1] == -1;
}

static void CheckPairsInNumbers()
{
	printf("MERGE'S SAVE CHECKS, THE NUMBERS\n");

	CheckedPair same = MatchingPair();

	Run(&same);
	Check(Kept(same) && !same.changed, "a matching pair is kept and no polygon changes");

	CheckedPair flags = MatchingPair();

	flags.shape.polygons[0].flags = 0x1;
	flags.shape.polygons[1].flags = 0x4;
	Run(&flags);
	Check(Kept(flags) && flags.changed && flags.shape.polygons[0].flags == 0x5 && flags.shape.polygons[1].flags == 0x5, "flags that differ: each polygon gets the two OR'd (0x55d558), the pair kept");

	CheckedPair gouraud = MatchingPair();

	gouraud.shape.polygons[1].engineType = 6;
	Run(&gouraud);
	Check(Kept(gouraud) && !gouraud.changed && gouraud.shape.polygons[1].engineType == 6, "types 5 and 6 are both 5 as Texture Map holds them (0x55da15): kept, the type left");

	CheckedPair threeD = MatchingPair();

	threeD.shape.polygons[1].engineType = 0x17;
	Run(&threeD);
	Check(Kept(threeD) && threeD.changed && threeD.shape.polygons[1].engineType == 5 && threeD.shape.polygons[0].engineType == 5,
		"types 5 and 0x17 (held 5 and 7): the higher-numbered polygon takes its partner's type (0x55d5d8), kept");

	CheckedPair picture = MatchingPair();

	picture.shape.polygons[1].colour = model::MakePolygonColour(4, 1);
	Run(&picture);
	Check(Parted(picture), "another picture parts the pair (0x55d636)");

	CheckedPair place = MatchingPair();

	place.lists[1].corners[1] = { 65, 64 };
	Run(&place);
	Check(Parted(place), "another place on the picture at a shared corner (the point 2) parts the pair (FUN_0055cdf0, 0x55d684)");

	CheckedPair fraction = MatchingPair();

	fraction.lists[1].corners[1] = { 64.6f, 64.2f };
	Run(&fraction);
	Check(Kept(fraction), "a place that differs by less than a texel is the same whole texel, as the game's loader holds it: kept");

	CheckedPair freeCorner = MatchingPair();

	freeCorner.lists[1].corners[2] = { 5, 5 };
	Run(&freeCorner);
	Check(Kept(freeCorner), "another place at a corner they don't share (the point 3) is not compared: kept");

	CheckedPair plain = MatchingPair();
	int32_t const textureColour = plain.shape.polygons[0].colour;

	plain.shape.polygons[1].engineType = 2;
	plain.shape.polygons[1].colour = 0x1234;
	Run(&plain);
	Check(Parted(plain), "one textured and one untextured parts the pair (0x55d5e3)");
	Check(plain.shape.polygons[1].colour == 0x1234 && plain.shape.polygons[0].colour == textureColour,
		"INTENG BUG, FIXED: the parted untextured polygon keeps its colour (inteng gives it its partner's picture number), the textured one its picture");

	CheckedPair flat = MatchingPair();

	flat.shape.polygons[0].engineType = 2;
	flat.shape.polygons[0].colour = 0x10;
	flat.shape.polygons[1].engineType = 2;
	flat.shape.polygons[1].colour = 0x20;
	Run(&flat);
	Check(Kept(flat) && flat.changed && flat.shape.polygons[1].colour == 0x10 && flat.shape.polygons[0].colour == 0x10,
		"two untextured of one type: the higher-numbered takes its partner's colour (0x55d6d6), kept");

	CheckedPair mixedPlain = MatchingPair();

	mixedPlain.shape.polygons[0].engineType = 2;
	mixedPlain.shape.polygons[0].colour = 0x10;
	mixedPlain.shape.polygons[1].engineType = 3;
	mixedPlain.shape.polygons[1].colour = 0x20;
	Run(&mixedPlain);
	Check(Parted(mixedPlain) && mixedPlain.shape.polygons[1].colour == 0x10, "two untextured of different types: parted, the colour taken (0x55d5e3, 0x55d6d6)");

	CheckedPair emptyList = MatchingPair();

	emptyList.lists[1].cornerCount = 0;
	Run(&emptyList);
	Check(Parted(emptyList) && emptyList.shape.polygons[1].colour == model::MakePolygonColour(3, 1),
		"a textured polygon whose list is empty is held untextured (0x55d8d2): parted from a textured partner, its colour word left");

	CheckedPair alone = MatchingPair();

	alone.merge = { -1, -1 };
	Run(&alone);
	Check(alone.parted == 0 && !alone.changed && alone.merge[0] == -1 && alone.merge[1] == -1, "no pair: nothing checked");

	CheckedPair wrongSize = MatchingPair();

	wrongSize.merge = { 1 };
	Run(&wrongSize);
	Check(wrongSize.parted == 0 && !wrongSize.changed && wrongSize.merge.size() == 1, "merge data that is not one number for each polygon is left");
}

// The copy's FRESH header (Bishop 2026-11-18, from Dietrich's TB-C8b read): inteng's FUN_0050e880 makes the copy's SHPHEAD1 anew (version 0, flags 0, number -1, no lock user), and
// Texture Map's two copies of a shape go through it: Merge on a shared shape (FUN_0055aa50) and Split (FUN_00564300). Ours keeps the shape's own flags less the lock flag (inteng's 0 loses
// them: INTENG BUG, FIXED, as shape_merge_edit.cpp FreshHead). Before the edit, the old shape is given a header the copy must NOT keep: the lock flag and a shape flag, a lock user, a
// version. SHPHEAD1's payload: flags at 0, the lock user's 16 bytes at 4, the version at 60 (avpx chunk_shape.cpp Shape_Header_Chunk::fill_data_block); the save adds 1 to every
// version (Shape_Header_Chunk::prepare_for_output).
static int32_t const kLockedFlag = 0x1;             // GENERAL_FLAG_LOCKED (chunk.hpp)
static int32_t const kUnstableBoundFlag = 0x4000;   // SHAPE_FLAG_UNSTABLEBOUND_ZPOS (chunk_shape.hpp): a flag of the shape's own, which the copy keeps
static int32_t const kPlantedVersion = 41;

static int32_t HeadNumberAt(std::vector<unsigned char> const &bytes, size_t at)
{
	int32_t number = -1;

	if (bytes.size() >= at + 4)
	{
		memcpy(&number, bytes.data() + at, 4);
	}

	return number;
}

static bool PlantShapeHeader(OpenLevelFile *open, Object_Chunk *object, int32_t *flags)
{
	Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;
	Shape_Header_Chunk *const head = shape ? shape->get_header() : nullptr;
	std::vector<unsigned char> bytes;

	if (!head || !open->PayloadOf(head, &bytes) || bytes.size() < 68)
	{
		return false;
	}

	int32_t const planted = HeadNumberAt(bytes, 0) | kLockedFlag | kUnstableBoundFlag;
	char const user[16] = { 'f', 'r', 'o', 's', 't' };

	memcpy(bytes.data(), &planted, 4);
	memcpy(bytes.data() + 4, user, 16);
	memcpy(bytes.data() + 60, &kPlantedVersion, 4);
	*flags = planted;
	return open->SetPayload(head, bytes);
}

// read back from the saved copy: copyUser's shape is the new one, oldUser's the one it was copied from
static void CheckFreshHeader(std::string const &label, OpenLevelFile const &after, Object_Chunk *copyUser, Object_Chunk *oldUser, bool planted, int32_t plantedFlags)
{
	Shape_Chunk *const newShape = copyUser ? copyUser->get_assoc_shape() : nullptr;
	Shape_Chunk *const oldShape = oldUser ? oldUser->get_assoc_shape() : nullptr;
	std::vector<unsigned char> fresh, kept;
	bool const read = planted && newShape && oldShape && newShape != oldShape && newShape->get_header() && oldShape->get_header() && after.PayloadOf(newShape->get_header(), &fresh) &&
		after.PayloadOf(oldShape->get_header(), &kept) && fresh.size() >= 68 && kept.size() >= 68;
	bool noUser = read;

	for (size_t b = 4; noUser && b < 20; b++)
	{
		noUser = fresh[b] == 0;
	}

	Check(read, label + ": the old shape's header was given a lock flag, a lock user and version " + std::to_string(kPlantedVersion) + " before the edit, and both headers are read back");
	Check(read && HeadNumberAt(fresh, 60) == 1, label + ": the new shape's version is a new shape's: 0, and 1 after the save (" + std::to_string(read ? HeadNumberAt(fresh, 60) : -1) + ")");
	Check(noUser, label + ": the new shape has no lock user" + (read && !noUser ? std::string(" (it has \"") + std::string((char const *)fresh.data() + 4, strnlen((char const *)fresh.data() + 4, 16)) + "\")" : ""));
	Check(read && HeadNumberAt(fresh, 0) == (plantedFlags & ~kLockedFlag), label + ": the new shape keeps the shape's own flags, without the lock flag: " + std::to_string(read ? HeadNumberAt(fresh, 0) : -1) +
		" (the old shape's were " + std::to_string(plantedFlags) + ")");
	Check(read && HeadNumberAt(kept, 60) == kPlantedVersion + 1 && HeadNumberAt(kept, 0) == plantedFlags && kept[4] == 'f', label + ": the old shape keeps its own header: version " +
		std::to_string(read ? HeadNumberAt(kept, 60) : -1) + ", its flags and lock user");
}

// ================================================================= F. Split Object > As Placed Obj (PolyEdit_SplitAsObject) on a stock level

static void SplitOnLevel(std::string const &levelPath, std::string const &copyFolder)
{
	std::string const level = BaseName(levelPath);

	printf("SPLIT %s\n", level.c_str());

	OpenLevelFile baseline;

	if (baseline.Open(levelPath.c_str()) != LevelOpenResult::Opened || !baseline.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	// a shape of 8 to 6000 polygons that nothing refuses, the one with the most of: merge data, lists of texture coordinates, a SHPCENTR, and not the shape of a module (a placed object's place is easier to read back)
	uint8_t id[8] = { 0 };
	bool have = false;
	int score = -1;
	model::ShapeModel original;
	std::vector<int32_t> merge;
	std::vector<model::UvList> lists;
	List<Object_Chunk *> all;

	baseline.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !object->get_assoc_shape() || HasChunk(object, "SHPMORPH") || HasChunk(object, "ANIMSEQU") || HasChunk(object, "SHPPCINF") || HasChunk(object, "SHPFRAGS") || HasChunk(object, "SHPFRGTP") || HasChunk(object, "SHPEXTFL"))
		{
			continue;
		}

		model::ShapeModel shape;
		std::string why;
		uint8_t const *const thisId = (uint8_t const *)&object->object_data.ID;

		size_t users = 0;

		if (host::ShapeEdit_Read(baseline.Level(), &baseline, thisId, &shape, &users, &why) != host::ShapeEditResult::Done || users != 1 || shape.polygons.size() < 8 || shape.polygons.size() > 6000)
		{
			continue;
		}

		std::vector<model::UvList> thisLists;
		std::vector<int32_t> thisMerge;
		bool const merged = HasChunk(object, "SHPMRGDT") && MergeNumbers(baseline, object, &thisMerge) && thisMerge.size() == shape.polygons.size();
		bool const listed = ReadLists(baseline, object, &thisLists) && !thisLists.empty();
		int const here = (merged ? 1 : 0) + (listed ? 1 : 0) + (HasChunk(object, "SHPCENTR") ? 1 : 0);

		if (here > score)
		{
			score = here;
			have = true;
			memcpy(id, thisId, 8);
			original = shape;
			merge = thisMerge;
			lists = thisLists;
		}

		if (score >= 3)
		{
			break;
		}
	}

	if (!have)
	{
		printf("  --  %s: no shape to split\n", level.c_str());
		return;
	}

	std::string const label = level + " Split";
	std::vector<int32_t> take = { 1, 3, 4 };
	std::vector<char> imageBefore, now;

	baseline.BuildLevel(imageBefore);

	OpenLevelFile work;

	Check(work.Open(levelPath.c_str()) == LevelOpenResult::Opened, label + ": the level is opened again");

	// refusals leave the level as it was
	{
		std::vector<int32_t> every(original.polygons.size());
		std::string why;

		for (size_t i = 0; i < every.size(); i++)
		{
			every[i] = (int32_t)i;
		}

		uint8_t const nobody[8] = { 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef };

		Check(host::PolyEdit_SplitAsObject(&work, id, every.data(), every.size(), nullptr, nullptr, &why) == host::PolySplitResult::AllPolygons, label + ": every polygon is refused");
		Check(host::PolyEdit_SplitAsObject(&work, nobody, take.data(), take.size(), nullptr, nullptr, &why) == host::PolySplitResult::NoObject, label + ": an ID no object has is refused");
		Check(work.BuildLevel(now) && now == imageBefore, label + ": after the refusals the level is as it was");
	}

	uint8_t madeId[8] = { 0 };
	std::string made, why;
	int32_t plantedFlags = 0;
	bool const planted = PlantShapeHeader(&work, FindObject(work, id), &plantedFlags);   // the new shape's header is a fresh one (CheckFreshHeader)
	host::PolySplitResult const result = host::PolyEdit_SplitAsObject(&work, id, take.data(), take.size(), madeId, &made, &why);

	Check(result == host::PolySplitResult::Done, label + ": " + std::to_string(take.size()) + " polygons are split off" + (result == host::PolySplitResult::Done ? "" : std::string(": ") + host::PolySplitWords(result) + " " + why));

	if (result != host::PolySplitResult::Done)
	{
		return;
	}

	std::string const copyPath = CopyPathIn(copyFolder, (level + "-poly-split.rif").c_str());

	DeleteFileA(copyPath.c_str());
	Check(work.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, label + ": the level is saved as a COPY");

	OpenLevelFile edited;

	Check(edited.Open(copyPath.c_str()) == LevelOpenResult::Opened && edited.Level(), label + ": the game's loader takes the copy");

	if (!edited.Level())
	{
		return;
	}

	Check(ObjectCount(edited) == ObjectCount(baseline) + 1, label + ": the level has one object more (" + made + ")");

	Object_Chunk *const oldObject = FindObject(edited, id);
	Object_Chunk *const newObject = FindObject(edited, madeId);

	Check(newObject != nullptr && oldObject != nullptr, label + ": the new object and the old one are both there");

	if (!newObject || !oldObject)
	{
		return;
	}

	model::ShapeModel oldNow, newNow;
	std::string ignore;

	Check(host::ShapeEdit_Read(edited.Level(), &edited, id, &oldNow, nullptr, &ignore) == host::ShapeEditResult::Done && host::ShapeEdit_Read(edited.Level(), &edited, madeId, &newNow, nullptr, &ignore) == host::ShapeEditResult::Done,
		label + ": both shapes are read");
	{
		// does the baseline already have a shape with the number the new one got?
		List<Object_Chunk *> bl;
		size_t clash = 0;
		int probeHighest = -1;

		baseline.Level()->list_objects(&bl);

		for (LIF<Object_Chunk *> each(&bl); !each.done(); each.next())
		{
			model::ShapeModel probe;
			std::string skip;

			bool const readIt = each() && each()->get_assoc_shape() && host::ShapeEdit_Read(baseline.Level(), &baseline, (uint8_t const *)&each()->object_data.ID, &probe, nullptr, &skip) == host::ShapeEditResult::Done;

			probeHighest = readIt && probe.number > probeHighest ? probe.number : probeHighest;

			if (readIt && probe.number == newNow.number)
			{
				clash++;
				printf("  (clash: object %.8s, shape polygons %zu, the baseline's objects read shape numbers up to %d)\n", (char const *)&each()->object_data.ID, probe.polygons.size(), probeHighest);
			}
		}

		Check(clash == 0, label + ": no shape of the level had the number the new one got (" + std::to_string(newNow.number) + ", " + std::to_string(clash) + " objects had it)");
	}

	Check(newNow.number != oldNow.number && newNow.number != original.number, label + ": the new object has a shape of its own (number " + std::to_string(newNow.number) + ")");
	CheckFreshHeader(label, edited, newObject, oldObject, planted, plantedFlags);
	Check(newNow.polygons.size() == take.size() && oldNow.polygons.size() + take.size() == original.polygons.size(), label + ": the new shape has " + std::to_string(take.size()) + " polygons, the old one the rest (new " + std::to_string(newNow.polygons.size()) + ", old " + std::to_string(oldNow.polygons.size()) + ", was " + std::to_string(original.polygons.size()) + ")");

	// each polygon split off is the old one in space (same corners), and the new object lies where the old one does
	bool inNew = newNow.polygons.size() == take.size();

	for (size_t k = 0; inNew && k < take.size(); k++)
	{
		model::ShapePolygon const &a = original.polygons[(size_t)take[k]];
		model::ShapePolygon const &b = newNow.polygons[k];

		inNew = a.engineType == b.engineType && a.flags == b.flags && a.cornerCount == b.cornerCount;

		for (int c = 0; inNew && c < a.cornerCount; c++)
		{
			inNew = memcmp(&original.points[(size_t)a.corners[c]], &newNow.points[(size_t)b.corners[c]], sizeof(model::ShapePoint)) == 0;
		}
	}

	Check(inNew, label + ": each polygon of the new shape is the one split off (corners in space, type and flags)");

	Object_Header_Chunk *const headOld = oldObject->get_header();
	Object_Header_Chunk *const headNew = newObject->get_header();
	std::vector<unsigned char> bytesOld, bytesNew;

	if (headNew && edited.PayloadOf(headNew, &bytesNew) && bytesNew.size() >= 60)
	{
		int32_t shapeId;

		memcpy(&shapeId, &bytesNew[56], 4);
		printf("  (the new object's header names shape %d; its shape reads as %d)\n", (int)shapeId, (int)newNow.number);
	}

	Check(headOld && headNew && edited.PayloadOf(headOld, &bytesOld) && edited.PayloadOf(headNew, &bytesNew) && bytesOld.size() >= 48 && bytesNew.size() >= 48 && memcmp(&bytesOld[20], &bytesNew[20], 28) == 0,
		label + ": the new object has the old one's place and turn");

	// the total number of polygons of the level is the same, and every other shape is the baseline's
	List<Object_Chunk *> others;
	size_t compared = 0, differ = 0;

	baseline.Level()->list_objects(&others);

	for (LIF<Object_Chunk *> each(&others); !each.done(); each.next())
	{
		Object_Chunk *const o = each();

		if (!o || !o->get_assoc_shape())
		{
			continue;
		}

		model::ShapeModel a, b;
		std::string skip;
		uint8_t const *const otherId = (uint8_t const *)&o->object_data.ID;
		bool const readA = host::ShapeEdit_Read(baseline.Level(), &baseline, otherId, &a, nullptr, &skip) == host::ShapeEditResult::Done;
		bool const readB = host::ShapeEdit_Read(edited.Level(), &edited, otherId, &b, nullptr, &skip) == host::ShapeEditResult::Done;

		if (readA && a.number == original.number)
		{
			continue;
		}

		differ += (readA != readB || (readA && !SameShape(a, b))) ? 1 : 0;
		compared++;
	}

	Check(compared > 0 && differ == 0, label + ": the other " + std::to_string(compared) + " shapes are the baseline's, number for number");

	// AS MODULE: the same split, but the new object is a module (the placed flag 0x400 clear, the module flag 0x200 set, no strategy)
	{
		OpenLevelFile mod;
		uint8_t moduleId[8] = { 0 };
		std::string moduleName, moduleWhy;

		Check(mod.Open(levelPath.c_str()) == LevelOpenResult::Opened, label + " As Module: the level is opened again");

		// the name is the caller's: a name an object has already is refused and the level is as it was; a free one is used exactly
		{
			Object_Chunk *const first = FindObject(mod, id);
			char taken[64] = { 0 };
			std::vector<char> beforeName, afterName;

			if (first)
			{
				strncpy_s(taken, sizeof(taken), first->object_data.o_name, _TRUNCATE);
			}

			mod.BuildLevel(beforeName);
			Check(first && host::PolyEdit_SplitAsObject(&mod, id, take.data(), take.size(), nullptr, nullptr, &moduleWhy, true, taken) == host::PolySplitResult::BadName && mod.BuildLevel(afterName) && afterName == beforeName,
				label + " As Module: a name an object has already is refused and the level is as it was");
		}

		host::PolySplitResult const asModule = host::PolyEdit_SplitAsObject(&mod, id, take.data(), take.size(), moduleId, &moduleName, &moduleWhy, true, "SplitModuleTest");

		Check(moduleName == "SplitModuleTest", label + " As Module: the new object has the name given (" + moduleName + ")");

		Check(asModule == host::PolySplitResult::Done, label + " As Module: the polygons are split off into a module" + (asModule == host::PolySplitResult::Done ? "" : std::string(": ") + host::PolySplitWords(asModule) + " " + moduleWhy));

		if (asModule == host::PolySplitResult::Done)
		{
			std::string const modulePath = CopyPathIn(copyFolder, (level + "-poly-split-module.rif").c_str());

			DeleteFileA(modulePath.c_str());
			Check(mod.SaveAs(modulePath.c_str()) == LevelSaveResult::Saved, label + " As Module: the level is saved as a COPY");

			OpenLevelFile loaded;

			Check(loaded.Open(modulePath.c_str()) == LevelOpenResult::Opened && loaded.Level(), label + " As Module: the game's loader takes the copy");

			Object_Chunk *const made2 = loaded.Level() ? FindObject(loaded, moduleId) : nullptr;
			Object_Header_Chunk *const head2 = made2 ? made2->get_header() : nullptr;
			std::vector<unsigned char> flagBytes;
			int32_t flags = 0;

			if (head2 && loaded.PayloadOf(head2, &flagBytes) && flagBytes.size() >= 4)
			{
				memcpy(&flags, flagBytes.data(), 4);
			}

			Check(made2 != nullptr && (flags & 0x400) == 0 && (flags & 0x200) != 0, label + " As Module: the new object is a module (flags " + std::to_string(flags) + ": placed 0x400 clear, module 0x200 set)");
			Check(loaded.Level() && ObjectCount(loaded) == ObjectCount(baseline) + 1, label + " As Module: the level has one object more");
		}
	}
}

// TM-B 2d (Dietrich, 2026-11-18): Merge's save of a SHARED shape that not all its users take (FUN_0055aa50's copy path): the first user gets a COPY of the shape, numbered one past the level's
// highest, holding the new pairing; the other users keep the shape as it was, less the mover's name. Read back through the game's loader from a saved copy.
static int32_t ShapeNumberOf(OpenLevelFile const &open, Object_Chunk *object)
{
	Object_Header_Chunk *const header = object ? object->get_header() : nullptr;
	std::vector<unsigned char> bytes;
	int32_t number = -1;

	if (header && open.PayloadOf(header, &bytes) && bytes.size() >= 60)
	{
		memcpy(&number, bytes.data() + 56, 4);
	}

	return number;
}

static Object_Chunk *ObjectNamed(OpenLevelFile const &open, std::string const &name)
{
	List<Object_Chunk *> all;

	open.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		if (each() && std::string(each()->object_data.o_name, strnlen(each()->object_data.o_name, sizeof(each()->object_data.o_name))) == name)
		{
			return each();
		}
	}

	return nullptr;
}

static void MergeOnCopyOnLevel(std::string const &levelPath, std::string const &copyFolder)
{
	std::string const level = BaseName(levelPath);

	printf("MERGE ON A COPY %s\n", level.c_str());

	OpenLevelFile work;

	if (work.Open(levelPath.c_str()) != LevelOpenResult::Opened || !work.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	// a shape with at least two users and merge data of its own
	uint8_t id[8] = { 0 };
	std::vector<std::string> users;
	std::vector<int32_t> numbers;
	bool have = false;
	List<Object_Chunk *> all;

	work.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done() && !have; each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !HasChunk(object, "SHPMRGDT"))
		{
			continue;
		}

		uint8_t const *const thisId = (uint8_t const *)&object->object_data.ID;
		std::vector<std::string> names;

		if (!host::PolyEdit_ShapeUserNames(&work, thisId, &names) || names.size() < 2 || !MergeNumbers(work, object, &numbers) || numbers.empty())
		{
			continue;
		}

		// the object must be the list's first name (the mover)
		Object_Chunk *const first = ObjectNamed(work, names[0]);

		if (!first)
		{
			continue;
		}

		memcpy(id, &first->object_data.ID, 8);
		users = names;
		have = MergeNumbers(work, first, &numbers) && !numbers.empty();
	}

	if (!have)
	{
		printf("  --  %s: no shared shape with merge data\n", level.c_str());
		return;
	}

	std::string const label = level + " Merge on a copy";
	int32_t const oldNumber = ShapeNumberOf(work, ObjectNamed(work, users[0]));
	std::vector<int32_t> changed = numbers;

	// the new pairing: the first pair parted (or, with none, the numbers as they are)
	for (size_t p = 0; p < changed.size(); p++)
	{
		if (changed[p] > (int32_t)p)
		{
			changed[(size_t)changed[p]] = -1;
			changed[p] = -1;
			break;
		}
	}

	std::string why;
	int32_t plantedFlags = 0;
	bool const planted = PlantShapeHeader(&work, ObjectNamed(work, users[0]), &plantedFlags);   // the copy's header is a fresh one (CheckFreshHeader)
	host::PolyMergeResult const result = host::PolyEdit_SetMergeDataOnCopy(&work, id, std::vector<std::string>(1, users[0]), changed, &why);

	Check(result == host::PolyMergeResult::Done, label + ": " + users[0] + " gets a copy of the shape it shares with " + std::to_string(users.size() - 1) + " more" +
		(result == host::PolyMergeResult::Done ? "" : std::string(": ") + host::PolyMergeWords(result) + " " + why));

	if (result != host::PolyMergeResult::Done)
	{
		return;
	}

	std::string const copyPath = CopyPathIn(copyFolder, (level + "-poly-merge-copy.rif").c_str());

	DeleteFileA(copyPath.c_str());
	Check(work.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, label + ": the level is saved as a COPY");

	OpenLevelFile edited;

	Check(edited.Open(copyPath.c_str()) == LevelOpenResult::Opened && edited.Level(), label + ": the game's loader takes the copy");

	if (!edited.Level())
	{
		return;
	}

	Object_Chunk *const mover = ObjectNamed(edited, users[0]);
	Object_Chunk *const keeper = ObjectNamed(edited, users[1]);
	int32_t const moverNumber = ShapeNumberOf(edited, mover), keeperNumber = ShapeNumberOf(edited, keeper);
	std::vector<std::string> moverUsers, keeperUsers;
	std::vector<int32_t> moverNumbers, keeperNumbers;

	Check(mover && keeper, label + ": both objects are in the copy");
	Check(moverNumber != oldNumber && moverNumber > 0 && keeperNumber == oldNumber, label + ": " + users[0] + " uses a new shape (" + std::to_string(moverNumber) + "), " + users[1] +
		" the old one (" + std::to_string(keeperNumber) + ")");
	Check(mover && host::PolyEdit_ShapeUserNames(&edited, (uint8_t const *)&mover->object_data.ID, &moverUsers) && moverUsers == std::vector<std::string>(1, users[0]),
		label + ": the copy's only user is " + users[0]);

	std::vector<std::string> expectedKeepers(users.begin() + 1, users.end());

	Check(keeper && host::PolyEdit_ShapeUserNames(&edited, (uint8_t const *)&keeper->object_data.ID, &keeperUsers) && keeperUsers == expectedKeepers,
		label + ": the old shape's users are the others, " + users[0] + " gone from its list");
	Check(mover && MergeNumbers(edited, mover, &moverNumbers) && moverNumbers == changed, label + ": the copy holds the new pairing");
	Check(keeper && MergeNumbers(edited, keeper, &keeperNumbers) && keeperNumbers == numbers, label + ": the old shape's pairing is as it was");
	CheckFreshHeader(label, edited, mover, keeper, planted, plantedFlags);
}

// TM-B 2e (Dietrich, 2026-11-18): Merge's save runs FUN_0055d500's checks before it writes (0x55bb73). On a stock shape whose pairs all pass: the first pair's lower polygon given a flag its partner
// lacks, the second pair's higher polygon given another picture; the save writes the first pair kept with the flags OR'd on both, the second parted, every other number as it was. Read back
// through the game's loader from a saved copy.
static bool ShapeLists(OpenLevelFile const &open, Object_Chunk *object, std::vector<model::UvList> *lists)
{
	Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;
	Chunk *const uv = shape ? shape->lookup_single_child("SHPUVCRD") : nullptr;
	std::vector<unsigned char> bytes;

	lists->clear();

	return shape && (!uv || (open.PayloadOf(uv, &bytes) && model::ReadUvLists(bytes.data(), bytes.size(), lists) == model::UvModelResult::Ok));
}

static void CheckPairsOnLevel(std::string const &levelPath, std::string const &copyFolder)
{
	std::string const level = BaseName(levelPath);

	printf("MERGE'S SAVE CHECKS %s\n", level.c_str());

	OpenLevelFile work;

	if (work.Open(levelPath.c_str()) != LevelOpenResult::Opened || !work.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	// a shape with at least two pairs, every pair of it passing the checks as the file holds it, the two first pairs textured
	uint8_t id[8] = { 0 };
	model::ShapeModel shape;
	std::vector<int32_t> numbers;
	size_t stockParted = 0, stockChanged = 0, shapesChecked = 0;
	int32_t pairA[2] = { -1, -1 }, pairB[2] = { -1, -1 };
	bool have = false;
	List<Object_Chunk *> all;

	work.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		std::vector<int32_t> merge;
		std::vector<model::UvList> lists;
		model::ShapeModel model;
		std::string ignore;

		if (!object || !HasChunk(object, "SHPMRGDT") || !MergeNumbers(work, object, &merge) || !ShapeLists(work, object, &lists) ||
			host::ShapeEdit_Read(work.Level(), &work, (uint8_t const *)&object->object_data.ID, &model, nullptr, &ignore) != host::ShapeEditResult::Done || merge.size() != model.polygons.size())
		{
			continue;
		}

		model::ShapeModel tried = model;
		std::vector<int32_t> triedMerge = merge;
		bool changed = false;
		size_t const parted = host::PolyEdit_CheckMergedPairs(&tried, lists, &triedMerge, &changed);

		shapesChecked++;
		stockParted += parted;
		stockChanged += changed ? 1 : 0;

		if (have || parted != 0 || changed)
		{
			continue;
		}

		std::vector<std::pair<int32_t, int32_t>> pairs;

		for (size_t p = 0; p < merge.size(); p++)
		{
			if (merge[p] >= 0 && merge[p] < (int32_t)p && model::EngineTypeIsTextured(model.polygons[p].engineType))
			{
				pairs.push_back(std::make_pair((int32_t)p, merge[p]));
			}
		}

		if (pairs.size() < 2)
		{
			continue;
		}

		memcpy(id, &object->object_data.ID, 8);
		shape = model;
		numbers = merge;
		pairA[0] = pairs[0].first; pairA[1] = pairs[0].second;
		pairB[0] = pairs[1].first; pairB[1] = pairs[1].second;
		have = true;
	}

	printf("  --  %s: %zu shapes with merge data; as the file holds them the checks would part %zu pairs and change the polygons of %zu shapes\n", level.c_str(), shapesChecked, stockParted, stockChanged);

	if (!have)
	{
		printf("  --  %s: no shape with two textured pairs that pass the checks\n", level.c_str());
		return;
	}

	std::string const label = level + " Merge's save checks";
	int32_t const flagsI = shape.polygons[(size_t)pairA[0]].flags, flagsJ = shape.polygons[(size_t)pairA[1]].flags;
	uint32_t free = 1;

	while (free != 0 && ((uint32_t)(flagsI | flagsJ) & free) != 0)
	{
		free <<= 1;
	}

	int32_t const bit = (int32_t)free;

	int32_t const picture = model::PolygonPictureNumber(shape.polygons[(size_t)pairB[0]].colour);

	shape.polygons[(size_t)pairA[1]].flags |= bit;
	shape.polygons[(size_t)pairB[0]].colour = model::MakePolygonColour(picture < 0x7ffe ? picture + 1 : 0, model::PolygonUvListNumber(shape.polygons[(size_t)pairB[0]].colour));

	std::string why;

	Check(bit != 0 && host::ShapeEdit_WriteSameCounts(&work, work.Level(), id, shape, &why) == host::ShapeEditResult::Done, label + ": the two polygons are changed in the editor's bytes " + why);

	size_t parted = 99;
	host::PolyMergeResult const result = host::PolyEdit_SetMergeData(&work, id, numbers, &why, &parted);

	Check(result == host::PolyMergeResult::Done && parted == 1, label + ": the save is written, one pair parted by the checks (" + std::to_string(parted) + ")" +
		(result == host::PolyMergeResult::Done ? "" : std::string(": ") + host::PolyMergeWords(result) + " " + why));

	if (result != host::PolyMergeResult::Done)
	{
		return;
	}

	std::string const copyPath = CopyPathIn(copyFolder, (level + "-poly-merge-checks.rif").c_str());

	DeleteFileA(copyPath.c_str());
	Check(work.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved, label + ": the level is saved as a COPY");

	OpenLevelFile edited;

	Check(edited.Open(copyPath.c_str()) == LevelOpenResult::Opened && edited.Level(), label + ": the game's loader takes the copy");

	if (!edited.Level())
	{
		return;
	}

	Object_Chunk *object = nullptr;
	List<Object_Chunk *> every;

	edited.Level()->list_objects(&every);

	for (LIF<Object_Chunk *> each(&every); !each.done() && !object; each.next())
	{
		if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
		{
			object = each();
		}
	}

	std::vector<int32_t> now;
	model::ShapeModel saved;

	Check(object && MergeNumbers(edited, object, &now) && host::ShapeEdit_Read(edited.Level(), &edited, id, &saved, nullptr, &why) == host::ShapeEditResult::Done &&
		now.size() == numbers.size() && saved.polygons.size() == shape.polygons.size(), label + ": the saved shape is read");

	if (now.size() != numbers.size() || saved.polygons.size() != shape.polygons.size())
	{
		return;
	}

	std::vector<int32_t> expected = numbers;

	expected[(size_t)pairB[0]] = -1;
	expected[(size_t)pairB[1]] = -1;

	Check(now == expected, label + ": the pair given another picture (" + std::to_string(pairB[0]) + " + " + std::to_string(pairB[1]) + ") is written parted, every other number as it was");

	int32_t const both = flagsI | flagsJ | bit;

	Check(saved.polygons[(size_t)pairA[0]].flags == both && saved.polygons[(size_t)pairA[1]].flags == both && now[(size_t)pairA[0]] == pairA[1],
		label + ": the pair whose flags differed (" + std::to_string(pairA[0]) + " + " + std::to_string(pairA[1]) + ") is kept, each polygon with the two OR'd (" +
		std::to_string(saved.polygons[(size_t)pairA[0]].flags) + ", " + std::to_string(saved.polygons[(size_t)pairA[1]].flags) + ", wanted " + std::to_string(both) + ")");
}

// ---- TM-C H1: Remove and Split keep the BONE ATTACHMENT (SHPBONHD) and the VERTEX COLOURS (SHPVTINT) of the points kept. inteng's removal FUN_00435080 (Delete polys' FUN_00563d90 and Split's
// FUN_00564300, which runs it on its copy of the shape, FUN_0050e880) ends with FUN_00435a00: in each SHPBONHD of the shape, SHPBONIN's point count becomes the shape's (0x435aaf), each SHPBONE keeps
// the records (20 bytes: the point, then four more numbers, moved whole) of the points kept, renumbered (0x435b2a-0x435b41), and a SHPBONE left with none is deleted (0x435bb0); then FUN_00435c80: each
// SHPVTINT of every object using the shape becomes one number per point of the shape, a kept point's old number, 0xffffffff for any other (0x435dde-0x435e25).
// No stock level has a bone attachment, so one is put in the shape (two bones: A holds every point, B only the points the polygon taken out frees); the vertex colours are the level's own.

namespace boneTest
{
	typedef std::vector<char> Bytes;

	int32_t IntAt(Bytes const &b, size_t at)
	{
		int32_t v;

		memcpy(&v, b.data() + at, 4);
		return v;
	}

	void PutIntAt(Bytes *b, size_t at, int32_t v)
	{
		memcpy(b->data() + at, &v, 4);
	}

	void PutInt(Bytes *b, int32_t v)
	{
		b->insert(b->end(), (char const *)&v, (char const *)&v + 4);
	}

	void PutText(Bytes *b, std::string const &s)
	{
		size_t const at = b->size();

		b->resize(at + ((s.size() + 4) & ~(size_t)3), 0);
		memcpy(b->data() + at, s.data(), s.size());
	}

	Bytes Start(char const *name)
	{
		Bytes c(12, 0);

		memcpy(c.data(), name, 8);
		return c;
	}

	void Finish(Bytes *c)
	{
		PutIntAt(c, 8, (int32_t)c->size());
	}

	struct Span
	{
		size_t at, size;
	};

	bool Children(Bytes const &b, size_t from, size_t to, std::vector<Span> *out)
	{
		out->clear();

		for (size_t at = from; at < to; )
		{
			if (to - at < 12)
			{
				return false;
			}

			int32_t const size = IntAt(b, at + 8);

			if (size < 12 || (size_t)size > to - at)
			{
				return false;
			}

			out->push_back({ at, (size_t)size });
			at += (size_t)size;
		}

		return true;
	}

	bool Is(Bytes const &b, Span const &s, char const *name)
	{
		return memcmp(b.data() + s.at, name, 8) == 0;
	}

	// the record's four numbers after the point, made from the OLD point number, so a record moved whole can be told from one made again
	std::array<int32_t, 5> Record(int32_t newPoint, int32_t oldPoint)
	{
		return { newPoint, oldPoint * 2 + 1, -oldPoint, oldPoint * 3, 0x3f800000 };
	}

	Bytes BoneHeader(int32_t points, std::vector<std::pair<std::string, std::vector<int32_t>>> const &bones)
	{
		Bytes header = Start("SHPBONHD"), info = Start("SHPBONIN");

		PutInt(&info, 7);
		PutInt(&info, 3);
		PutInt(&info, points);
		PutText(&info, "testhier");
		Finish(&info);
		header.insert(header.end(), info.begin(), info.end());

		for (auto const &bone : bones)
		{
			Bytes data = Start("SHPBONDT"), holder = Start("SHPBONE");

			PutText(&data, bone.first);
			PutInt(&data, (int32_t)bone.second.size());

			for (int32_t const p : bone.second)
			{
				for (int32_t const n : Record(p, p))
				{
					PutInt(&data, n);
				}
			}

			PutInt(&data, 11);   // the two numbers after the records, kept as they were
			PutInt(&data, 12);
			Finish(&data);
			holder.insert(holder.end(), data.begin(), data.end());
			Finish(&holder);
			header.insert(header.end(), holder.begin(), holder.end());
		}

		Finish(&header);
		return header;
	}

	// the REBSHAPE whose SHPHEAD1 has this number (its payload's int at 20)
	bool ShapeSpan(Bytes const &image, int32_t number, Span *out)
	{
		std::vector<Span> top, inside;

		if (!Children(image, 12, image.size(), &top))
		{
			return false;
		}

		for (Span const &t : top)
		{
			if (!Is(image, t, "REBSHAPE") || !Children(image, t.at + 12, t.at + t.size, &inside))
			{
				continue;
			}

			for (Span const &c : inside)
			{
				if (Is(image, c, "SHPHEAD1") && c.size >= 12 + 24 && IntAt(image, c.at + 12 + 20) == number)
				{
					*out = t;
					return true;
				}
			}
		}

		return false;
	}

	bool PutInShape(OpenLevelFile *file, int32_t number, Bytes const &chunk)
	{
		Bytes image;
		Span shape;

		if (!file->BuildLevel(image) || !ShapeSpan(image, number, &shape))
		{
			return false;
		}

		image.insert(image.begin() + (std::ptrdiff_t)(shape.at + shape.size), chunk.begin(), chunk.end());
		PutIntAt(&image, shape.at + 8, (int32_t)(shape.size + chunk.size()));
		PutIntAt(&image, 8, (int32_t)image.size());
		return file->ReplaceLevel(image) == LevelOpenResult::Opened;
	}

	struct Bone
	{
		std::string name;
		std::vector<std::array<int32_t, 5>> records;
		int32_t after[2];
	};

	struct Attachment
	{
		int headers = 0, holders = 0;
		int32_t number = 0, sub = 0, points = -1;
		std::string hierarchy;
		std::vector<Bone> bones;
	};

	bool ReadAttachment(OpenLevelFile *file, int32_t number, Attachment *out)
	{
		Bytes image;
		Span shape;
		std::vector<Span> inside, parts, data;

		*out = Attachment();

		if (!file->BuildLevel(image) || !ShapeSpan(image, number, &shape) || !Children(image, shape.at + 12, shape.at + shape.size, &inside))
		{
			return false;
		}

		for (Span const &c : inside)
		{
			if (!Is(image, c, "SHPBONHD"))
			{
				continue;
			}

			out->headers++;

			if (!Children(image, c.at + 12, c.at + c.size, &parts))
			{
				return false;
			}

			for (Span const &p : parts)
			{
				if (Is(image, p, "SHPBONIN") && p.size >= 12 + 13)
				{
					out->number = IntAt(image, p.at + 12);
					out->sub = IntAt(image, p.at + 16);
					out->points = IntAt(image, p.at + 20);
					out->hierarchy = std::string(image.data() + p.at + 24, strnlen(image.data() + p.at + 24, p.size - 24));
				}
				else if (Is(image, p, "SHPBONE"))
				{
					out->holders++;

					if (!Children(image, p.at + 12, p.at + p.size, &data))
					{
						return false;
					}

					for (Span const &d : data)
					{
						if (!Is(image, d, "SHPBONDT"))
						{
							continue;
						}

						Bone bone;
						size_t const nameLength = strnlen(image.data() + d.at + 12, d.size - 12);
						size_t at = d.at + 12 + ((nameLength + 4) & ~(size_t)3);

						bone.name.assign(image.data() + d.at + 12, nameLength);

						if (at + 4 > d.at + d.size)
						{
							return false;
						}

						int32_t const count = IntAt(image, at);

						at += 4;

						if (count < 0 || at + (size_t)count * 20 + 8 > d.at + d.size)
						{
							return false;
						}

						for (int32_t k = 0; k < count; k++, at += 20)
						{
							bone.records.push_back({ IntAt(image, at), IntAt(image, at + 4), IntAt(image, at + 8), IntAt(image, at + 12), IntAt(image, at + 16) });
						}

						bone.after[0] = IntAt(image, at);
						bone.after[1] = IntAt(image, at + 4);
						out->bones.push_back(bone);
					}
				}
			}
		}

		return true;
	}

	// every SHPVTINT of the objects whose header names this shape (OBJHEAD1's payload, int at 56): the set's name and its numbers
	bool ReadTints(OpenLevelFile *file, int32_t number, std::vector<std::pair<std::string, std::vector<int32_t>>> *out)
	{
		Bytes image;
		std::vector<Span> top, inside;

		out->clear();

		if (!file->BuildLevel(image) || !Children(image, 12, image.size(), &top))
		{
			return false;
		}

		for (Span const &t : top)
		{
			if (!Is(image, t, "RBOBJECT") || !Children(image, t.at + 12, t.at + t.size, &inside))
			{
				continue;
			}

			bool mine = false;

			for (Span const &c : inside)
			{
				mine = mine || (Is(image, c, "OBJHEAD1") && c.size >= 12 + 60 && IntAt(image, c.at + 12 + 56) == number);
			}

			for (Span const &c : inside)
			{
				if (!mine || !Is(image, c, "SHPVTINT") || c.size < 12 + 16)
				{
					continue;
				}

				int32_t const count = IntAt(image, c.at + 12 + 12);

				if (count < 0 || 12 + 16 + (size_t)count * 4 > c.size)
				{
					return false;
				}

				std::pair<std::string, std::vector<int32_t>> one(std::string(image.data() + c.at + 12, strnlen(image.data() + c.at + 12, 8)), std::vector<int32_t>());

				for (int32_t k = 0; k < count; k++)
				{
					one.second.push_back(IntAt(image, c.at + 12 + 16 + (size_t)k * 4));
				}

				out->push_back(one);
			}
		}

		return true;
	}

	// what the attachment must be after a removal whose point map is `map` (-1: the point is gone), from the one put in: bone A's records of the points kept, renumbered, in the old order; bone B's the
	// same, and no SHPBONE at all for B when none of its points is kept
	void CheckAttachment(Attachment const &got, std::vector<int32_t> const &map, std::vector<int32_t> const &freed, std::string const &label)
	{
		int32_t kept = 0;
		std::vector<std::array<int32_t, 5>> wantA, wantB;

		for (size_t q = 0; q < map.size(); q++)
		{
			if (map[q] >= 0)
			{
				kept++;
				wantA.push_back(Record(map[q], (int32_t)q));
			}
		}

		for (int32_t const q : freed)
		{
			if (map[(size_t)q] >= 0)
			{
				wantB.push_back(Record(map[(size_t)q], q));
			}
		}

		size_t const wantBones = wantB.empty() ? 1 : 2;

		Check(got.headers == 1 && got.number == 7 && got.sub == 3 && got.hierarchy == "testhier", label + ": the shape still has its one bone attachment, its hierarchy named as it was (" + std::to_string(got.headers) + " SHPBONHD, " +
			got.hierarchy + " " + std::to_string(got.number) + " " + std::to_string(got.sub) + ")");
		Check(got.points == kept, label + ": SHPBONIN's point count is the shape's new one (" + std::to_string(got.points) + ", the shape has " + std::to_string(kept) + "; FUN_00435a00 0x435aaf)");
		Check(got.holders == (int)wantBones && got.bones.size() == wantBones, label + ": " + std::to_string(wantBones) + " bone(s) left (" + std::to_string(got.holders) + " SHPBONE; a bone left with no point goes, 0x435bb0)");

		if (got.bones.size() == wantBones)
		{
			Check(got.bones[0].name == "boneA" && got.bones[0].records == wantA && got.bones[0].after[0] == 11 && got.bones[0].after[1] == 12, label + ": bone A holds the " + std::to_string(wantA.size()) +
				" points kept, renumbered, each record's other numbers moved with it, the two numbers after them as they were (" + std::to_string(got.bones[0].records.size()) + " records)");

			if (wantBones == 2)
			{
				Check(got.bones[1].name == "boneB" && got.bones[1].records == wantB && got.bones[1].after[0] == 11 && got.bones[1].after[1] == 12, label + ": bone B holds its " + std::to_string(wantB.size()) +
					" points, renumbered (" + std::to_string(got.bones[1].records.size()) + " records)");
			}
		}
	}

	void CheckTints(std::vector<std::pair<std::string, std::vector<int32_t>>> const &before, std::vector<std::pair<std::string, std::vector<int32_t>>> const &after, std::vector<int32_t> const &map,
		std::string const &label)
	{
		int32_t kept = 0;

		for (int32_t const m : map)
		{
			kept += m >= 0 ? 1 : 0;
		}

		bool same = before.size() == after.size() && !after.empty();

		for (size_t s = 0; same && s < after.size(); s++)
		{
			std::vector<int32_t> want((size_t)kept, -1);

			for (size_t q = 0; q < before[s].second.size() && q < map.size(); q++)
			{
				if (map[q] >= 0)
				{
					want[(size_t)map[q]] = before[s].second[q];
				}
			}

			same = after[s].first == before[s].first && after[s].second == want;
		}

		Check(same, label + ": each of the object's " + std::to_string(before.size()) + " vertex colour lists (SHPVTINT) has one number per point left (" + std::to_string(kept) + "), each kept point's old one (" +
			(after.empty() ? std::string("none") : std::to_string(after[0].second.size())) + " numbers in the first; FUN_00435c80)");
	}
}

static void BonesAndTintsOnLevel(std::string const &levelPath, std::string const &copyFolder)
{
	std::string const level = BaseName(levelPath);

	printf("BONES AND VERTEX COLOURS %s\n", level.c_str());

	OpenLevelFile baseline;

	if (baseline.Open(levelPath.c_str()) != LevelOpenResult::Opened || !baseline.Level())
	{
		Check(false, "the game's loader takes the level");
		return;
	}

	// a shape that nothing refuses, used by one object that has vertex colours, with a polygon whose taking out frees a point (its corners not all shared)
	uint8_t id[8] = { 0 };
	bool have = false;
	model::ShapeModel original;
	int32_t take = -1;
	std::vector<int32_t> freed;
	std::vector<std::pair<std::string, std::vector<int32_t>>> tints;
	List<Object_Chunk *> all;

	baseline.Level()->list_objects(&all);

	for (LIF<Object_Chunk *> each(&all); !each.done() && !have; each.next())
	{
		Object_Chunk *const object = each();

		if (!object || !object->get_assoc_shape() || HasChunk(object, "SHPMORPH") || HasChunk(object, "ANIMSEQU") || HasChunk(object, "SHPPCINF") || HasChunk(object, "SHPFRAGS") || HasChunk(object, "SHPFRGTP") || HasChunk(object, "SHPEXTFL") ||
			HasChunk(object, "SHPBONHD"))
		{
			continue;
		}

		model::ShapeModel shape;
		std::string why;
		size_t users = 0;
		uint8_t const *const thisId = (uint8_t const *)&object->object_data.ID;

		if (host::ShapeEdit_Read(baseline.Level(), &baseline, thisId, &shape, &users, &why) != host::ShapeEditResult::Done || users != 1 || shape.polygons.size() < 8 || shape.polygons.size() > 6000 ||
			!boneTest::ReadTints(&baseline, shape.number, &tints) || tints.empty() || tints[0].second.size() != shape.points.size())
		{
			continue;
		}

		std::vector<int> uses(shape.points.size(), 0);

		for (model::ShapePolygon const &p : shape.polygons)
		{
			for (int c = 0; c < p.cornerCount; c++)
			{
				uses[(size_t)p.corners[c]]++;
			}
		}

		for (size_t i = 0; i < shape.polygons.size() && !have; i++)
		{
			std::vector<int32_t> alone;

			for (int c = 0; c < shape.polygons[i].cornerCount; c++)
			{
				if (uses[(size_t)shape.polygons[i].corners[c]] == 1)
				{
					alone.push_back(shape.polygons[i].corners[c]);
				}
			}

			if (!alone.empty())
			{
				have = true;
				take = (int32_t)i;
				freed = alone;
				original = shape;
				memcpy(id, thisId, 8);
			}
		}
	}

	if (!have)
	{
		printf("  --  %s: no shape with vertex colours and a polygon that frees a point\n", level.c_str());
		return;
	}

	std::vector<int32_t> every;

	for (size_t q = 0; q < original.points.size(); q++)
	{
		every.push_back((int32_t)q);
	}

	boneTest::Bytes const attachment = boneTest::BoneHeader((int32_t)original.points.size(), { { "boneA", every }, { "boneB", freed } });

	// REMOVE: the map is the one PolyEdit_RemoveInModel gives for the same polygon
	{
		std::string const label = level + " Remove (bones, colours)";
		OpenLevelFile work;
		host::PolyRemoveMap map;
		std::string why;

		Check(work.Open(levelPath.c_str()) == LevelOpenResult::Opened && boneTest::PutInShape(&work, original.number, attachment), label + ": a bone attachment is put in the shape and the game's loader takes it");
		Check(host::PolyEdit_Remove(&work, id, &take, 1, &map, &why) == host::PolyRemoveResult::Done && map.point.size() == original.points.size(), label + ": polygon " + std::to_string(take) + " is taken out, freeing " +
			std::to_string(freed.size()) + " point(s) " + why);

		std::string const copyPath = CopyPathIn(copyFolder, (level + "-poly-removed-bones.rif").c_str());
		OpenLevelFile edited;
		boneTest::Attachment got;
		std::vector<std::pair<std::string, std::vector<int32_t>>> tintsAfter;

		DeleteFileA(copyPath.c_str());
		Check(work.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved && edited.Open(copyPath.c_str()) == LevelOpenResult::Opened, label + ": the level is saved as a copy and the game's loader takes it");
		Check(boneTest::ReadAttachment(&edited, original.number, &got), label + ": the copy's bone attachment is read");
		boneTest::CheckAttachment(got, map.point, freed, label);
		Check(boneTest::ReadTints(&edited, original.number, &tintsAfter), label + ": the copy's vertex colours are read");
		boneTest::CheckTints(tints, tintsAfter, map.point, label);
	}

	// SPLIT: the old shape as Remove; the NEW shape keeps the attachment too, through its own map (its points are those polygon `take` uses), and the new object has no vertex colours (FUN_00564300 names
	// no SHPVTINT, and its copy of the shape has no object yet when FUN_00435c80 runs)
	{
		std::string const label = level + " Split (bones, colours)";
		OpenLevelFile work;
		uint8_t madeId[8] = { 0 };
		std::string made, why;
		std::vector<int32_t> rest;
		model::ShapeModel fresh = original, cut = original;
		host::PolyRemoveMap freshMap, cutMap;

		for (size_t i = 0; i < original.polygons.size(); i++)
		{
			if ((int32_t)i != take)
			{
				rest.push_back((int32_t)i);
			}
		}

		Check(host::PolyEdit_RemoveInModel(&fresh, nullptr, rest.data(), rest.size(), &freshMap) == host::PolyRemoveResult::Done && host::PolyEdit_RemoveInModel(&cut, nullptr, &take, 1, &cutMap) == host::PolyRemoveResult::Done,
			label + ": the maps of the two shapes are worked out");
		Check(work.Open(levelPath.c_str()) == LevelOpenResult::Opened && boneTest::PutInShape(&work, original.number, attachment), label + ": a bone attachment is put in the shape and the game's loader takes it");
		Check(host::PolyEdit_SplitAsObject(&work, id, &take, 1, madeId, &made, &why) == host::PolySplitResult::Done, label + ": polygon " + std::to_string(take) + " is split off as " + made + " " + why);

		std::string const copyPath = CopyPathIn(copyFolder, (level + "-poly-split-bones.rif").c_str());
		OpenLevelFile edited;

		DeleteFileA(copyPath.c_str());
		Check(work.SaveAs(copyPath.c_str()) == LevelSaveResult::Saved && edited.Open(copyPath.c_str()) == LevelOpenResult::Opened, label + ": the level is saved as a copy and the game's loader takes it");

		Object_Chunk *const newObject = FindObject(edited, madeId);
		int32_t const newNumber = ShapeNumberOf(edited, newObject);
		boneTest::Attachment gotOld, gotNew;
		std::vector<std::pair<std::string, std::vector<int32_t>>> tintsOld, tintsNew;

		Check(newObject && newNumber >= 0 && newNumber != original.number, label + ": the new object and its shape are found (shape " + std::to_string(newNumber) + ")");
		Check(boneTest::ReadAttachment(&edited, original.number, &gotOld), label + " (old shape): its bone attachment is read");
		boneTest::CheckAttachment(gotOld, cutMap.point, freed, label + " (old shape)");
		Check(boneTest::ReadTints(&edited, original.number, &tintsOld), label + " (old shape): its vertex colours are read");
		boneTest::CheckTints(tints, tintsOld, cutMap.point, label + " (old shape)");
		Check(boneTest::ReadAttachment(&edited, newNumber, &gotNew), label + " (new shape): its bone attachment is read");
		boneTest::CheckAttachment(gotNew, freshMap.point, freed, label + " (new shape)");
		Check(boneTest::ReadTints(&edited, newNumber, &tintsNew) && tintsNew.empty(), label + " (new shape): the new object has no vertex colours (" + std::to_string(tintsNew.size()) + " lists)");
	}
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("host_poly_edit_test: give stock levels (.rif) and then a folder for the copies; with none there is nothing to run.\n");
		printf("passed host_poly_edit_test (0 ok)\n");
		return 0;
	}

	Numbers();
	RemoveInNumbers();
	MergeInNumbers();
	CheckPairsInNumbers();

	std::string const copyFolder = argv[argc - 1];

	for (int i = 1; i < argc - 1; i++)
	{
		std::vector<char> original;

		if (!ReadWholeFile(argv[i], &original))
		{
			printf("  FAILED  the level can be read: %s\n", argv[i]);
			checksFailed++;
			continue;
		}

		OneLevel(argv[i], copyFolder, original);
		TypeChanges(argv[i], copyFolder);
		RemoveOnLevel(argv[i], copyFolder);
		UnmergeOnLevel(argv[i], copyFolder);
		MergeOnCopyOnLevel(argv[i], copyFolder);
		CheckPairsOnLevel(argv[i], copyFolder);
		SplitOnLevel(argv[i], copyFolder);
		BonesAndTintsOnLevel(argv[i], copyFolder);
	}

	if (checksFailed)
	{
		printf("FAILED host_poly_edit_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_poly_edit_test\n");
	return 0;
}
