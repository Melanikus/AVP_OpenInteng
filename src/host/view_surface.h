// view_surface.h: what the editor's loop asks the drawing surfaces before each frame (F3's seam with the resize and lost-device work).
// The functions are Frost's to write (view_surface.cpp). Until they land, view_surface_stub.cpp stands in for them. Agreed 2026-11-14 by
// Melanikus (Dietrich and Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE TWO SURFACES. The level's view is a child window over its box in the editor's window, and it's the window the game draws into
// (hWndMain): the game owns its whole back buffer, as it expects to. The editor's own window, the parent, has one more swap chain over its
// whole client area, for the panels, which are drawn on it with AvPx's own text and menu drawing (R-INT-6). The parent has
// WS_CLIPCHILDREN, so the panels never draw over the view.

#pragma once

#include <windows.h>
#include <d3d9.h>

// Once, after InitialiseDirect3D: the parent window, which the panels' swap chain presents to. Until it's called there's no panels' chain,
// and BeforeFrame works for the view alone.
void ViewSurface_Start(HWND panelsWindow);

// Once per frame, before anything is drawn: the view child's client size and the parent's. Gives back true when this frame may draw, and
// false to skip drawing it: the device is lost or not back yet, the panels' chain couldn't be made, or the parent is 0 by 0 while it's
// minimised. It does any reset itself.
// - A side below 0 counts as 0. Only a 0 by 0 parent means nothing is drawn, with no reset. A view of no size (the panels' layout gives
//   its room away first) is drawn as 1 by 1, so the panels still draw. Any other side is held to 1 to 16384, as the layout holds it
//   (kPanelMostSide).
// - A new view size resets the device: the back buffer at the view's size, with the game's whole-screen block and viewport, as
//   R_ChangeResolution sets them. A new parent size alone only makes the panels' chain again, at that size, with no reset.
// - The panels' chain lives on the card, so it's let go of before every reset and while the device is lost, and made again after. If it
//   can't be made, this says false and tries again later.
// - True means Direct3D said the device is fine this frame and everything that lives on the card is back, so the game's own
//   R_BeginScene won't sit in its wait. Everything about drawing into the view's box and the panels is the loop's, after this says true.
// - It tries a reset at most once a call, and waits longer after each one that fails, so the loop can just keep calling it.
bool ViewSurface_BeforeFrame(int viewWidth, int viewHeight, int windowWidth, int windowHeight);

// The panels' swap chain, at the parent's size, or null: before ViewSurface_Start, and whenever BeforeFrame last said false
IDirect3DSwapChain9 *ViewSurface_PanelsChain();

// Whether the device can never be drawn with again: TestCooperativeLevel answered something other than fine, lost or not reset yet
// (such as D3DERR_DRIVERINTERNALERROR). From then on BeforeFrame says false for good, as Microsoft's advice is to stop, and the loop says
// so and closes, rather than wait for a device that won't come back.
bool ViewSurface_IsBroken();
