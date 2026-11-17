// See autolink_render.h. Written 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "autolink_render.h"

#include <windows.h>
#include <d3d9.h>

#include <math.h>
#include <string.h>

#include "autolink_scene.h"

namespace host
{
	namespace
	{
		double const kCentreX = autolink_rules::kCentreX;   // FUN_0057f520's +0x2c (autolink_rules.h)
		double const kCentreY = autolink_rules::kCentreY;   // +0x30
		double const kLens = autolink_rules::kLens;         // +0x34 / +0x38: pixels to the unit of depth
		float const kNear = 100.0f;           // NAMED (autolink_render.h)
		size_t const kMostTriangles = 4000000;   // 4 million (one stock level has under 100 000): a hostile shape can't make the buffer endless
		UINT const kTrianglesPerDraw = 60000;

		struct Vertex
		{
			float x, y, z;
			D3DCOLOR colour;
		};

		DWORD const kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;

		LRESULT CALLBACK QuietWindow(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
		{
			return DefWindowProcW(window, message, wParam, lParam);
		}
	}

	struct AutoLinkRenderer::Parts
	{
		HWND window = NULL;
		IDirect3D9 *d3d = NULL;
		IDirect3DDevice9 *device = NULL;
		IDirect3DSurface9 *readBack = NULL;    // a system-memory copy of the back buffer (GetRenderTargetData)
		IDirect3DVertexBuffer9 *vertices = NULL;
		D3DPRESENT_PARAMETERS present = {};
		std::vector<Vertex> held;              // the scene's triangles, kept to fill the buffer again after a lost device
		std::vector<size_t> firstOf;           // module n's triangles: [firstOf[n], firstOf[n + 1])
		double origin[3] = { 0, 0, 0 };        // the vertices are held relative to it, so a float keeps a unit's precision

		void ReleaseVolatile()
		{
			if (readBack) { readBack->Release(); readBack = NULL; }
			if (vertices) { vertices->Release(); vertices = NULL; }
		}

		bool MakeVolatile(std::string *why)
		{
			if (!readBack && FAILED(device->CreateOffscreenPlainSurface(kAutoLinkPictureWidth, kAutoLinkPictureHeight, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &readBack, NULL)))
			{
				*why = "AutoLink: no memory surface for the read-back";
				return false;
			}

			if (!vertices && !held.empty())
			{
				UINT const bytes = (UINT)(held.size() * sizeof(Vertex));
				void *at = NULL;

				if (FAILED(device->CreateVertexBuffer(bytes, D3DUSAGE_WRITEONLY, kFvf, D3DPOOL_DEFAULT, &vertices, NULL)) || FAILED(vertices->Lock(0, bytes, &at, 0)))
				{
					*why = "AutoLink: no vertex buffer for the scene";
					ReleaseVolatile();
					return false;
				}

				memcpy(at, held.data(), bytes);
				vertices->Unlock();
			}

			return true;
		}

		// A lost device (another program took the display) is had back when it can be
		bool Ready(std::string *why)
		{
			HRESULT const state = device->TestCooperativeLevel();

			if (state == D3DERR_DEVICELOST)
			{
				*why = "AutoLink: the display is held by another program (D3DERR_DEVICELOST)";
				return false;
			}

			if (state == D3DERR_DEVICENOTRESET)
			{
				ReleaseVolatile();

				if (FAILED(device->Reset(&present)))
				{
					*why = "AutoLink: the device could not be reset";
					return false;
				}
			}

			return MakeVolatile(why);
		}
	};

	AutoLinkRenderer::AutoLinkRenderer() : parts(new Parts())
	{
	}

	AutoLinkRenderer::~AutoLinkRenderer()
	{
		Close();
		delete parts;
	}

