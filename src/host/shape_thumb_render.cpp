// See shape_thumb_render.h. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The device, the hidden window and the read-back are autolink_render.cpp's (Drake), as that file does them.
#include "shape_thumb_render.h"

#include <windows.h>
#include <d3d9.h>
#include <math.h>
#include <string.h>

namespace host
{
	namespace
	{
		struct Vertex
		{
			float x, y, z;
			D3DCOLOR colour;
			float u, v;
		};

		DWORD const kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
		size_t const kTrianglesPerDraw = 10000;   // DrawPrimitiveUP's count in one call, well under every card's most (65,535)

		LRESULT CALLBACK QuietWindow(HWND window, UINT message, WPARAM w, LPARAM l)
		{
			return DefWindowProcW(window, message, w, l);
		}

		D3DCOLOR Shaded(uint32_t grey, float shade)
		{
			float const s = shade < 0.0f ? 0.0f : (shade > 1.0f ? 1.0f : shade);
			uint32_t const c = (uint32_t)((float)grey * s + 0.5f);
			return D3DCOLOR_XRGB(c, c, c);
		}
	}

	struct ShapeThumbRenderer::Parts
	{
		HWND window = NULL;
		IDirect3D9 *d3d = NULL;
		IDirect3DDevice9 *device = NULL;
		IDirect3DSurface9 *readBack = NULL;   // system memory, the back buffer's size: lost with nothing (it isn't a device resource that a reset takes)
		D3DPRESENT_PARAMETERS present = {};

		void ReleaseVolatile()
		{
			if (readBack)
			{
				readBack->Release();
				readBack = NULL;
			}
		}

		bool MakeVolatile(std::string *why)
		{
			ReleaseVolatile();

			if (FAILED(device->CreateOffscreenPlainSurface(kShapeThumbDrawSide, kShapeThumbDrawSide, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &readBack, NULL)))
			{
				*why = "the picker's pictures: no room to read a picture back";
				return false;
			}

			return true;
		}

		// The device ready to draw: lost and not yet back, false with *lost; lost and back, reset first (as AutoLink's Ready)
		bool Ready(std::string *why, bool *lost)
		{
			HRESULT const state = device->TestCooperativeLevel();

			if (state == D3DERR_DEVICELOST)
			{
				*why = "the picker's pictures: the card is busy (the device is lost); they are drawn when it is back";
				*lost = true;
				return false;
			}

			if (state == D3DERR_DEVICENOTRESET)
			{
				ReleaseVolatile();

				if (FAILED(device->Reset(&present)))
				{
					*why = "the picker's pictures: the device could not be reset";
					return false;
				}
			}

			return readBack ? true : MakeVolatile(why);
		}
	};

	ShapeThumbRenderer::ShapeThumbRenderer() : parts(new Parts())
	{
	}

	ShapeThumbRenderer::~ShapeThumbRenderer()
	{
		Close();
		delete parts;
	}

	bool ShapeThumbRenderer::IsOpen() const
	{
		return parts->device != NULL;
	}

	bool ShapeThumbRenderer::Open(std::string *why)
	{
		std::string ignored;

		why = why ? why : &ignored;
		Close();

		HINSTANCE const instance = GetModuleHandleW(NULL);
		WNDCLASSW kind = {};

		kind.lpfnWndProc = QuietWindow;
		kind.hInstance = instance;
		kind.lpszClassName = L"AVP_OpenInteng_PickerPictures";
		RegisterClassW(&kind);   // already registered by an earlier renderer: fine

		// never in front: WS_EX_NOACTIVATE, WS_EX_TOOLWINDOW, WS_POPUP without WS_VISIBLE, and nothing here calls ShowWindow (as AutoLink's window)
		parts->window = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kind.lpszClassName, L"", WS_POPUP, 0, 0, kShapeThumbDrawSide, kShapeThumbDrawSide, NULL, NULL,
			instance, NULL);
		if (!parts->window)
		{
			*why = "the picker's pictures: their hidden window could not be made";   // (Drake's nit on 66a5284e: not "Direct3D is not there")
			Close();
			return false;
		}

		parts->d3d = Direct3DCreate9(D3D_SDK_VERSION);

		if (!parts->d3d)
		{
			*why = "the picker's pictures: Direct3D 9 is not there";
			Close();
			return false;
		}

		D3DPRESENT_PARAMETERS &p = parts->present;

