// texture_loop_waits.h: CAM-9's rule as a plain sum: which of Texture Map's waits inteng runs as a loop of its own (waiting in FUN_0058b700), so its WinMain never gets round
// to Home and Insert (0x58cfe8 / 0x58cff6) while one is up. Read from inteng.exe's switch table 0x550ca4 of FUN_0054d260 (AvP-docs/tools/drake/camera/tm-command-cases.js):
// the texture chooser 0x2a FUN_005575a0 and the repeats grid 0x10f FUN_00557c00 (both the chooser's flag), Wrap / Tile's plane polygon 0x2e FUN_0055e400 / 0x2f
// FUN_0055e5d0, Get UV coords 0x31 and Get Anim 0xb3 FUN_0055c210, Anim's Select 0x9d FUN_005406a0, the base polygon 0xe7 FUN_00556ff0 and Invert's object 0x101 (its own
// wait). NOT Copy Texturing's Object (0xb5, only sets DAT_00914d80) or Multiple (0x13f / 0x140): inteng keeps those in its frame, so they have no place here.
// Written 2026-11-19 by Melanikus (Drake) for AVP_OpenInteng (CAM-9's unit). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#pragma once

namespace host
{
	struct TextureLoopWaitState
	{
		bool chooser = false;           // textureChooserOn: the texture chooser or Select Number of Repeats' grid
		int planeMode = 0;              // texturePlaneMode: Wrap's (1) or Tile's (2) plane polygon to be chosen
		bool getUv = false;             // textureCopyPending: Get UV coords from which polygon
		bool getAnim = false;           // textureAnimCopyPick: Get Anim from which polygon
		bool animSelect = false;        // textureAnimPick: Anim's Select
		bool basePolygon = false;       // textureBasePick: Shift+B's base polygon
		bool invertObject = false;      // textureInvertPending: Invert's object
	};

	inline bool TextureLoopWaits(TextureLoopWaitState const &s)
	{
		return s.chooser || s.planeMode != 0 || s.getUv || s.getAnim || s.animSelect || s.basePolygon || s.invertObject;
	}
}
