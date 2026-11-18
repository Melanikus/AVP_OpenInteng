// multinode_test.cpp: the Texture Map's multiple node mode, its maths (src/model/multinode.h; reassess/tm-b-multinode-map.md). Written 2026-11-18 by Melanikus (Dietrich) for
// AVP_OpenInteng (TM-B). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <algorithm>
#include <string>
#include <vector>

#include "multinode.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

// A triangle at the points given (x, y; z 0), selected, no partner, its coordinates u, v at each corner
static model::MultiNodePolygon Triangle(int32_t const xy[6], int32_t const uv[6])
{
	model::MultiNodePolygon polygon = {};

	polygon.selected = true;
	polygon.partner = -1;
	polygon.corners = 3;

	for (int c = 0; c < 3; c++)
	{
		polygon.x[c] = xy[c * 2];
		polygon.y[c] = xy[c * 2 + 1];
		polygon.z[c] = 0;
		polygon.u[c] = uv[c * 2];
		polygon.v[c] = uv[c * 2 + 1];
	}

	return polygon;
}

static bool Links(std::vector<model::MultiNode> const &nodes, size_t node, std::vector<int32_t> want)
{
	std::vector<int32_t> have = nodes[node].links;

	std::sort(have.begin(), have.end());
	std::sort(want.begin(), want.end());

	return have == want;
}