		memset(&p, 0, sizeof(p));
		p.Windowed = TRUE;
		p.BackBufferWidth = kShapeThumbDrawSide;
		p.BackBufferHeight = kShapeThumbDrawSide;
		p.BackBufferFormat = D3DFMT_X8R8G8B8;
		p.BackBufferCount = 1;
		p.SwapEffect = D3DSWAPEFFECT_DISCARD;
		p.hDeviceWindow = parts->window;
		p.EnableAutoDepthStencil = TRUE;
		p.AutoDepthStencilFormat = D3DFMT_D24X8;
		p.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

		DWORD const flags = D3DCREATE_FPU_PRESERVE;   // the game's code needs its floating point as it was

		// one change a try (Drake's nit on 66a5284e): the depth buffer's form first, then the vertex processing
		struct Try
		{
			D3DFORMAT depth;
			DWORD processing;
		};

		Try const tries[] = { { D3DFMT_D24X8, D3DCREATE_HARDWARE_VERTEXPROCESSING }, { D3DFMT_D16, D3DCREATE_HARDWARE_VERTEXPROCESSING },
			{ D3DFMT_D16, D3DCREATE_SOFTWARE_VERTEXPROCESSING } };

		for (Try const &t : tries)
		{
			p.AutoDepthStencilFormat = t.depth;

			if (!parts->device && FAILED(parts->d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, parts->window, flags | t.processing, &p, &parts->device)))
			{
				parts->device = NULL;
			}
		}

		if (!parts->device)
		{
			*why = "the picker's pictures: the card gave no Direct3D 9 device";
			Close();
			return false;
		}

		return parts->MakeVolatile(why);
	}

	void ShapeThumbRenderer::Close()
	{
		parts->ReleaseVolatile();

		if (parts->device) { parts->device->Release(); parts->device = NULL; }
		if (parts->d3d) { parts->d3d->Release(); parts->d3d = NULL; }
		if (parts->window) { DestroyWindow(parts->window); parts->window = NULL; }
	}

	bool ShapeThumb_ViewMatrix(float const eye[3], float const at[3], float view[4][4])
	{
		// the camera's rows in the world: forward toward the target; right = down x forward and down = forward x right, as the world is (x right, y down, z
		// forward: right x down = forward); Direct3D's y is up, so the down row goes in turned over (as autolink_render.cpp does with the game's own rows)
		float f[3] = { at[0] - eye[0], at[1] - eye[1], at[2] - eye[2] };
		float const fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);

		if (!(fl > 1e-6f))
		{
			return false;
		}

		for (float &c : f)
		{
			c /= fl;
		}

		// down x forward with down (0, 1, 0): (1 * f2 - 0 * f1, 0 * f0 - 0 * f2, 0 * f1 - 1 * f0) = (f2, 0, -f0)
		float r[3] = { f[2], 0.0f, -f[0] };
		float const rl = sqrtf(r[0] * r[0] + r[2] * r[2]);

		if (!(rl > 1e-6f))
		{
			r[0] = 1.0f;   // looking straight down or up: any right will do
			r[2] = 0.0f;
		}
		else
		{
			r[0] /= rl;
			r[2] /= rl;
		}

		float const d[3] = { f[1] * r[2] - f[2] * r[1], f[2] * r[0] - f[0] * r[2], f[0] * r[1] - f[1] * r[0] };   // forward x right

		memset(view, 0, sizeof(float) * 16);

		for (int i = 0; i < 3; i++)
		{
			view[i][0] = r[i];
			view[i][1] = -d[i];
			view[i][2] = f[i];
		}

		view[3][0] = -(r[0] * eye[0] + r[1] * eye[1] + r[2] * eye[2]);
		view[3][1] = d[0] * eye[0] + d[1] * eye[1] + d[2] * eye[2];
		view[3][2] = -(f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2]);
		view[3][3] = 1.0f;
		return true;
	}

	bool ShapeThumb_LensMatrix(float fieldOfView, float nearPlane, float farPlane, float lens[4][4])
	{
		if (!(fieldOfView > 0.01f && fieldOfView < 3.0f) || !(nearPlane > 0.0f) || !(farPlane > nearPlane))
		{
			return false;
		}

		float const scale = 1.0f / tanf(fieldOfView / 2.0f);

		memset(lens, 0, sizeof(float) * 16);
		lens[0][0] = scale;
		lens[1][1] = scale;
		lens[2][2] = farPlane / (farPlane - nearPlane);
		lens[2][3] = 1.0f;
		lens[3][2] = -nearPlane * farPlane / (farPlane - nearPlane);
		return true;
	}

	void ShapeThumb_HalveSmooth(uint8_t const *pixels, int32_t side, int32_t pitch, std::vector<uint8_t> *out)
	{
		int32_t const half = side / 2;

		out->assign((size_t)half * (size_t)half * 4, 0);

		for (int32_t y = 0; y < half; y++)
		{
			uint8_t const *const top = pixels + (size_t)(2 * y) * (size_t)pitch;
			uint8_t const *const bottom = top + pitch;

			for (int32_t x = 0; x < half; x++)
			{
				uint8_t *const to = &(*out)[((size_t)y * (size_t)half + (size_t)x) * 4];

				for (int c = 0; c < 3; c++)
				{
					unsigned const sum = (unsigned)top[(2 * x) * 4 + c] + top[(2 * x + 1) * 4 + c] + bottom[(2 * x) * 4 + c] + bottom[(2 * x + 1) * 4 + c];
					to[c] = (uint8_t)((sum + 2) / 4);
				}

				to[3] = 255;
			}
		}
	}

	bool ShapeThumbRenderer::Draw(std::vector<ShapeThumbBatch> const &batches, std::vector<ShapeThumbTexture> const &textures, float const eye[3], float const at[3],
		float fieldOfView, uint32_t background, std::vector<uint8_t> *bgra, std::string *why, bool *tryLater)
	{
		std::string ignored;
		bool lost = false;

		why = why ? why : &ignored;

		if (tryLater)
		{
			*tryLater = false;
		}

		if (!bgra || !parts->device)
		{
			*why = "the picker's pictures: no device";
			return false;
		}

		if (!parts->Ready(why, &lost))
		{
			if (tryLater)
			{
				*tryLater = lost;   // the card is busy, not the picture at fault: said in the result, not only in the words (Drake's SHOULD 1 on df479439)
			}

			return false;
		}

		// the near plane below holds a framed sphere only for a lens up to about 89 degrees across: a wider one is refused (Drake's SHOULD 2 on 66a5284e)
		if (!(fieldOfView > 0.01f && fieldOfView <= kShapeThumbMostFieldOfView))
		{
			*why = "the picker's pictures: a lens wider than the renderer frames";
			return false;
		}

		IDirect3DDevice9 *const device = parts->device;
		float view[4][4], lens[4][4];
		float const distance = sqrtf((at[0] - eye[0]) * (at[0] - eye[0]) + (at[1] - eye[1]) * (at[1] - eye[1]) + (at[2] - eye[2]) * (at[2] - eye[2]));

		// the depth: from just in front of the shape's nearest possible point to past its farthest (the camera frames a sphere round the middle: the radius
		// is at most the distance times the lens' half-angle sine, so half the distance either side holds it)
		float const nearPlane = distance * 0.3f > 1.0f ? distance * 0.3f : 1.0f;

		if (!ShapeThumb_ViewMatrix(eye, at, view) || !ShapeThumb_LensMatrix(fieldOfView, nearPlane, distance * 2.0f + 1.0f, lens))
		{
			*why = "the picker's pictures: the camera can't be made (the shape has no size)";
			return false;
		}

		D3DMATRIX world = {}, camera, glass;

		world.m[0][0] = world.m[1][1] = world.m[2][2] = world.m[3][3] = 1.0f;
		memcpy(camera.m, view, sizeof(camera.m));
		memcpy(glass.m, lens, sizeof(glass.m));

		// EVERY ALLOCATION BEFORE THE SCENE (Drake's SHOULD 1 on 66a5284e): the corners' room for the largest draw is taken first, so nothing in the scene can
		// throw: a throw there would leak the textures made below and leave the device inside BeginScene for every later picture
		std::vector<Vertex> corners;

		corners.reserve(kTrianglesPerDraw * 3);

		std::vector<IDirect3DTexture9 *> made(textures.size(), NULL);

		// each picture's pixels a texture for this drawing only (managed: the card's copy is Direct3D's to keep)
		for (size_t i = 0; i < textures.size(); i++)
		{
			ShapeThumbTexture const &t = textures[i];

			if (t.width == 0 || t.height == 0 || t.width > kShapeThumbMostTextureSide || t.height > kShapeThumbMostTextureSide ||
				t.bgra.size() != (size_t)t.width * (size_t)t.height * 4)
			{
				continue;
			}

			IDirect3DTexture9 *texture = NULL;
			D3DLOCKED_RECT locked = {};

			if (FAILED(device->CreateTexture(t.width, t.height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL)))
			{
				continue;
			}

			if (FAILED(texture->LockRect(0, &locked, NULL, 0)) || locked.Pitch < (INT)(t.width * 4))
			{
				texture->Release();
				continue;
			}

			for (uint32_t row = 0; row < t.height; row++)
			{
				memcpy((uint8_t *)locked.pBits + (size_t)row * (size_t)locked.Pitch, t.bgra.data() + (size_t)row * t.width * 4, (size_t)t.width * 4);
			}

			texture->UnlockRect(0);
			made[i] = texture;
		}

		device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, (D3DCOLOR)(0xFF000000u | (background & 0x00FFFFFFu)), 1.0f, 0);

		bool drawn = SUCCEEDED(device->BeginScene());
		bool drewAll = true;   // every draw call answered that it drew

		if (drawn)
		{
			device->SetTransform(D3DTS_WORLD, &world);
			device->SetTransform(D3DTS_VIEW, &camera);
			device->SetTransform(D3DTS_PROJECTION, &glass);
			device->SetRenderState(D3DRS_LIGHTING, FALSE);            // the shade is worked out already, in each corner's colour
			device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);     // both sides
			device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
			device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
			device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
			device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			device->SetRenderState(D3DRS_FOGENABLE, FALSE);
			device->SetRenderState(D3DRS_ALPHAREF, 0x80);
			device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
			device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
			device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
			device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
			device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
			device->SetFVF(kFvf);

			for (ShapeThumbBatch const &batch : batches)
			{
				IDirect3DTexture9 *const texture = batch.picture >= 0 && (size_t)batch.picture < made.size() ? made[(size_t)batch.picture] : NULL;

				device->SetTexture(0, texture);
				device->SetRenderState(D3DRS_ALPHATESTENABLE, texture ? TRUE : FALSE);
				device->SetTextureStageState(0, D3DTSS_COLOROP, texture ? D3DTOP_MODULATE : D3DTOP_SELECTARG1);
				device->SetTextureStageState(0, D3DTSS_COLORARG1, texture ? D3DTA_TEXTURE : D3DTA_DIFFUSE);
				device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
				device->SetTextureStageState(0, D3DTSS_ALPHAOP, texture ? D3DTOP_SELECTARG1 : D3DTOP_DISABLE);
				device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

				// a textured corner is the texel times its shade (white times the shade); a shaded one is a light grey times it
				uint32_t const grey = texture ? 255u : 200u;
				size_t const triangles = batch.corners.size() / 3;

				for (size_t first = 0; first < triangles; first += kTrianglesPerDraw)
				{
					size_t const count = triangles - first < kTrianglesPerDraw ? triangles - first : kTrianglesPerDraw;

					corners.resize(count * 3);   // (inside the room reserved before the scene: never an allocation here)

					for (size_t k = 0; k < count * 3; k++)
					{
						ShapeThumbCorner const &c = batch.corners[first * 3 + k];

						corners[k] = { c.x, c.y, c.z, Shaded(grey, c.shade), texture ? c.u : 0.0f, texture ? c.v : 0.0f };
					}

					drewAll = SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)count, corners.data(), sizeof(Vertex))) && drewAll;   // (Drake's nit: said)
				}
			}

			device->SetTexture(0, NULL);
			device->EndScene();
		}

		for (IDirect3DTexture9 *texture : made)
		{
			if (texture)
			{
				texture->Release();
			}
		}

		if (!drawn || !drewAll)
		{
			*why = !drawn ? "the picker's pictures: the device would not begin a picture" : "the picker's pictures: the card refused a part of the drawing";
			return false;
		}

		IDirect3DSurface9 *back = NULL;
		D3DLOCKED_RECT locked = {};

		if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) || FAILED(device->GetRenderTargetData(back, parts->readBack)) ||
			FAILED(parts->readBack->LockRect(&locked, NULL, D3DLOCK_READONLY)))
		{
			if (back)
			{
				back->Release();
			}

			*why = "the picker's pictures: a picture could not be read back";
			return false;
		}

		back->Release();

		bool const room = locked.Pitch >= kShapeThumbDrawSide * 4;

		if (room)
		{
			ShapeThumb_HalveSmooth((uint8_t const *)locked.pBits, kShapeThumbDrawSide, locked.Pitch, bgra);
		}

		parts->readBack->UnlockRect();

		if (!room)
		{
			*why = "the picker's pictures: the read-back's rows are too short";
			return false;
		}

		return true;
	}
}
