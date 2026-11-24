// shape_pane_gpu.cpp: THE LIVE PANES' DRAW PATH (shape_pane_gpu.h). Written 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng, in place of Vasquez's
// stand-in. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The owner, 2026-11-23: "Goal, I want sub milisecond draw on each of the panes and I want them real-time renderered rotating in 3d". Each frame, in the
// level's own frame (the screen side's hook, before the panels), every pane asked for is drawn, turning, into ONE texture on the card, the atlas (the game's
// TextureUsage_RenderTarget), each into a region its box's size; then, in the panels' pass, each region is shown in its box (ShapePaneGpu_Composite), over the
// object card and under the picker's names, frame and bar. The safety case, reviewed by Drake before this code: AvP-docs tools/frost/pickertime/
// SAFETY-CASE-live-panes.md. How it keeps each of its rules:
//   1. ITS OWN RESOURCES: the atlas is made and released only here, and its number is checked against the slot's own name and usage before every use (Drake's
//      hole 1: a number must never outlive its texture); the drawings' buffers and pictures are the store's (shape_pane_store.h), only read here.
//   2. EVERY STATE PUT BACK: one D3DSBT_ALL state block captured before the first pane and applied after the last; the render target and depth surface, which
//      a state block doesn't hold, kept and put back by hand, BEFORE the Apply (setting a render target resets the viewport, which the block then puts back).
//   3. BOUNDED: the frame's triangles (shape_pane_plan.h, kShapePanesTrianglesAFrame) and the atlas's room; a pane past either keeps its still picture.
//   5. A LOST DEVICE: the atlas is the texture manager's (released for the reset and made again, its slot held); the depth surface, the state block and the
//      timing queries are let go through device_reset_hooks.h before the reset (Drake's hole 2), safely every frame the device stays lost, and made again at the
//      next frame that draws. No surface of the atlas is held from one frame to the next. The hook's owner is this file's own, kept as long as the program runs,
//      so it is never taken away (at the program's end the device goes after it).
//   8. Drake's four: the main thread only (every call here comes from the frame or the paint); every Create* failure becomes words, and no null is ever used;
//      timestamp queries may be refused or disjoint ("gpu unknown"), read a frame later, never waited on; the panes are drawn once a frame (the screen side's
//      hook runs once; FrameEnd draws only what was asked for since the last).
//   9. THE PANES' OWN VERTEX SHADER (section 9, and Drake's four on it): our HLSL text below, compiled once at run time by D3DX, a failure remembered for the
//      sitting and its words logged once, cut short; it never normalizes (a face with no area comes from the store with a zero normal, which lights it at the
//      floor, as the still pictures do); it writes the shade to red, green and blue and 1 to alpha; each constant has a fixed register, set with exactly that
//      start and count.
#include "3dc.h"
#include "inline.h"
#include "gameplat.h"
#include "platform.h"
#include "renderer.h"
#include "d3_func.h"
#include "TextureManager.h"
#include "device_reset_hooks.h"   // what the panes keep on the card alone, let go before a reset (avpx, Melanikus/AvP #299)
#include "quad_part.h"            // where a region of the atlas lies in the texture's places (avpx, #299)
#include "logString.h"

#include <windows.h>
#include <string.h>

#include <string>
#include <vector>

#include "shape_pane_gpu.h"
#include "shape_pane_plan.h"
#include "shape_pane_store.h"
#include "shape_thumb.h"          // ShapeThumb_LightTurned: the light turned with the camera, as the still pictures are lit
#include "state_line.h"

namespace
{
	float const kFieldOfView = 0.7f;                         // the still pictures' (shape_picker_screen.cpp)
	char const *const kAtlasName = "editor_shape_panes_atlas";
	size_t const kMostQueued = 4096;                         // panes asked for in one frame, at most (a screen holds a few hundred)
	size_t const kMostShaderWords = 400;                     // the compiler's words kept in the log, at most