int main()
{
	printf("THE MULTIPLE NODE MODE\n");

	// the square 0,0 / 1000,0 / 1000,1000 / 0,1000 as two triangles: (P0 P1 P2) and (P0 P2 P3)
	int32_t const tri1[6] = { 0, 0, 1000, 0, 1000, 1000 }, tri2[6] = { 0, 0, 1000, 1000, 0, 1000 };
	int32_t const uv1[6] = { 0, 0, 63, 0, 63, 63 }, uv2[6] = { 0, 0, 63, 63, 0, 63 };
	std::vector<model::MultiNodePolygon> pair = { Triangle(tri1, uv1), Triangle(tri2, uv2) };
	std::vector<model::MultiNode> nodes = model::MultiNodeJoin(pair);

	Check(nodes.size() == 4 && nodes[0].members.size() == 2 && nodes[2].members.size() == 2 && nodes[1].members.size() == 1 && nodes[3].members.size() == 1,
		"two triangles sharing an edge: 4 nodes, the shared corners each with 2 members (" + std::to_string(nodes.size()) + ")");

	// the 400 rule, the ends counted
	int32_t const near400[6] = { 400, -400, 5000, 0, 5000, 5000 }, near401[6] = { 401, 0, 5000, 0, 5000, 5000 };
	std::vector<model::MultiNodePolygon> reach = { Triangle(tri1, uv1), Triangle(near400, uv1), Triangle(near401, uv1) };
	std::vector<model::MultiNode> reachNodes = model::MultiNodeJoin(reach);

	Check(model::MultiNodeOf(reachNodes, 1, 0) == 0, "a corner 400 away on two axes joins the first node (the ends counted)");
	Check(model::MultiNodeOf(reachNodes, 2, 0) != 0, "a corner 401 away does not");
	Check(model::MultiNodeOf(reachNodes, 0, 3) == -1 && model::MultiNodeOf(reachNodes, 7, 0) == -1, "INTENG BUG, FIXED: a corner no node holds is -1, not node 0 (FUN_005603c0 0x560426)");

	// links of two unpaired triangles: each triangle's three, the shared diagonal included
	model::MultiNodeLink(&nodes, pair);
	Check(Links(nodes, 0, { 1, 2, 3 }) && Links(nodes, 1, { 2 }) && Links(nodes, 2, { 3 }) && Links(nodes, 3, {}), "two unpaired triangles: their edges, the shared one once, each in its lower node");

	// a merged pair: the quad's four edges, no diagonal
	std::vector<model::MultiNodePolygon> merged = pair;

	merged[0].partner = 1;
	merged[0].quadKnown = true;
	merged[0].quad = { { 1, 2, 3, 0 }, { 1, 2, -1, 0 }, { -1, 1, 2, 0 } };   // P1 (own 1), P2 (own 2, partner 1), P3 (partner 2), P0 (own 0, partner 0)
	merged[1].partner = 0;
	merged[1].quadKnown = true;
	merged[1].quad = { { 3, 2, 1, 0 }, { 2, 1, -1, 0 }, { -1, 2, 1, 0 } };   // P3 (own 2), P2 (own 1, partner 2), P1 (partner 1), P0 (own 0, partner 0)

	std::vector<model::MultiNode> quadNodes = model::MultiNodeJoin(merged);

	model::MultiNodeLink(&quadNodes, merged);
	Check(Links(quadNodes, 0, { 1, 3 }) && Links(quadNodes, 1, { 2 }) && Links(quadNodes, 2, { 3 }) && Links(quadNodes, 3, {}),
		"a merged pair: the quad's four edges and no diagonal (FUN_00560210's quad way, every pair: INTENG BUG FIXED, inteng's way hangs on +0xa0 == 4)");

	// THE LINE-UP over a 64 x 64 picture: the shared corner P0 is (0, 0) in one triangle and (64, 0) in the other: one picture apart, the first moves by one picture
	std::vector<model::MultiNodePolygon> apart = pair;

	apart[1].u[0] = 64;
	apart[1].u[1] = 127;
	apart[1].u[2] = 64;

	std::vector<model::MultiNode> apartNodes = model::MultiNodeJoin(apart);
	bool const lined = model::MultiNodeLineUp(&apartNodes, &apart, 64, 64);

	Check(lined && apart[0].u[0] == 64 && apart[0].u[1] == 127 && apart[0].u[2] == 127 && apart[0].v[2] == 63,
		"one picture apart: the triangle with the smaller u moves a whole picture, all three corners (" + std::to_string(apart[0].u[0]) + " " + std::to_string(apart[0].u[1]) + " " +
		std::to_string(apart[0].u[2]) + ")");
	Check(apartNodes[0].u == 64 && apartNodes[0].v == 0 && apartNodes[2].u == 127 && apartNodes[2].v == 63, "each node is the largest u and v of its members");

	// one texel off a whole picture still counts (du % 64 == 63): 65 moves by 64
	std::vector<model::MultiNodePolygon> offByOne = pair;

	offByOne[1].u[0] = 63;
	offByOne[1].u[1] = 126;
	offByOne[1].u[2] = 63;

	std::vector<model::MultiNode> offNodes = model::MultiNodeJoin(offByOne);

	Check(model::MultiNodeLineUp(&offNodes, &offByOne, 64, 64) && offByOne[0].u[0] == 64, "63 apart (a picture less one texel): moved by a picture, ((63 + 1) / 64) x 64");

	// not near a whole picture: nobody moves, the node takes the largest
	std::vector<model::MultiNodePolygon> between = pair;

	between[1].u[0] = 30;

	std::vector<model::MultiNode> betweenNodes = model::MultiNodeJoin(between);

	Check(model::MultiNodeLineUp(&betweenNodes, &between, 64, 64) && between[0].u[0] == 0 && between[1].u[0] == 30 && betweenNodes[0].u == 30,
		"30 apart: nothing moves, the node is at the larger, 30");

	// a moved polygon takes its merge partner along, selected or not
	std::vector<model::MultiNodePolygon> withPartner = apart;

	withPartner = pair;
	withPartner[1].u[0] = 64;
	withPartner[1].u[1] = 127;
	withPartner[1].u[2] = 64;

	model::MultiNodePolygon partner = Triangle(tri1, uv1);

	partner.selected = false;
	partner.partner = 0;
	withPartner[0].partner = 2;
	withPartner.push_back(partner);

	std::vector<model::MultiNode> partnerNodes = model::MultiNodeJoin(withPartner);

	Check(partnerNodes.size() == 4, "an unselected partner's corners make no node");
	Check(model::MultiNodeLineUp(&partnerNodes, &withPartner, 64, 64) && withPartner[2].u[0] == 64 && withPartner[2].u[1] == 127,
		"the moved triangle's unselected partner moves with it (0x574559)");

	// a line-up that never settles (a 1 x 1 picture: every distance is a whole picture, and each move overshoots) gives up after 100 passes, the coordinates as they were
	std::vector<model::MultiNodePolygon> endless = pair;

	endless[1].u[0] = 1;

	std::vector<model::MultiNodePolygon> const before = endless;
	std::vector<model::MultiNode> endlessNodes = model::MultiNodeJoin(endless);
	bool const settled = model::MultiNodeLineUp(&endlessNodes, &endless, 1, 1);
	bool same = true;

	for (size_t p = 0; p < endless.size(); p++)
	{
		for (int c = 0; c < 3; c++)
		{
			same = same && endless[p].u[c] == before[p].u[c] && endless[p].v[c] == before[p].v[c];
		}
	}

	Check(!settled, "a line-up that never settles fails after 100 passes (0x574686)");
	Check(same, "INTENG BUG, FIXED: a failed line-up leaves every coordinate as it was (inteng keeps its moves, 0x5742ad)");

	// a quad of the file (4 corners of its own): its four edges, no diagonal; the line-up moves all four corners
	model::MultiNodePolygon quad = Triangle(tri1, uv1);

	quad.corners = 4;
	quad.x[3] = 0; quad.y[3] = 1000; quad.z[3] = 0; quad.u[3] = 0; quad.v[3] = 63;

	std::vector<model::MultiNodePolygon> quads = { quad, quad };

	for (int c = 0; c < 4; c++)
	{
		quads[1].u[c] += 64;
	}

	std::vector<model::MultiNode> quadOwn = model::MultiNodeJoin(quads);

	model::MultiNodeLink(&quadOwn, quads);
	Check(quadOwn.size() == 4 && Links(quadOwn, 0, { 1, 3 }) && Links(quadOwn, 1, { 2 }) && Links(quadOwn, 2, { 3 }), "a quad of the file: four nodes, its four edges, no diagonal");
	Check(model::MultiNodeLineUp(&quadOwn, &quads, 64, 64) && quads[0].u[3] == 64 && quads[0].u[1] == 127, "a quad of the file a picture apart: all four of its corners move");

	// the write-back: every member corner takes its node's (u, v)
	std::vector<model::MultiNodePolygon> written = pair;
	std::vector<model::MultiNode> writeNodes = model::MultiNodeJoin(written);

	model::MultiNodeLineUp(&writeNodes, &written, 64, 64);
	writeNodes[0].u = 10;
	writeNodes[0].v = 20;

	std::vector<int32_t> const changed = model::MultiNodeWriteBack(writeNodes, &written);

	Check(written[0].u[0] == 10 && written[0].v[0] == 20 && written[1].u[0] == 10 && written[1].v[0] == 20 && changed.size() == 2,
		"the write-back: both triangles' shared corner takes the node's place (FUN_00560450)");
	Check(model::MultiNodeWriteBack(writeNodes, &written).empty(), "written again: nothing changes");

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed)
	{
		printf("FAILED multinode_test\n");
		return 1;
	}

	printf("passed multinode_test\n");
	return 0;
}
