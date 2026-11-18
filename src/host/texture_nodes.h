// texture_nodes.h: the Texture Map's O / Shift+O (0xbf / 0xc0, CYCLE NODES, inteng FUN_00571310) and V / Shift+V (0xc1 / 0xc2, REVERSE NODES, FUN_00571910) on the selected polygons
// (TM-C H5). The key table already gives the commands (texture_keys.cpp); the polygon screen's TextureMapCommand hands them here in one line. The maths: src/model/uv_nodes.h.
// Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// AS INTENG: the selection's undo step first (FUN_00573570 at 0x57132e / 0x57192e); each selected textured polygon alone turns its nodes, a merged pair turns as one quad once (from its lower
// number: the higher half selected alone does nothing, 0x5714ee); with Shift every frame of their texture animation too (a pair's frames together when both animate in one group).
// NAMED: inteng says nothing; here the status line says how many polygons and frames turned. The coordinates and the animation are two writes of the level (the coordinates first): an
// animation the loader refused would leave the coordinates turned, said on the status line. A pair whose two animations have different counts of sequences or frames turns its coordinates
// but not its frames (inteng indexes the partner's frames by the polygon's own counts, past the end of a shorter one).
// DECIDED UV-NODES-ALONE-PARTNER (Drake 11-18): a pairing TextureQuadOf can't use (Alone: a partner number outside the shape, the polygon itself, a partner not a triangle) turns the polygon
// alone; inteng reads a stack word nothing wrote there (texture_quad.h's INTENG BUG, FIXED).

#pragma once

#include <stdint.h>

void TextureNodes_Command(int32_t command);
