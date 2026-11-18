// host_uv_nodes_test.cpp: the test of src/model/uv_nodes.cpp, the Texture Map's cycle (O) and reverse (V) of a polygon's nodes as inteng's FUN_00571310 / FUN_00571910 do them (TM-C H5).
// Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// CHECKS, every answer worked by hand from the decompile (uv_nodes.h): a triangle alone (cycle A B C -> B C A, reverse -> A C B: node 0 stays); a file quad (DECIDED: cycle -> B C D A, reverse
// -> D C B A); a merged pair as one quad, with the shared corners holding DIFFERENT values in the two triangles, so each read's side shows (the cycle reads own-first; the reverse's far corner
// takes the partner's); the same on animation frames' six numbers; the counts that are not 3 or 4, and a quad no corner of which the two hold, left as they were.

#include <stdio.h>
#include <string.h>

#include <string>

#include "uv_nodes.h"

using namespace model;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static UvCorner N(float u)
{
	return { u, u + 0.5f };   // a node named by its u; its v says the same
}

static std::string Said(UvCorner const *nodes, int count)
{
	std::string out;

	for (int k = 0; k < count; k++)
	{
		out += (k ? " " : "") + std::to_string((int)nodes[k].u);
	}

	return out;
}

static bool Is(UvCorner const *nodes, int count, std::initializer_list<int> want)
{
	int k = 0;

	for (int u : want)
	{
		if (k >= count || nodes[k].u != (float)u || nodes[k].v != (float)u + 0.5f)
		{
			return false;
		}

		k++;
	}

	return k == count;
}

int main()
{
	// a triangle alone: A=1 B=2 C=3
	UvCorner t[3] = { N(1), N(2), N(3) };

	UvNodes_Alone(t, 3, NodeTurn::Cycle);
	Check(Is(t, 3, { 2, 3, 1 }), "a triangle cycled: A B C -> B C A (" + Said(t, 3) + ")");

	UvCorner r[3] = { N(1), N(2), N(3) };

	UvNodes_Alone(r, 3, NodeTurn::Reverse);
	Check(Is(r, 3, { 1, 3, 2 }), "a triangle reversed: A B C -> A C B, node 0 stays (" + Said(r, 3) + ")");

	// a file quad (DECIDED UV-NODES-FILE-QUAD)
	UvCorner q[4] = { N(1), N(2), N(3), N(4) };

	UvNodes_Alone(q, 4, NodeTurn::Cycle);
	Check(Is(q, 4, { 2, 3, 4, 1 }), "a file quad cycled: A B C D -> B C D A (" + Said(q, 4) + ")");

	UvCorner qr[4] = { N(1), N(2), N(3), N(4) };

	UvNodes_Alone(qr, 4, NodeTurn::Reverse);
	Check(Is(qr, 4, { 4, 3, 2, 1 }), "a file quad reversed: A B C D -> D C B A (" + Said(qr, 4) + ")");

	// other counts left alone
	UvCorner two[2] = { N(1), N(2) }, five[5] = { N(1), N(2), N(3), N(4), N(5) };

	UvNodes_Alone(two, 2, NodeTurn::Cycle);
	UvNodes_Alone(five, 5, NodeTurn::Reverse);
	Check(Is(two, 2, { 1, 2 }) && Is(five, 5, { 1, 2, 3, 4, 5 }), "2 or 5 nodes are left as they were");

	// a merged pair: corner 0 the polygon's alone (its node 0), corner 1 shared (own 1, partner 2), corner 2 the partner's alone (its node 0), corner 3 shared (own 2, partner 1).
	// own: a=10 at corner 0, b=20 at corner 1, d=40 at corner 3; partner: b'=21 at corner 1, c=30 at corner 2, d'=41 at corner 3
	TextureQuad quad = {};
	int32_t const ownPlaces[4] = { 0, 1, -1, 2 }, partnerPlaces[4] = { -1, 2, 0, 1 };

	memcpy(quad.ownCorners, ownPlaces, sizeof(ownPlaces));
	memcpy(quad.partnerCorners, partnerPlaces, sizeof(partnerPlaces));

	{
		UvCorner own[3] = { N(10), N(20), N(40) }, partner[3] = { N(30), N(41), N(21) };

		// cycle: corner 0 <- corner 1 own-first (20); corner 1 <- corner 2 (30), both; corner 2 <- corner 3 own-first (40); corner 3 <- the first (10), both
		UvNodes_Quad(quad, own, partner, NodeTurn::Cycle);
		Check(Is(own, 3, { 20, 30, 10 }) && Is(partner, 3, { 40, 10, 30 }), "a pair cycled round its four corners, each read own-first: own " + Said(own, 3) + ", partner " + Said(partner, 3) +
			" (want 20 30 10, 40 10 30)");
	}

	{
		UvCorner own[3] = { N(10), N(20), N(40) }, partner[3] = { N(30), N(41), N(21) };

		// reverse: swap 0 and 3: near 10, far the partner's 41 -> corner 3 = 10 both, corner 0 = 41; swap 1 and 2: near own's 20, far 30 -> corner 2 = 20, corner 1 = 30 both
		UvNodes_Quad(quad, own, partner, NodeTurn::Reverse);
		Check(Is(own, 3, { 41, 30, 10 }) && Is(partner, 3, { 20, 10, 30 }), "a pair reversed: 0 <-> 3 and 1 <-> 2, the far corner the partner's: own " + Said(own, 3) + ", partner " +
			Said(partner, 3) + " (want 41 30 10, 20 10 30)");
	}

	// a quad with a corner neither holds: nothing
	{
		TextureQuad broken = quad;
		UvCorner own[3] = { N(10), N(20), N(40) }, partner[3] = { N(30), N(41), N(21) };

		broken.ownCorners[2] = -1;
		broken.partnerCorners[2] = -1;
		UvNodes_Quad(broken, own, partner, NodeTurn::Cycle);
		Check(Is(own, 3, { 10, 20, 40 }) && Is(partner, 3, { 30, 41, 21 }), "a quad with a corner neither triangle holds is left as it was");
	}

	// frames: six numbers
	{
		int32_t six[6] = { 1, 101, 2, 102, 3, 103 };

		UvNodes_FrameAlone(six, NodeTurn::Cycle);

		int32_t const cycled[6] = { 2, 102, 3, 103, 1, 101 };

		Check(memcmp(six, cycled, sizeof(six)) == 0, "a frame cycled: its three (u, v) as the triangle's");

		UvNodes_FrameAlone(six, NodeTurn::Reverse);

		int32_t const reversed[6] = { 2, 102, 1, 101, 3, 103 };

		Check(memcmp(six, reversed, sizeof(six)) == 0, "a frame reversed: nodes 1 and 2 swap");
	}

	{
		int32_t own[6] = { 10, 0, 20, 0, 40, 0 }, partner[6] = { 30, 0, 41, 0, 21, 0 };

		UvNodes_FrameQuad(quad, own, partner, NodeTurn::Cycle);

		int32_t const wantOwn[6] = { 20, 0, 30, 0, 10, 0 }, wantPartner[6] = { 40, 0, 10, 0, 30, 0 };

		Check(memcmp(own, wantOwn, sizeof(own)) == 0 && memcmp(partner, wantPartner, sizeof(partner)) == 0, "a pair's frames cycled together as the pair's nodes");
	}

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