	// THE PANES' VERTEX SHADER: each corner's place on the glass by one matrix (c0 to c3); its shade the still pictures' own (shape_thumb.cpp: 0.35 + 0.65 times
	// how squarely its face meets the light (c4), either side alike), times the batch's base (c5: a picture's texels 1, a plain face's grey 200 of 255); its
	// picture place passed on. No loop, no texture read, no normalize, no index into the constants.
	char const kShaderText[] =
		"float4x4 placeOnGlass : register(c0);\n"
		"float3 toLight : register(c4);\n"
		"float base : register(c5);\n"
		"struct Corner { float3 place : POSITION; float3 normal : NORMAL; float2 picture : TEXCOORD0; };\n"
		"struct Drawn { float4 place : POSITION; float4 colour : COLOR0; float2 picture : TEXCOORD0; };\n"
		"Drawn vs_main(Corner c)\n"
		"{\n"
		"	Drawn d;\n"
		"	d.place = mul(float4(c.place, 1.0f), placeOnGlass);\n"
		"	float shade = (0.35f + 0.65f * abs(dot(c.normal, toLight))) * base;\n"
		"	d.colour = float4(shade, shade, shade, 1.0f);\n"
		"	d.picture = c.picture;\n"
		"	return d;\n"
		"}\n";

	UINT const kMatrixRegister = 0;   // c0 to c3
	UINT const kLightRegister = 4;    // c4
	UINT const kBaseRegister = 5;     // c5

	struct Queued
	{
		int handle;
		int32_t box[4];
		float turn;
	};

	struct Shown   // a pane drawn this frame: its box (the window's pixels) and its region of the atlas
	{
		int32_t box[4];
		uint32_t region[4];
	};

	std::vector<Queued> queued;       // asked for since the last FrameEnd
	std::vector<Shown> shown;         // drawn by the last FrameEnd, shown by Composite

	texID_t atlas = 0;                // the atlas's number, while atlasMade
	bool atlasMade = false;
	IDirect3DSurface9 *depth = nullptr;          // the atlas's own depth (on the card alone: let go before a reset)
	IDirect3DStateBlock9 *saved = nullptr;       // the device's every state, captured before the panes and put back after (let go before a reset)
	IDirect3DVertexShader9 *shader = nullptr;    // the panes' own (kept across a reset: it isn't the card's alone)
	bool shaderTried = false;
	std::string notLive;              // why the panes aren't drawn live ("" while they are)

	// THE TIMING (rule 6): two sets of queries, one issued this frame while the other is read; each read a frame after it was issued, never waited on
	struct Timing
	{
		IDirect3DQuery9 *disjoint = nullptr, *frequency = nullptr, *started = nullptr, *ended = nullptr;
		bool issued = false;
	};

	Timing timing[2];
	int timingNow = 0;
	bool queriesRefused = false;      // the tests' own switch (ShapePaneGpu_RefuseQueries): CreateQuery treated as failed
	bool queriesFailed = false;       // CreateQuery failed once: the card's time is unknown for the sitting
	bool gpuKnown = false;
	uint32_t gpuUs = 0, cpuUs = 0, mostUs = 0;
	uint64_t framesDrawn = 0;
	uint32_t drawnLast = 0;
	ULONGLONG lastDrawnAt = 0;        // when a FrameEnd last drew panes (GetTickCount64); none does while the picker is closed (its hook is let go)
	ULONGLONG const kDrawnLately = 250;   // ms: "drawn" says the last frame's panes only if a frame drew them this lately (about five of the editor's 47 ms frames)
	bool hookAdded = false;
	int hookOwner = 0;                // (its address is the hook's owner)

	template <typename T> void LetGo(T *&thing)
	{
		if (thing)
		{
			thing->Release();
			thing = nullptr;
		}
	}

	void LetGoOfTheTiming(Timing &t)
	{
		LetGo(t.disjoint);
		LetGo(t.frequency);
		LetGo(t.started);
		LetGo(t.ended);
		t.issued = false;
	}

