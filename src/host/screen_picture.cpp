// See screen_picture.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "screen_picture.h"

#include <d3dx9.h>

// Windows' own numbers for PrintWindow's two flags (WinUser.h): only the inside of the window, and everything the screen composer shows
// for it, Direct3D's pictures included. Named here in case the Windows headers in use are older than PW_RENDERFULLCONTENT.
static UINT const kPrintInsideOnly = 0x00000001;   // PW_CLIENTONLY
static UINT const kPrintEverything = 0x00000002;   // PW_RENDERFULLCONTENT

bool ScreenPicture_Save(IDirect3DDevice9 *device, HWND window, const char *path, const ui::Box &view, const ui::Box &patch, uint32_t panelsColour,
	ScreenPictureReport *report)
{
	report->saved = false;
	report->panelsSeen = false;
	report->viewMean = 0.0;
	report->viewLitShare = 0.0;

	RECT inside = { 0, 0, 0, 0 };

	if (!device || !window || !path || !GetClientRect(window, &inside))
	{
		return false;
	}

	int32_t const width = inside.right - inside.left;
	int32_t const height = inside.bottom - inside.top;

	if (width < 1 || height < 1 || width > ui::kPanelMostSide || height > ui::kPanelMostSide)
	{
		return false;
	}

	// A picture in the processor's memory, in a format Windows' drawing can write into (Direct3D 9 gives such a surface a GDI handle)
	IDirect3DSurface9 *picture = 0;

	if (FAILED(device->CreateOffscreenPlainSurface((UINT)width, (UINT)height, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &picture, NULL)) || !picture)
	{
		return false;
	}

	HDC drawing = 0;
	bool printed = false;

	if (SUCCEEDED(picture->GetDC(&drawing)))
	{
		printed = PrintWindow(window, drawing, kPrintInsideOnly | kPrintEverything) != FALSE;
		picture->ReleaseDC(drawing);
	}

	D3DLOCKED_RECT held;

	if (printed && SUCCEEDED(picture->LockRect(&held, NULL, D3DLOCK_READONLY)))
	{
		ScreenPicture_Measure((const uint32_t *)held.pBits, width, height, held.Pitch / 4, view, patch, panelsColour, report);
		picture->UnlockRect();
	}

	report->saved = printed && SUCCEEDED(D3DXSaveSurfaceToFileA(path, D3DXIFF_PNG, picture, NULL, NULL));
	picture->Release();
	return report->saved && report->panelsSeen;
}