	bool AutoLinkRenderer::Open(std::string *why)
	{
		std::string ignored;

		why = why ? why : &ignored;
		Close();

		HINSTANCE const instance = GetModuleHandleW(NULL);
		WNDCLASSW kind = {};

		kind.lpfnWndProc = QuietWindow;
		kind.hInstance = instance;
		kind.lpszClassName = L"AVP_OpenInteng_AutoLinkPictures";
		RegisterClassW(&kind);   // already registered by an earlier renderer: fine

		// the window never takes the front: WS_EX_NOACTIVATE (a click or a show does not activate it), WS_EX_TOOLWINDOW (no taskbar button, not in Alt+Tab), WS_POPUP without WS_VISIBLE, and
		// nothing here or in the device ever calls ShowWindow or SetForegroundWindow (the device is windowed, this window its device and focus window). host_autolink_render_test checks
		// that the foreground window is the same before Open and after Open, Load and the pictures.
		parts->window = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kind.lpszClassName, L"", WS_POPUP, 0, 0, kAutoLinkPictureWidth, kAutoLinkPictureHeight, NULL, NULL, instance, NULL);
		parts->d3d = parts->window ? Direct3DCreate9(D3D_SDK_VERSION) : NULL;

		if (!parts->d3d)
		{
			*why = "AutoLink: Direct3D 9 is not there";
			Close();
			return false;
		}

		D3DPRESENT_PARAMETERS &p = parts->present;

		memset(&p, 0, sizeof(p));
		p.Windowed = TRUE;
		p.BackBufferWidth = kAutoLinkPictureWidth;
		p.BackBufferHeight = kAutoLinkPictureHeight;
		p.BackBufferFormat = D3DFMT_X8R8G8B8;
		p.BackBufferCount = 1;
		p.SwapEffect = D3DSWAPEFFECT_DISCARD;
		p.hDeviceWindow = parts->window;
		p.EnableAutoDepthStencil = TRUE;
		p.AutoDepthStencilFormat = D3DFMT_D24X8;
		p.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

		// FPU_PRESERVE: the worker's floating point stays as it was (the game's code needs its precision)
		DWORD const flags = D3DCREATE_FPU_PRESERVE;

