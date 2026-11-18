// uv_nodes.h: the Texture Map's O / Shift+O (CYCLE NODES, 0xbf / 0xc0, inteng FUN_00571310) and V / Shift+V (REVERSE NODES, 0xc1 / 0xc2, FUN_00571910) on a polygon's texture coordinates
// (its nodes), and on its texture animation's frames with Shift (TM-C H5; texture-map-closeout-dietrich.md part 2). The pure part, no level: the screen's own is src/host/texture_nodes.h.
// Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// INTENG (dump-all-4.txt 30092-30700), on each selected polygon that is textured (FUN_0042bce0 of its picture):
// - ALONE (no partner in the pairing, DAT_00914d3c -1): its three nodes. Cycle: node k takes node k + 1's (u, v), the last takes the first's (0x5713cc-0x571417). Reverse: nodes 1 and 2 swap,
//   node 0 stays (0x5719e0-0x571a25).
// - A MERGED PAIR (the polygon and its partner as one quad of four corners, FUN_0055cdf0: model::TextureQuadOf), done once, when the polygon's number is not above its partner's and the
//   partner is textured (0x5714ee / 0x571af6). A corner's (u, v) is read from the polygon where it has that corner, else from the partner, and written to both where each has it. Cycle: corner
//   k takes corner k + 1's, corner 3 the first's (0x571518-0x571658). Reverse: corners 0 and 3 swap, then 1 and 2 (0x571b24-...); a swap reads the far corner from the polygon, then from the
//   partner (the partner's wins where both have it), and the near one own-first.
// - With Shift (param 1): the same on every frame of the polygon's texture animation (a frame: six numbers, u0 v0 u1 v1 u2 v2, as TEXTANIM holds them), and for a pair on both polygons'
//   frames together when both animate in the same group (0x571666-...).
// DECIDED UV-NODES-FILE-QUAD (Drake 11-18): a polygon of the FILE with four corners of its own (inteng holds none: its quads are two triangles and their pairing) is turned as inteng turns a
// merged quad's four corners, in its own order: Cycle k <- k + 1, 3 <- 0; Reverse 0 <-> 3, 1 <-> 2.

#pragma once

#include <stdint.h>

#include "texture_quad.h"
#include "uv_model.h"

namespace model
{
	enum class NodeTurn
	{
		Cycle,     // O, FUN_00571310
		Reverse,   // V, FUN_00571910
	};

	// One polygon alone: its nodes (count 3, or 4 for a file quad: DECIDED UV-NODES-FILE-QUAD); any other count is left as it is
	void UvNodes_Alone(UvCorner *nodes, int count, NodeTurn turn);

	// A merged pair as one quad (quad from TextureQuadOf): own and partner are the two triangles' three nodes each
	void UvNodes_Quad(TextureQuad const &quad, UvCorner *own, UvCorner *partner, NodeTurn turn);

	// The same on animation frames: six numbers each (u0 v0 u1 v1 u2 v2)
	void UvNodes_FrameAlone(int32_t *six, NodeTurn turn);
	void UvNodes_FrameQuad(TextureQuad const &quad, int32_t *ownSix, int32_t *partnerSix, NodeTurn turn);
}
