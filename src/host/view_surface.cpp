// See view_surface.h. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// This is the device keeper's seam to the game's own device (d3d, d3_func.cpp). The game resizes its device in R_ChangeResolution
// (avpx/win32/d3_func.cpp:2152 at the pin 85704f7a), but the editor can't call that:
// - it moves the window to its corner and makes it the active one (ChangeWindowsSize, :2199; win_proj.cpp:662), which the editor never does;
// - it ends a frame before the reset and begins one after (:2159 and :2233), which only works in the middle of a frame;
// - when a size won't reset it tries 800 by 600 (:2180-2190), where the editor's size is always its window's.
// So the reset below takes R_ChangeResolution's other steps, in its order, and each says which lines it copies. If those lines change in the
// game, these have to change with them: a source check compares the two.

#include "3dc.h"
#include "d3_func.h"
#include "renderer.h"
#include "vdb.h"

#include <windows.h>

#include "device_keeper.h"
#include "view_surface.h"

// The game's own functions, declared exactly as d3_func.cpp defines them. Two have no header: d3_func.cpp declares them for itself (:114-115,
// defined at :852 and :225). SetTransforms is in renderer.h:135 too (defined at :2953). The linker's names include the types, so a
// declaration that didn't match would fail to link, never link to the wrong thing.
bool CreateVolatileResources();
bool ReleaseVolatileResources();
void SetTransforms();

namespace
{
	// The game's device, as the keeper asks of it
	class GameDevice : public host::DeviceSeam
	{
	public:
		host::DeviceSays Ask() override
		{
			HRESULT const answer = d3d.lpD3DDevice->TestCooperativeLevel();

			if (SUCCEEDED(answer))
			{
				return host::DeviceSays::Fine;
			}

			if (answer == D3DERR_DEVICELOST)
			{
				return host::DeviceSays::Lost;
			}

			if (answer == D3DERR_DEVICENOTRESET)
			{
				return host::DeviceSays::ReadyToReset;
			}

			// D3DERR_DRIVERINTERNALERROR, or anything else: Microsoft's advice is to stop
			return host::DeviceSays::Broken;
		}

		void LetGoOfResources() override
		{
			ReleaseVolatileResources();
		}

		bool Reset(int32_t width, int32_t height) override
		{
			// R_ChangeResolution's reset, at the size asked for (:2163-2167)
			UINT const oldWidth = d3d.d3dpp.BackBufferWidth;
			UINT const oldHeight = d3d.d3dpp.BackBufferHeight;
			d3d.d3dpp.BackBufferWidth = (UINT)width;
			d3d.d3dpp.BackBufferHeight = (UINT)height;

			if (FAILED(d3d.lpD3DDevice->Reset(&d3d.d3dpp)))
			{
				// Not reset: the device keeps its old back buffer, so the size the game knows stays the old one too, and its own frame start
				// (R_BeginScene) would reset it at the size its screen block says
				d3d.d3dpp.BackBufferWidth = oldWidth;
				d3d.d3dpp.BackBufferHeight = oldHeight;
				return false;
			}

			// Only now that the back buffer is the new size: R_ChangeResolution's whole-screen settings at that size, the viewport
			// (:2201-2204), the game's screen block (:2209-2222) and the aspect ratio (:2225)
			d3d.D3DViewport.Width = (DWORD)width;
			d3d.D3DViewport.Height = (DWORD)height;
			d3d.lpD3DDevice->SetViewport(&d3d.D3DViewport);

			ScreenDescriptorBlock.SDB_Width = width;
			ScreenDescriptorBlock.SDB_Height = height;
			ScreenDescriptorBlock.SDB_Size = width * height;
			ScreenDescriptorBlock.SDB_CentreX = width / 2;
			ScreenDescriptorBlock.SDB_CentreY = height / 2;
			ScreenDescriptorBlock.SDB_ProjX = width / 2;
			ScreenDescriptorBlock.SDB_ProjY = height / 2;
			ScreenDescriptorBlock.SDB_ClipLeft = 0;
			ScreenDescriptorBlock.SDB_ClipRight = width;
			ScreenDescriptorBlock.SDB_ClipUp = 0;
			ScreenDescriptorBlock.SDB_ClipDown = height;

			d3d.aspectRatio = (float)width / (float)height;
			return true;
		}