	// BEFORE A RESET of the device (device_reset_hooks.h): everything of the panes' on the card alone let go (safe every frame the device stays lost); each is
	// made again at the next frame that draws
	void LetGoBeforeReset(void *)
	{
		LetGo(depth);
		LetGo(saved);

		for (Timing &t : timing)
		{
			LetGoOfTheTiming(t);
		}

		gpuKnown = false;
	}

	bool AtlasHeldForReset()
	{
		uint32_t width = 0, height = 0;

		// (a slot held for a reset isn't valid, so Tex_GetRealSize says no; the number is the manager's own, given by Tex_Create, so it is in its list)
		return atlasMade && !Tex_GetRealSize(atlas, &width, &height) && Tex_GetTextureDetails(atlas).heldForReset;
	}

	// the atlas's number still means the atlas: in the list, its texture there, its own name and usage (Drake's hole 1). If not (the level was opened again and
	// every texture let go, say), the number is forgotten at once, never used, and the atlas made again when next needed. One only held for a reset is still ours,
	// and simply isn't used until it is made again.
	bool AtlasIsOurs()
	{
		uint32_t width = 0, height = 0;

		if (atlasMade && Tex_GetRealSize(atlas, &width, &height))
		{
			Texture const &t = Tex_GetTextureDetails(atlas);

			if (t.name == kAtlasName && t.usage == TextureUsage_RenderTarget && t.texture)
			{
				return true;
			}

			atlasMade = false;   // someone else's now: forgotten
			return false;
		}

		if (atlasMade && !AtlasHeldForReset())
		{
			atlasMade = false;   // gone: forgotten
		}

		return false;
	}

	// everything a frame needs on the card, made the first time it is missing; false with notLive saying why
	bool MakeWhatIsMissing(IDirect3DDevice9 *device)
	{
		if (!hookAdded)
		{
			hookAdded = DeviceReset_Add(&hookOwner, LetGoBeforeReset);

			if (!hookAdded)
			{
				notLive = "the live panes couldn't ask to be told of a reset of the card";
				return false;
			}
		}

		if (!shaderTried)
		{
			shaderTried = true;

			ID3DXBuffer *code = nullptr, *errors = nullptr;
			HRESULT const compiled = D3DXCompileShader(kShaderText, (UINT)(sizeof(kShaderText) - 1), NULL, NULL, "vs_main", "vs_2_0", D3DXSHADER_PACKMATRIX_ROWMAJOR, &code,
				&errors, NULL);

			if (FAILED(compiled) || !code || FAILED(device->CreateVertexShader((DWORD const *)code->GetBufferPointer(), &shader)) || !shader)
			{
				shader = nullptr;
				notLive = "the card can't run the live panes' shader (vs_2_0)";

				std::string words;

				if (errors && errors->GetBufferPointer() && errors->GetBufferSize() > 0)
				{
					words.assign((char const *)errors->GetBufferPointer(), strnlen((char const *)errors->GetBufferPointer(), errors->GetBufferSize()));
				}

				LogString("Live panes: " + notLive + (words.empty() ? std::string() : ": " + words.substr(0, kMostShaderWords)));
			}

			LetGo(code);
			LetGo(errors);
		}

		if (!shader)
		{
			return false;   // (notLive says why, from the one try)
		}

		if (AtlasHeldForReset())
		{
			notLive = "the card is being reset";
			return false;
		}

		if (!AtlasIsOurs())
		{
			texID_t const made = Tex_Create(kAtlasName, host::kShapePaneAtlasSide, host::kShapePaneAtlasSide, 32, TextureUsage_RenderTarget);

			if (made == MISSING_TEXTURE)
			{
				notLive = "the card can't make the live panes' picture (" + std::to_string(host::kShapePaneAtlasSide) + " x " + std::to_string(host::kShapePaneAtlasSide) + ")";
				return false;
			}

			atlas = made;
			atlasMade = true;
		}

		if (!depth && (FAILED(device->CreateDepthStencilSurface(host::kShapePaneAtlasSide, host::kShapePaneAtlasSide, D3DFMT_D24X8, D3DMULTISAMPLE_NONE, 0, TRUE, &depth,
			NULL)) || !depth))
		{
			depth = nullptr;

			if (FAILED(device->CreateDepthStencilSurface(host::kShapePaneAtlasSide, host::kShapePaneAtlasSide, D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, TRUE, &depth, NULL)) || !depth)
			{
				depth = nullptr;
				notLive = "the card can't make the live panes' depth";
				return false;
			}
		}

		if (!saved && (FAILED(device->CreateStateBlock(D3DSBT_ALL, &saved)) || !saved))
		{
			saved = nullptr;
			notLive = "the card can't keep its settings for the live panes";
			return false;
		}

		// the timing queries: refused (an old card, or the tests' switch) means the card's time is unknown, never that the panes aren't drawn
		for (Timing &t : timing)
		{
			if (!queriesFailed && !t.disjoint)
			{
				queriesFailed = queriesRefused || FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &t.disjoint)) ||
					FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &t.frequency)) || FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &t.started)) ||
					FAILED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &t.ended)) || !t.disjoint || !t.frequency || !t.started || !t.ended;
			}

			if (queriesFailed)
			{
				LetGoOfTheTiming(t);
			}
		}

		notLive.clear();
		return true;
	}

	// the card's time for the set issued a frame ago, if it has it now: never waited on (D3DGETDATA_FLUSH asks the card to get on with it; the answer is read
	// again next frame if it isn't there yet)
	void ReadTheCardsTime()
	{
		Timing &t = timing[1 - timingNow];

		if (queriesFailed)
		{
			gpuKnown = false;
			return;
		}

		if (!t.issued || !t.disjoint)
		{
			return;
		}

		BOOL disjoint = TRUE;
		UINT64 frequency = 0, started = 0, ended = 0;

		if (t.disjoint->GetData(&disjoint, sizeof(disjoint), D3DGETDATA_FLUSH) != S_OK || t.frequency->GetData(&frequency, sizeof(frequency), 0) != S_OK ||
			t.started->GetData(&started, sizeof(started), 0) != S_OK || t.ended->GetData(&ended, sizeof(ended), 0) != S_OK)
		{
			return;   // not there yet: the last time stays as it was (known or not)
		}

		t.issued = false;

		if (disjoint || frequency == 0 || ended < started)
		{
			gpuKnown = false;   // the clock changed speed in between: that time means nothing
			return;
		}

		UINT64 const us = (ended - started) * 1000000ull / frequency;
		gpuUs = us > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)us;
		gpuKnown = true;
	}
}

