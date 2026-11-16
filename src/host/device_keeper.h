// Keeping the editor's Direct3D device drawing: when it's lost, when its windows change size, and while it's minimised. Written 2026-11-14
// by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY. A Direct3D 9 device can be "lost": another program takes the screen, the machine locks or sleeps, the display changes. While it's
// lost nothing can be drawn. When it can be got back, it has to be "reset", and before a reset everything it keeps in its default pool has
// to be let go of, then made again after. The same reset is how its back buffer changes size. The game does this in its own frame start
// (R_BeginScene, d3_func.cpp), by waiting there in a loop until the device is back. The editor can't wait like that: its window has to keep
// answering. So the editor asks before each frame, here, and never waits.
//
// TWO SIZES. The editor has two windows on one device: the level's view, a child window, which is the device's own back buffer; and its
// parent, the editor's whole window, whose panels are drawn into a second swap chain of the device's (the panels). The panels are in the
// default pool too.
//
// THE RULES. BeforeFrame is called once a frame, before anything is drawn, with the view's client size, the parent's, and the time. It says
// whether the frame may draw.
// - A side below 0 is taken as 0, and one above kSurfaceMostSide as kSurfaceMostSide.
// - The parent 0 by 0 is minimised: nothing is drawn, nothing is made or reset, and the device isn't even asked. Otherwise each of the
//   parent's sides is at least 1, and so is each of the view's: a view the layout has given no room is drawn 1 by 1, so the panels still are.
// - Each call asks the device at most once (TestCooperativeLevel) and tries at most one reset, so it can never loop on its own.
// - Fine: the frame draws. If the view's size changed, the device is reset at the new size first. If only the parent's size changed, only
//   the panels are made again, at the new size: no reset. Lost: everything in its default pool is let go of, and it's asked again
//   kLostAskAgainMs later. Ready to be reset: it's reset at the sizes the windows have now, which may have changed while it was lost.
//   Anything else: it's broken, as Microsoft's advice is to stop, and it never draws again.
// - A reset is always made with nothing held in the default pool: the game's resources and the panels are let go of first, and made
//   again after, the panels last.
// - A reset that fails, or resources or panels that can't be made, are tried again later, never sooner than a wait that starts at
//   kResetFirstWaitMs and doubles each time it fails, up to kResetLongestWaitMs. The wait starts again from the first once a frame draws.
// - Nothing is let go of unless it's held, and nothing is made while it's held, so neither is done twice.
//
// It talks to the device only through DeviceSeam, so a test can give it a pretend device. The editor gives it the game's own
// (view_surface.cpp).

#pragma once

#include <stdint.h>

namespace host
{
	// The largest side of either window, the same as the panel layout's largest window side
	int32_t const kSurfaceMostSide = 16384;

	// How long a lost device is left before it's asked again
	uint32_t const kLostAskAgainMs = 50;

	// The wait after a reset that failed, at first and at the longest: it doubles each time one fails
	uint32_t const kResetFirstWaitMs = 50;
	uint32_t const kResetLongestWaitMs = 2000;

	// What the device says when asked whether it can draw (TestCooperativeLevel)
	enum class DeviceSays
	{
		Fine,        // D3D_OK: it can draw
		Lost,        // D3DERR_DEVICELOST: it can't draw, and can't be reset yet
		ReadyToReset,// D3DERR_DEVICENOTRESET: it can be reset now
		Broken,      // anything else, such as D3DERR_DRIVERINTERNALERROR
	};

	// What the keeper asks of the device
	class DeviceSeam
	{
	public:
		virtual ~DeviceSeam() {}

		// Whether it can draw (TestCooperativeLevel)
		virtual DeviceSays Ask() = 0;

		// Lets go of the game's resources in its default pool, which a reset needs gone (ReleaseVolatileResources)
		virtual void LetGoOfResources() = 0;

		// Resets it with a back buffer of width by height: the view's. Gives back whether that worked.
		virtual bool Reset(int32_t width, int32_t height) = 0;

		// Makes the game's resources again, after a reset or after the device came back by itself (CreateVolatileResources). Gives back
		// whether that worked.
		virtual bool RemakeResources() = 0;

		// Lets go of the panels' swap chain, which a reset needs gone too
		virtual void LetGoOfPanels() = 0;

		// Makes the panels' swap chain, width by height: the parent's. Gives back whether that worked.
		virtual bool MakePanels(int32_t width, int32_t height) = 0;
	};

	enum class SurfaceState
	{
		Drawing,     // the device is fine, at the windows' sizes, with its resources and its panels
		Waiting,     // lost, or a reset, the resources or the panels failed: it's tried again later
		Minimised,   // the parent is 0 by 0: nothing is drawn, made or reset
		Broken,      // the device can't go on: nothing is drawn again
	};

	class DeviceKeeper
	{
	public:
		// For a device just made, with the game's resources and no panels yet, and a back buffer of width by height (taken as any view's
		// size is)
		DeviceKeeper(DeviceSeam &seam, int32_t width, int32_t height);

		// Once a frame, before anything is drawn: the view's client size, the parent's, and the time in milliseconds from any clock that only
		// goes up (one that wraps round after 49 days is fine). Gives back whether this frame may draw.
		bool BeforeFrame(int32_t viewWidth, int32_t viewHeight, int32_t windowWidth, int32_t windowHeight, uint32_t nowMs);

		// The panels' window has changed (or has just been given): the panels are let go of, if held, and made again on the next frame that
		// can draw
		void PanelsWindowChanged();

		SurfaceState State() const { return state; }

		// The back buffer's size now, and the panels'
		int32_t Width() const { return width; }
		int32_t Height() const { return height; }
		int32_t PanelsWidth() const { return panelsWidth; }
		int32_t PanelsHeight() const { return panelsHeight; }
		bool HasPanels() const { return hasPanels; }

		// How many resets were tried, and how many of them failed, since it was made
		uint32_t ResetsTried() const { return resetsTried; }
		uint32_t ResetsFailed() const { return resetsFailed; }

		// The wait the next failure would start
		uint32_t NextWaitMs() const { return waitMs; }

	private:
		bool ResetTo(int32_t viewWidth, int32_t viewHeight, int32_t windowWidth, int32_t windowHeight, uint32_t nowMs);
		void LetGoOfEverything();
		bool MakePanels(int32_t windowWidth, int32_t windowHeight, uint32_t nowMs);
		void TryAgainAfter(uint32_t nowMs, uint32_t waitForMs);
		void Failed(uint32_t nowMs);
		bool Drawn();

		DeviceSeam &seam;
		SurfaceState state;
		int32_t width;
		int32_t height;
		int32_t panelsWidth;
		int32_t panelsHeight;
		bool hasResources;
		bool hasPanels;
		bool waiting;
		uint32_t tryAt;
		uint32_t waitMs;
		uint32_t resetsTried;
		uint32_t resetsFailed;
	};
}
