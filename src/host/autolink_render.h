// autolink_render.h: AutoLink's pictures drawn by Direct3D 9 (AvP-docs\engine\inteng\reassess\autolink-pictures-drake.md AP-7, AP-9; Bishop's plan: our D3D9 renderer, read back in 32-bit;
// the owner's ruling: AutoLink must not hold up the view). The renderer has a Direct3D 9 device of its OWN, on a hidden window of its own: it shares nothing with the editor's view, so the
// thread that owns it (AutoLink's worker, or the frame within a budget) can draw while the view goes on. One thread at a time uses one renderer.
// Written 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES: FUN_004c6380 draws every module that takes part with its polygons flat in its number + 1, through the 3dc view block FUN_0057f520 sets: a screen of 320 x 240, centre
// (160, 120), 160 pixels to the unit of depth (90 degrees across, square pixels); FUN_004c6fb0 reads the pixels back. WHAT THIS DOES: the same picture on the card: each module's polygons
// (fans of its corners) in one vertex buffer made once per scene, each picture the camera of an AutoLinkView, the modules of shown[] drawn, a depth buffer, no culling, no light, no texture,
// the colour the ID; read back with GetRenderTargetData in 32-bit. NAMED: the near plane (100 units, 1 mm: the 3dc engine's near clip is not read), Direct3D's pixel centres (a pixel is
// drawn when its centre is inside the triangle; inteng's own rasteriser is not read), the far plane (the farthest corner of the picture, so the depth buffer's steps are as small as they can be).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "autolink_portal.h"
#include "autolink_views.h"

namespace host
{
	struct AutoLinkScene;

	// The most records Load takes (a picture's IDs are 24-bit: this keeps every record's own)
	size_t const kAutoLinkMostRecords = 1000000;

	class AutoLinkRenderer
	{
	public:
		AutoLinkRenderer();
		~AutoLinkRenderer();

		// Makes the device on a hidden window of its own (call from the thread that will draw). False, with *why, when the card or Direct3D 9 can't give one.
		bool Open(std::string *why);
		void Close();

		// The scene's polygons into the vertex buffer: every module of scene.list, in its order (module n is drawn in n + 1). False with *why when they can't be held (too many, a bad
		// point number is left out and counted in *leftOut).
		bool Load(AutoLinkScene const &scene, size_t *leftOut, std::string *why);

		// The same for AutoLink's RECORDS (Dietrich's driver, Vasquez's AutoLinkRecords): record n (its place and its shape's points, inteng's units) drawn in n + 1: the scene's modules in file
		// order, then one stand-in box per door. Picture's shown[] then covers every record, stand-ins included. False with *why for more than kAutoLinkMostRecords records (the IDs are 24-bit; no level comes near).
		bool Load(std::vector<PortalModule> const &records, size_t *leftOut, std::string *why);

		// One picture from the view: record n drawn when shown[n] (a shorter shown[] leaves the rest out), 320 x 240 pixels into *pixels, row by row from the top, each the ID in its low 24
		// bits (0 nothing). False with *why when the device is lost and can't be had back, or nothing is loaded.
		bool Picture(AutoLinkView const &view, std::vector<uint8_t> const &shown, std::vector<uint32_t> *pixels, std::string *why);

		// How many records are loaded (scene.list's size, or the records')
		size_t ModuleCount() const;

	private:
		struct Parts;
		Parts *parts;

		AutoLinkRenderer(AutoLinkRenderer const &) = delete;
		AutoLinkRenderer &operator=(AutoLinkRenderer const &) = delete;
	};
}