// (ShapePaneGpu_Upload, _Forget, _ForgetAll and ShapePaneStore_Find are the store's: shape_pane_store.cpp)
namespace host
{
	void ShapePaneGpu_Draw(int handle, int32_t const windowBox[4], float turn)
	{
		// a box starting left of the window or above it is refused (Drake's NIT on 0ae66bf9): the composite queues its place as a whole number of pixels from the
		// window's top left, so a negative one would land far off; the picker's grid never asks for one
		if (handle < 0 || !windowBox || windowBox[0] < 0 || windowBox[1] < 0 || windowBox[2] <= 0 || windowBox[3] <= 0 || queued.size() >= kMostQueued)
		{
			return;
		}

		Queued q;
		q.handle = handle;
		memcpy(q.box, windowBox, sizeof(q.box));
		q.turn = turn;
		queued.push_back(q);
	}

	void ShapePaneGpu_FrameEnd()
	{
		shown.clear();
		drawnLast = 0;

		IDirect3DDevice9 *const device = d3d.lpD3DDevice;

		if (queued.empty() || !device)
		{
			queued.clear();
			return;
		}

		LARGE_INTEGER cpuStart = {}, cpuEnd = {}, cpuFrequency = {};
		QueryPerformanceCounter(&cpuStart);

		if (!MakeWhatIsMissing(device))
		{
			queued.clear();
			return;
		}

		ReadTheCardsTime();

		// THE PLAN: each asked pane whose drawing the store holds, given its region, within the frame's triangles
		std::vector<ShapePaneAsk> asks;
		std::vector<ShapePaneDrawing const *> drawings;

		for (Queued const &q : queued)
		{
			ShapePaneDrawing const *const drawing = ShapePaneStore_Find(q.handle);

			if (!drawing || !drawing->corners || !drawing->indices)
			{
				continue;
			}

			ShapePaneAsk a;
			a.handle = q.handle;
			memcpy(a.box, q.box, sizeof(a.box));
			a.turn = q.turn;
			a.triangles = drawing->triangles;
			asks.push_back(a);
			drawings.push_back(drawing);
		}

		queued.clear();

		std::vector<ShapePanePlace> const places = ShapePane_Plan(asks, kShapePaneAtlasSide, kShapePanesTrianglesAFrame);
		IDirect3DSurface9 *atlasSurface = nullptr, *oldTarget = nullptr, *oldDepth = nullptr;
		r_Texture const atlasTexture = Tex_GetTexture(atlas);

		if (asks.empty() || !atlasTexture || FAILED(atlasTexture->GetSurfaceLevel(0, &atlasSurface)) || !atlasSurface)
		{
			LetGo(atlasSurface);
			return;
		}

		Timing &t = timing[timingNow];
		bool const timed = !queriesFailed && t.disjoint && !t.issued;

		if (timed)
		{
			t.disjoint->Issue(D3DISSUE_BEGIN);
			t.started->Issue(D3DISSUE_END);
		}

		// EVERY STATE KEPT, then the atlas drawn into (section 2)
		saved->Capture();
		device->GetRenderTarget(0, &oldTarget);
		device->GetDepthStencilSurface(&oldDepth);
		device->SetRenderTarget(0, atlasSurface);
		device->SetDepthStencilSurface(depth);
		device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0);   // see-through everywhere but where a pane draws