		bool RemakeResources() override
		{
			// R_ChangeResolution's last steps (:2227-2231): the resources made again, the game's transforms set, and its effects told
			// CreateVolatileResources makes every buffer first and can only fail after, setting the texture stages (d3_func.cpp:907-917).
			// So when it fails, what it made is let go of here, the game's own way (each one deleted only if it's there): the keeper then
			// holds no resources and neither does the card, and the next try, or a reset, starts from nothing.
			if (!CreateVolatileResources())
			{
				ReleaseVolatileResources();
				return false;
			}

			SetTransforms();
			d3d.effectSystem->Reset();
			return true;
		}

		void LetGoOfPanels() override
		{
			if (panels)
			{
				panels->Release();
				panels = nullptr;
			}
		}

		bool MakePanels(int32_t width, int32_t height) override
		{
			// Before ViewSurface_Start names the parent there's nowhere to show panels, so there's nothing to make, and that's not a failure
			if (!panelsWindow)
			{
				return true;
			}

			// The device's own settings, at the parent's size, showing in the parent. An extra swap chain has no depth buffer of its own: the
			// panels are flat, drawn after the level, so they need none.
			D3DPRESENT_PARAMETERS settings = d3d.d3dpp;
			settings.BackBufferWidth = (UINT)width;
			settings.BackBufferHeight = (UINT)height;
			settings.hDeviceWindow = panelsWindow;
			settings.Windowed = TRUE;
			settings.FullScreen_RefreshRateInHz = 0;
			settings.EnableAutoDepthStencil = FALSE;

			// A chain still held is let go of first, and a failed make leaves none behind, so no chain is ever left on the card to block a
			// reset. (The keeper never asks for a second while it holds one, but this doesn't rely on that.)
			LetGoOfPanels();

			IDirect3DSwapChain9 *made = nullptr;

			if (FAILED(d3d.lpD3DDevice->CreateAdditionalSwapChain(&settings, &made)) || !made)
			{
				if (made)
				{
					made->Release();
				}

				return false;
			}

			panels = made;
			return true;
		}

		HWND panelsWindow = NULL;
		IDirect3DSwapChain9 *panels = nullptr;
	};

	GameDevice theGameDevice;

	// Whether BeforeFrame last said the frame may draw: the panels' chain is only handed out then
	bool lastFrameDraws = false;

	// Made on the first call, at the back buffer's size then: the game made the device, with its resources, before the loop began
	host::DeviceKeeper *theKeeper = nullptr;
}

// The keeper, made the first time it's needed, at the back buffer's size then: the game made the device, with its resources, before the
// loop began
static host::DeviceKeeper &TheKeeper()
{
	if (!theKeeper)
	{
		static host::DeviceKeeper keeper(theGameDevice, (int32_t)d3d.d3dpp.BackBufferWidth, (int32_t)d3d.d3dpp.BackBufferHeight);
		theKeeper = &keeper;
	}

	return *theKeeper;
}

void ViewSurface_Start(HWND panelsWindow)
{
	if (!d3d.lpD3DDevice)
	{
		return;
	}

	// Any panels made for another window go, and the next frame that can draw makes them for this one
	theGameDevice.panelsWindow = panelsWindow;
	TheKeeper().PanelsWindowChanged();
}

bool ViewSurface_BeforeFrame(int viewWidth, int viewHeight, int windowWidth, int windowHeight)
{
	lastFrameDraws = false;

	if (!d3d.lpD3DDevice)
	{
		return false;
	}

	// GetTickCount's milliseconds wrap round after 49 days, which the keeper allows for
	lastFrameDraws = TheKeeper().BeforeFrame(viewWidth, viewHeight, windowWidth, windowHeight, (uint32_t)GetTickCount());
	return lastFrameDraws;
}

IDirect3DSwapChain9 *ViewSurface_PanelsChain()
{
	return lastFrameDraws ? theGameDevice.panels : nullptr;
}

bool ViewSurface_IsBroken()
{
	return theKeeper && theKeeper->State() == host::SurfaceState::Broken;
}
