// See device_keeper.h. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "device_keeper.h"

namespace host
{
	// A side as the rules take it: below 0 is 0, above kSurfaceMostSide is kSurfaceMostSide
	static int32_t Side(int32_t side)
	{
		return side < 0 ? 0 : side > kSurfaceMostSide ? kSurfaceMostSide : side;
	}

	// A side that has to be drawn: at least 1
	static int32_t DrawnSide(int32_t side)
	{
		int32_t const kept = Side(side);
		return kept < 1 ? 1 : kept;
	}

	// Whether the time has come, on a clock that wraps round: the difference, taken as a number with a sign, says which comes first
	static bool TimeHasCome(uint32_t nowMs, uint32_t atMs)
	{
		return (int32_t)(nowMs - atMs) >= 0;
	}

	DeviceKeeper::DeviceKeeper(DeviceSeam &deviceSeam, int32_t startWidth, int32_t startHeight)
		: seam(deviceSeam), state(SurfaceState::Drawing), width(DrawnSide(startWidth)), height(DrawnSide(startHeight)), panelsWidth(0),
		  panelsHeight(0), hasResources(true), hasPanels(false), waiting(false), tryAt(0), waitMs(kResetFirstWaitMs), resetsTried(0),
		  resetsFailed(0)
	{
	}

	bool DeviceKeeper::BeforeFrame(int32_t viewWidth, int32_t viewHeight, int32_t windowWidth, int32_t windowHeight, uint32_t nowMs)
	{
		if (state == SurfaceState::Broken)
		{
			return false;
		}

		// Minimised: the parent 0 by 0. Nothing drawn, made or reset, the device not asked. What it was waiting for, it still waits for.
		if (Side(windowWidth) == 0 && Side(windowHeight) == 0)
		{
			state = SurfaceState::Minimised;
			return false;
		}

		int32_t const wantedWidth = DrawnSide(viewWidth);
		int32_t const wantedHeight = DrawnSide(viewHeight);
		int32_t const wantedPanelsWidth = DrawnSide(windowWidth);
		int32_t const wantedPanelsHeight = DrawnSide(windowHeight);

		// Not before its time
		if (waiting && !TimeHasCome(nowMs, tryAt))
		{
			state = SurfaceState::Waiting;
			return false;
		}

		switch (seam.Ask())
		{
			case DeviceSays::Fine:
				// A new view size is a reset. Only a new parent size is only the panels made again.
				if (wantedWidth != width || wantedHeight != height)
				{
					return ResetTo(wantedWidth, wantedHeight, wantedPanelsWidth, wantedPanelsHeight, nowMs);
				}

				// Back by itself, or never gone: it only needs what was let go of
				if (!hasResources)
				{
					if (!seam.RemakeResources())
					{
						Failed(nowMs);
						return false;
					}

					hasResources = true;
				}

				if (!hasPanels || wantedPanelsWidth != panelsWidth || wantedPanelsHeight != panelsHeight)
				{
					if (!MakePanels(wantedPanelsWidth, wantedPanelsHeight, nowMs))
					{
						return false;
					}
				}

				return Drawn();

			case DeviceSays::ReadyToReset:
				return ResetTo(wantedWidth, wantedHeight, wantedPanelsWidth, wantedPanelsHeight, nowMs);

			case DeviceSays::Lost:
				LetGoOfEverything();
				TryAgainAfter(nowMs, kLostAskAgainMs);
				return false;

			case DeviceSays::Broken:
			default:
				state = SurfaceState::Broken;
				return false;
		}
	}

	void DeviceKeeper::PanelsWindowChanged()
	{
		if (hasPanels)
		{
			seam.LetGoOfPanels();
			hasPanels = false;
		}
	}

	bool DeviceKeeper::ResetTo(int32_t wantedWidth, int32_t wantedHeight, int32_t wantedPanelsWidth, int32_t wantedPanelsHeight, uint32_t nowMs)
	{
		LetGoOfEverything();
		resetsTried++;

		if (!seam.Reset(wantedWidth, wantedHeight))
		{
			resetsFailed++;
			Failed(nowMs);
			return false;
		}

		width = wantedWidth;
		height = wantedHeight;

		// Reset, but its resources couldn't be made: the next try asks again, finds it fine, and makes them
		if (!seam.RemakeResources())
		{
			Failed(nowMs);
			return false;
		}

		hasResources = true;

		// The panels last, in the same way
		if (!MakePanels(wantedPanelsWidth, wantedPanelsHeight, nowMs))
		{
			return false;
		}

		return Drawn();
	}

	void DeviceKeeper::LetGoOfEverything()
	{
		if (hasPanels)
		{
			seam.LetGoOfPanels();
			hasPanels = false;
		}

		if (hasResources)
		{
			seam.LetGoOfResources();
			hasResources = false;
		}
	}

	// The panels made at the parent's size, after letting go of any it has at another size. On failure it waits as for a failed reset.
	bool DeviceKeeper::MakePanels(int32_t wantedPanelsWidth, int32_t wantedPanelsHeight, uint32_t nowMs)
	{
		if (hasPanels)
		{
			seam.LetGoOfPanels();
			hasPanels = false;
		}

		if (!seam.MakePanels(wantedPanelsWidth, wantedPanelsHeight))
		{
			Failed(nowMs);
			return false;
		}

		hasPanels = true;
		panelsWidth = wantedPanelsWidth;
		panelsHeight = wantedPanelsHeight;
		return true;
	}

	void DeviceKeeper::TryAgainAfter(uint32_t nowMs, uint32_t waitForMs)
	{
		waiting = true;
		tryAt = nowMs + waitForMs;
		state = SurfaceState::Waiting;
	}

	void DeviceKeeper::Failed(uint32_t nowMs)
	{
		TryAgainAfter(nowMs, waitMs);
		waitMs = waitMs >= kResetLongestWaitMs / 2 ? kResetLongestWaitMs : waitMs * 2;
	}

	bool DeviceKeeper::Drawn()
	{
		waiting = false;
		waitMs = kResetFirstWaitMs;
		state = SurfaceState::Drawing;
		return true;
	}
}
