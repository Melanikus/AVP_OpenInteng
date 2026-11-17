// autolink_views.h: AutoLink's cameras for one portal and the read-back of one picture, as inteng's FUN_004c7ab0 places them and FUN_004c6fb0 reads them
// (AvP-docs\engine\inteng\reassess\autolink-pictures-drake.md AP-1..AP-8). Pure functions: no file, no device, safe on any thread.
// Written 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "autolink_portal.h"
#include "autolink_rules.h"

namespace host
{
	// One camera: its place in inteng's units, its matrix (16.16 rows: the camera's right, down and forward in the level), and which of the portal's views it is
	struct AutoLinkView
	{
		int32_t place[3];
		int32_t turn[9];
		int32_t step;   // 0 the base (0x4c7f59), 1-4 the 70-degree turns T1..T4 (0x4c803b / 0x4c811d / 0x4c81f1 / 0x4c82c5), 5-6 the shifted views (x and z contacts only: 0x4c83f3 or 0x4c859f, 0x4c866f)
	};

	// The views of one portal (FUN_004c7ab0 0x4c7dad-0x4c866f), in inteng's order: 5 for a contact on y, 7 on x or z
	void AutoLink_ViewsFor(Portal const &portal, std::vector<AutoLinkView> *views);

	// The 320 x 240 picture's size (FUN_0057f520's clip, 0x4c6c29)
	int32_t const kAutoLinkPictureWidth = autolink_rules::kPictureWidth;
	int32_t const kAutoLinkPictureHeight = autolink_rules::kPictureHeight;

	// The modules a picture shows (FUN_004c6fb0 0x4c706f-0x4c7110): a pixel's low 24 bits are the ID (module number + 1), 0 is nothing. A module is seen when some pixel starts a 3 x 3 block
	// (rows r..r+2, columns c..c+2) all of its ID; the starts are the 318 x 238 that keep the block inside. *seenNumbers gets the module NUMBERS (ID - 1), in rising order. False, with
	// *seenNumbers empty, when a pixel holds an ID above moduleCount ("unexpected colour found": the view links nothing), or the picture is not 320 x 240.
	bool AutoLink_Seen(std::vector<uint32_t> const &pixels, int32_t moduleCount, std::vector<int32_t> *seenNumbers);
}