		device->SetVertexShader(shader);
		device->SetPixelShader(NULL);
		device->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);             // both sides
		device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
		device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device->SetRenderState(D3DRS_FOGENABLE, FALSE);
		device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
		device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0F);
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHAREF, 0x80);
		device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
		device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		for (size_t i = 0; i < asks.size(); i++)
		{
			ShapePanePlace const &place = places[i];
			ShapePaneDrawing const &drawing = *drawings[i];
			float eye[3], matrix[4][4], toLight[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

			if (!place.drawn)
			{
				continue;
			}

			ShapePane_TurnEye(drawing.eye, drawing.at, asks[i].turn, eye);

			if (!ShapePane_Matrix(eye, drawing.at, kFieldOfView, place.region[2], place.region[3], matrix))
			{
				continue;
			}

			ShapeThumb_LightTurned(asks[i].turn, toLight);

			D3DVIEWPORT9 const region = { place.region[0], place.region[1], place.region[2], place.region[3], 0.0f, 1.0f };

			device->SetViewport(&region);
			device->SetVertexShaderConstantF(kMatrixRegister, &matrix[0][0], 4);
			device->SetVertexShaderConstantF(kLightRegister, toLight, 1);
			device->SetStreamSource(0, drawing.corners, 0, sizeof(ShapePaneCorner));
			device->SetIndices(drawing.indices);

			for (ShapePaneBatch const &batch : drawing.batches)
			{
				float const base[4] = { batch.picture ? 1.0f : 200.0f / 255.0f, 0.0f, 0.0f, 0.0f };

				if (batch.triangles == 0 || batch.vertexCount == 0)
				{
					continue;
				}

				device->SetVertexShaderConstantF(kBaseRegister, base, 1);
				device->SetTexture(0, batch.picture);
				device->SetRenderState(D3DRS_ALPHATESTENABLE, batch.picture ? TRUE : FALSE);
				device->SetTextureStageState(0, D3DTSS_COLOROP, batch.picture ? D3DTOP_MODULATE : D3DTOP_SELECTARG1);
				device->SetTextureStageState(0, D3DTSS_COLORARG1, batch.picture ? D3DTA_TEXTURE : D3DTA_DIFFUSE);
				device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
				device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
				device->SetTextureStageState(0, D3DTSS_ALPHAARG1, batch.picture ? D3DTA_TEXTURE : D3DTA_DIFFUSE);

				// the store's exact range for this batch (hole 3: every index was checked below its point count before anything was made)
				device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, batch.minVertex, batch.vertexCount, batch.firstIndex, batch.triangles);
			}

			Shown s;
			memcpy(s.box, asks[i].box, sizeof(s.box));
			memcpy(s.region, place.region, sizeof(s.region));
			shown.push_back(s);
		}

		// EVERYTHING PUT BACK: the render target and depth first (setting a target resets the viewport), then every other state from the block
		device->SetTexture(0, NULL);
		device->SetStreamSource(0, NULL, 0, 0);
		device->SetIndices(NULL);
		device->SetRenderTarget(0, oldTarget);
		device->SetDepthStencilSurface(oldDepth);
		saved->Apply();
		LetGo(oldTarget);
		LetGo(oldDepth);
		LetGo(atlasSurface);

		if (timed)
		{
			t.ended->Issue(D3DISSUE_END);
			t.frequency->Issue(D3DISSUE_END);
			t.disjoint->Issue(D3DISSUE_END);
			t.issued = true;
			timingNow = 1 - timingNow;
		}

		drawnLast = (uint32_t)shown.size();
		lastDrawnAt = GetTickCount64();
		framesDrawn++;

		QueryPerformanceCounter(&cpuEnd);
		QueryPerformanceFrequency(&cpuFrequency);

		LONGLONG const ticks = cpuEnd.QuadPart - cpuStart.QuadPart;
		cpuUs = cpuFrequency.QuadPart > 0 && ticks >= 0 ? (uint32_t)(ticks * 1000000 / cpuFrequency.QuadPart) : 0;
		mostUs = cpuUs > mostUs ? cpuUs : mostUs;
	}

	void ShapePaneGpu_Composite()
	{
		if (shown.empty() || !AtlasIsOurs())
		{
			return;
		}

		Texture const &t = Tex_GetTextureDetails(atlas);

		for (Shown const &s : shown)
		{
			float uv[4];

			if (!QuadPart_UVs(t.width, t.height, t.realWidth, t.realHeight, s.region[0], s.region[1], s.region[2], s.region[3], uv))
			{
				continue;
			}

			// DrawFontQuad's corners: bottom left, top left, bottom right, top right
			float corners[8] = { uv[0], uv[3], uv[0], uv[1], uv[2], uv[3], uv[2], uv[1] };

			DrawFontQuad((uint32_t)s.box[0], (uint32_t)s.box[1], (uint32_t)s.box[2], (uint32_t)s.box[3], atlas, corners, 0xFFFFFFFFu, TRANSLUCENCY_NORMAL);
		}
	}

	void ShapePaneGpu_StateFields()
	{
		bool const valid = AtlasIsOurs();

		// drawn: the panes the last frame drew, or none when no frame has drawn any lately (the picker closed, its hook let go); by time, so a state line said
		// twice in one frame says the same both times
		uint32_t const drawnNow = lastDrawnAt != 0 && GetTickCount64() - lastDrawnAt <= kDrawnLately ? drawnLast : 0;
		StateLine_Add("pickerpanetime", "cpu " + std::to_string((unsigned long)cpuUs) + " gpu " + (gpuKnown ? std::to_string((unsigned long)gpuUs) : std::string("unknown")) +
			" most " + std::to_string((unsigned long)mostUs));
		StateLine_Add("pickerpanedraw", "live " + std::string(notLive.empty() && shader && valid ? "1" : "0") + " drawn " + std::to_string((unsigned long)drawnNow) +
			" frame " + std::to_string((unsigned long long)framesDrawn) + " atlas " + (atlasMade ? std::to_string((unsigned long long)atlas) : std::string("-1")) + " valid " +
			(valid ? "1" : "0") + (notLive.empty() ? std::string() : " why " + notLive));
	}

	void ShapePaneGpu_RefuseQueries()
	{
		queriesRefused = true;
	}
}
