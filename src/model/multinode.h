// multinode.h: the Texture Map's MULTIPLE NODE MODE (sub-mode 3), its pure maths: the selected polygons' corners joined into nodes (FUN_00573f20), the nodes linked (FUN_00560210),
// lined up on the picture (FUN_00574310) and written back to the corners (FUN_00560450). The host gathers the polygons and keeps the nodes (reassess/tm-b-multinode-map.md).
// Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng (TM-B). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stdint.h>

#include <vector>

#include "texture_quad.h"

namespace model
{
	int32_t const kMultiNodeReach = 400;     // a corner joins a node within 400 on each axis (FUN_00573f20 0x57406a-0x5740ec, the ends counted)
	int const kMultiNodeMostPasses = 100;    // the line-up gives up after 100 passes (0x574686)

	// One polygon the mode works on, in the order the host gathers them: every SELECTED polygon (shapes in order, each shape's in its selection order), then each selected
	// one's merge partner that is not selected itself (selected false: its corners are no node's, but the line-up moves it with its partner).
	struct MultiNodePolygon
	{
		bool selected;
		int32_t partner;              // the place of its merge partner in this list, -1 for none
		bool quadKnown;               // quad holds model::TextureQuadOf of it and its partner (a merged pair)
		TextureQuad quad;
		int corners;                  // 3, or 4 for a quad of the file (TM-B, DECIDED: ours keeps the file's quads, which inteng's copy holds as two triangles; all four corners are members)
		int32_t x[4], y[4], z[4];     // its corners where the 400 rule measures them
		int32_t u[4], v[4];           // its texture coordinates, whole texels (the line-up moves them)
	};

	struct MultiNodeMember
	{
		int32_t polygon;   // the place in the polygon list
		int corner;        // 0..2 (0..3 for a quad of the file)
	};

	struct MultiNode
	{
		int32_t x, y, z;                        // the corner that made it (never averaged)
		std::vector<MultiNodeMember> members;
		std::vector<int32_t> links;             // the higher-numbered nodes it is joined to (each link is kept once, in its lower-numbered node)
		int32_t u, v;                           // its place on the picture (the line-up sets it)
	};

	// FUN_00573f20's join: for each selected polygon in the list's order, its corners 0..2: the FIRST node whose x, y and z each lie within reach of the corner's (the ends
	// counted) takes it as a member; with none, the corner makes a new node. (Inteng measures every shape's corners together, in the order of the walk: named, the join is not
	// the same for every order.)
	std::vector<MultiNode> MultiNodeJoin(std::vector<MultiNodePolygon> const &polygons, int32_t reach = kMultiNodeReach);

	// The node a polygon's corner is a member of, -1 for none. INTENG BUG, FIXED: FUN_005603c0 gives 0 (the first node) for a corner no node holds (0x560426), so links and the
	// line-up's "not done" reached node 0 for an unselected partner's corners; here no node is reached.
	int32_t MultiNodeOf(std::vector<MultiNode> const &nodes, int32_t polygon, int corner);

	// FUN_00560210's links, for every member: a polygon with no partner joins its corner's node to the nodes of corners (c + 1) % 3 and (c + 2) % 3 (a quad of the file: (c + 1) % 4 and
	// (c + 3) % 4, its edges); a merged pair (quadKnown)
	// joins it to its two neighbours round the quad only (places (p + 1) & 3 and (p + 3) & 3, no diagonal), a place the polygon hasn't being its partner's corner. A link is kept
	// in the lower-numbered node once. INTENG BUG, FIXED: inteng takes the quad's way only when the single-node window last held four nodes (+0xa0 == 4, 0x560254), for the whole
	// selection; here every merged pair is a quad.
	void MultiNodeLink(std::vector<MultiNode> *nodes, std::vector<MultiNodePolygon> const &polygons);

	// FUN_00574310, the line-up over a picture width x height (both at least 1). Each pass, for each node not done: the largest u and the largest v of its members (a tie
	// replaces); with more than one member, a member whose distance from them is a whole number of pictures give or take one texel each way (du % width is 0, 1 or width - 1,
	// and dv % height the same) is moved by that many pictures, ((du + 1) / width) x width and the same down - all three corners of its polygon and of its partner - and the
	// nodes of the moved corners are not done again; the node's (u, v) is then the largest u and v and it is done. A pass that moves nothing ends it: true. More than 100
	// passes: false. The polygons are changed only on true (INTENG BUG, FIXED: a failed line-up keeps its moves in inteng, 0x5742ad, and the mode is left with the
	// coordinates moved).
	bool MultiNodeLineUp(std::vector<MultiNode> *nodes, std::vector<MultiNodePolygon> *polygons, int32_t width, int32_t height);

	// FUN_00560450 for every node: each member corner takes its node's (u, v). Gives the places of the polygons whose coordinates changed.
	std::vector<int32_t> MultiNodeWriteBack(std::vector<MultiNode> const &nodes, std::vector<MultiNodePolygon> *polygons);
}