		if (FAILED(parts->d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, parts->window, flags | D3DCREATE_HARDWARE_VERTEXPROCESSING, &p, &parts->device)))
		{
			p.AutoDepthStencilFormat = D3DFMT_D16;

			if (FAILED(parts->d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, parts->window, flags | D3DCREATE_SOFTWARE_VERTEXPROCESSING, &p, &parts->device)))
			{
				*why = "AutoLink: the card gave no Direct3D 9 device";
				Close();
				return false;
			}
		}

		return parts->MakeVolatile(why);
	}

	void AutoLinkRenderer::Close()
	{
		parts->ReleaseVolatile();

		if (parts->device) { parts->device->Release(); parts->device = NULL; }
		if (parts->d3d) { parts->d3d->Release(); parts->d3d = NULL; }
		if (parts->window) { DestroyWindow(parts->window); parts->window = NULL; }
	}

	size_t AutoLinkRenderer::ModuleCount() const
	{
		return parts->firstOf.empty() ? 0 : parts->firstOf.size() - 1;
	}

	bool AutoLinkRenderer::Load(AutoLinkScene const &scene, size_t *leftOut, std::string *why)
	{
		std::vector<PortalModule> records;

		records.reserve(scene.list.size());

		for (AutoLinkModule const &one : scene.list)
		{
			records.push_back(one.module);
		}

		return Load(records, leftOut, why);
	}

	bool AutoLinkRenderer::Load(std::vector<PortalModule> const &records, size_t *leftOut, std::string *why)
	{
		std::string ignored;
		size_t dropped = 0;

		why = why ? why : &ignored;
		parts->held.clear();
		parts->firstOf.assign(1, 0);

		if (parts->vertices)
		{
			parts->vertices->Release();
			parts->vertices = NULL;
		}

		// an ID is the record's number + 1 in the colour's low 24 bits; far fewer records than that are held (a hostile level could name millions), so no ID wraps onto another
		if (records.size() > kAutoLinkMostRecords)
		{
			*why = "AutoLink: the level has more than 1,000,000 records";
			return false;
		}

		// the origin: the first record's place, so every vertex is a difference of two numbers of the same level
		if (!records.empty())
		{
			for (int i = 0; i < 3; i++)
			{
				parts->origin[i] = records[0].place[i];
			}
		}

		for (size_t n = 0; n < records.size(); n++)
		{
			PortalModule const &module = records[n];
			D3DCOLOR const colour = (D3DCOLOR)(0xFF000000u | (uint32_t)((n + autolink_rules::kIdOfNumber) & 0x00FFFFFFu));   // FUN_004c6380: the number + 1

			for (size_t k = 0; module.shape && k < module.shape->polygons.size(); k++)
			{
				std::vector<int32_t> const &corners = module.shape->polygons[k];
				std::vector<Vertex> fan;

				for (int32_t const point : corners)
				{
					if (point < 0 || (size_t)point * 3 + 2 >= module.shape->points.size())
					{
						fan.clear();
						dropped++;
						break;
					}

					Vertex v;

					v.x = (float)((double)module.place[0] + module.shape->points[(size_t)point * 3] - parts->origin[0]);
					v.y = (float)((double)module.place[1] + module.shape->points[(size_t)point * 3 + 1] - parts->origin[1]);
					v.z = (float)((double)module.place[2] + module.shape->points[(size_t)point * 3 + 2] - parts->origin[2]);
					v.colour = colour;
					fan.push_back(v);
				}

				// a polygon's corners as a fan of triangles (the shapes' polygons are flat and convex)
				for (size_t j = 1; j + 1 < fan.size(); j++)
				{
					if (parts->held.size() / 3 >= kMostTriangles)
					{
						*why = "AutoLink: the level has more than 4 million triangles";
						parts->held.clear();
						parts->firstOf.assign(1, 0);
						return false;
					}

					parts->held.push_back(fan[0]);
					parts->held.push_back(fan[j]);
					parts->held.push_back(fan[j + 1]);
				}
			}

			parts->firstOf.push_back(parts->held.size() / 3);
		}

		if (leftOut)
		{
			*leftOut = dropped;
		}

		return !parts->device || parts->MakeVolatile(why);
	}

	bool AutoLinkRenderer::Picture(AutoLinkView const &view, std::vector<uint8_t> const &shown, std::vector<uint32_t> *pixels, std::string *why)
	{
		std::string ignored;

		why = why ? why : &ignored;

		if (!pixels || !parts->device)
		{
			*why = "AutoLink: no device";
			return false;
		}

		if (!parts->Ready(why))
		{
			return false;
		}

		IDirect3DDevice9 *const device = parts->device;

		// The camera (AP-1..AP-6): a corner d (relative to the place) is seen at x = right . d, y = down . d, z = forward . d, the rows of the 16.16 matrix; Direct3D's y is up, so y is turned
		// over. The place is made relative to the vertices' origin.
		double row[9];

		for (int i = 0; i < 9; i++)
		{
			row[i] = view.turn[i] / 65536.0;
		}

		double const p[3] = { view.place[0] - parts->origin[0], view.place[1] - parts->origin[1], view.place[2] - parts->origin[2] };
		D3DMATRIX camera = {};

		// row-vector convention: (x y z 1) x M; column 0 the right row, 1 minus the down row, 2 the forward row
		for (int i = 0; i < 3; i++)
		{
			camera.m[i][0] = (float)row[i];
			camera.m[i][1] = (float)-row[3 + i];
			camera.m[i][2] = (float)row[6 + i];
		}

		camera.m[3][0] = (float)-(row[0] * p[0] + row[1] * p[1] + row[2] * p[2]);
		camera.m[3][1] = (float)(row[3] * p[0] + row[4] * p[1] + row[5] * p[2]);
		camera.m[3][2] = (float)-(row[6] * p[0] + row[7] * p[1] + row[8] * p[2]);
		camera.m[3][3] = 1.0f;

		// the far plane: the farthest drawn corner ahead of the camera (at least just past the near plane)
		double farthest = kNear * 2.0;

		for (size_t n = 0; n + 1 < parts->firstOf.size() && n < shown.size(); n++)
		{
			if (!shown[n])
			{
				continue;
			}

			for (size_t v = parts->firstOf[n] * 3; v < parts->firstOf[n + 1] * 3; v++)
			{
				Vertex const &c = parts->held[v];
				double const ahead = row[6] * (c.x - p[0]) + row[7] * (c.y - p[1]) + row[8] * (c.z - p[2]);

				farthest = ahead > farthest ? ahead : farthest;
			}
		}

		float const n = kNear, f = (float)(farthest * 1.01);
		D3DMATRIX lens = {};

		// x on the screen = 160 + 160 x / z (FUN_0057f520: centre 160, 160 pixels to the unit): a clip x of x, with w = z, over the half width 160; y the same over the half height 120
		lens.m[0][0] = (float)(kLens / kCentreX);
		lens.m[1][1] = (float)(kLens / kCentreY);
		lens.m[2][2] = f / (f - n);
		lens.m[2][3] = 1.0f;
		lens.m[3][2] = -n * f / (f - n);

		D3DMATRIX identity = {};

		identity.m[0][0] = identity.m[1][1] = identity.m[2][2] = identity.m[3][3] = 1.0f;

		device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

		if (FAILED(device->BeginScene()))
		{
			*why = "AutoLink: the device would not begin a picture";
			return false;
		}

		device->SetTransform(D3DTS_WORLD, &identity);
		device->SetTransform(D3DTS_VIEW, &camera);
		device->SetTransform(D3DTS_PROJECTION, &lens);
		device->SetRenderState(D3DRS_LIGHTING, FALSE);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
		device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device->SetRenderState(D3DRS_DITHERENABLE, FALSE);
		device->SetRenderState(D3DRS_FOGENABLE, FALSE);
		device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
		device->SetTexture(0, NULL);
		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
		device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		device->SetFVF(kFvf);

		if (parts->vertices)
		{
			device->SetStreamSource(0, parts->vertices, 0, sizeof(Vertex));

			for (size_t m = 0; m + 1 < parts->firstOf.size() && m < shown.size(); m++)
			{
				if (!shown[m])
				{
					continue;
				}

				for (size_t first = parts->firstOf[m]; first < parts->firstOf[m + 1]; first += kTrianglesPerDraw)
				{
					size_t const left = parts->firstOf[m + 1] - first;

					device->DrawPrimitive(D3DPT_TRIANGLELIST, (UINT)(first * 3), (UINT)(left < kTrianglesPerDraw ? left : kTrianglesPerDraw));
				}
			}
		}

		device->EndScene();

		IDirect3DSurface9 *back = NULL;
		D3DLOCKED_RECT locked = {};

		if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) || FAILED(device->GetRenderTargetData(back, parts->readBack)) ||
		    FAILED(parts->readBack->LockRect(&locked, NULL, D3DLOCK_READONLY)))
		{
			if (back)
			{
				back->Release();
			}

			*why = "AutoLink: the picture could not be read back";
			return false;
		}

		back->Release();
		pixels->resize((size_t)kAutoLinkPictureWidth * kAutoLinkPictureHeight);

		for (int r = 0; r < kAutoLinkPictureHeight; r++)
		{
			uint32_t const *const from = (uint32_t const *)((uint8_t const *)locked.pBits + (size_t)r * (size_t)locked.Pitch);

			for (int c = 0; c < kAutoLinkPictureWidth; c++)
			{
				(*pixels)[(size_t)r * kAutoLinkPictureWidth + c] = from[c] & 0x00FFFFFFu;
			}
		}

		parts->readBack->UnlockRect();
		return true;
	}
}
