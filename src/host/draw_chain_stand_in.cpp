// draw_chain_stand_in.cpp: stands in for AvPx's R_DrawScreenToChain until the editor's avpx pin carries it (see draw_chain.h). Written
// 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// What the real one does: clears the chain's back buffer to background, runs fill(context) to put the screen's text on the game's screen
// list, draws that into the chain at width by height, and presents it, giving back true. What this does instead: nothing at all, and it
// gives back false, the real one's answer when it can't draw. So fill is never run, nothing reaches the card, and the panels stay as
// their window shows them. It's taken out, with draw_chain.h, in the change that moves the pin.

#include "draw_chain.h"

bool R_DrawScreenToChain(IDirect3DSwapChain9 *, uint32_t, uint32_t, uint32_t, void (*)(void *), void *)
{
	return false;
}
