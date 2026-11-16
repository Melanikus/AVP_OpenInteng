// draw_chain.h: AvPx's R_DrawScreenToChain (R-DRAW-CHAIN-1), the call that draws a screen of the game's text into the panels' swap chain.
// Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// UNTIL THE PIN MOVES. The editor's avpx pin (85704f7a) is older than R-DRAW-CHAIN-1 (AvPx's branch r-draw-chain-1, 5f7304e5), so its
// renderer.h doesn't declare R_DrawScreenToChain yet. So it's declared here, word for word as AvPx's renderer.h declares it, and
// draw_chain_stand_in.cpp stands in for it: it draws nothing and gives back false, so the editor runs with no panels' text until then.
// Both are taken out in the change that moves the pin to an avpx_main commit carrying R-DRAW-CHAIN-1 (agreed with Dietrich). That change
// can't forget the stand-in: with AvPx's own definition linked too, the link refuses two.

#pragma once

#include <stdint.h>

struct IDirect3DSwapChain9;
bool R_DrawScreenToChain	(IDirect3DSwapChain9 *chain, uint32_t width, uint32_t height, uint32_t background, void (*fill)(void *context), void *context);
