// autolink_run.h: AutoLink's DRIVER, the work behind Module Linking's AutoLink > Module / Environment (inteng's FUN_004c8930), split as the team agreed
// (AvP-docs\director\autolink-interfaces-2026-11-16.md sections 4, 5 and 5b): the WORKER part says which pictures are made, with which records drawn, and what
// each picture puts in the tree (no tree, no file, no device: any thread); the FRAME part is the one writer of the link trees. Written 2026-11-16 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (rows ML-A-1 and ML-A-7 of AvP-docs\engine\inteng\reassess\module-linking-map-dietrich.md, read on listing-all-2):
// - FUN_004c8930, for module a (made current by FUN_004c10c0(a, deleteOld, 0)): when a stand-in s stands for a (DAT_00704330[s] == a, the first such s from 0),
//   s is hidden and a drawn while FUN_004c7ab0 runs, then a hidden and s drawn, and they stay so (0x4c8a33-0x4c8a91).
// - FUN_004c7ab0: for each record b, in number order, that is not a and has a file object (+0x154: the stand-ins have none), and that meets a (FUN_004c4750),
//   each of the portal's views calls FUN_004c6fb0(0, a copy of the empty list DAT_00704348) (0x4c7d5b-0x4c7da3, 0x4c7f58).
// - FUN_004c6fb0(door, handled) for one view: the picture is drawn and read (a bad pixel: "unexpected colour found", it leaves at 0x4c729a: nothing more);
//   each record i seen, in number order: with Set Visibility's distance != 0, cut when a gap between a's box and i's box is above it (0x4c7317-0x4c73d2,
//   AutoLink_DistanceCut); a stand-in (DAT_00704330[i] > -1) puts its DOOR in the tree and, when i is not in `handled` yet, i is added to `handled` and to
//   this picture's door list (0x4c7444-0x4c7530); any other record goes in the tree itself. Then, for each stand-in s of the door list in order: s hidden,
//   its door drawn, FUN_004c6fb0(door, a copy of handled) on the SAME view, then s drawn and the door hidden again (0x4c76ea-0x4c7929). Last, a door picture
//   that made or reused a branch closes it (0x4c7952-0x4c7963).
// INTENG BUGS, FIXED (MAPPING-ORDERS section 15; row ML-A-7): the door picture finds its door with the insertion's walk (module_links.h), and a picture with a bad pixel still
// ends with the Close of a door branch it re-opened (AutoLinkTreeWriter).
// So what is drawn depends only on the geometry and these flags, never on the trees: the worker works it out alone. Its EVENTS, one per FUN_004c6fb0 call and one
// at that call's end, carry what the frame must do to the tree of a, in inteng's order (LinkTree::AutoLinkBegin / AutoLinkAdd / AutoLinkEnd).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "autolink_portal.h"
#include "autolink_views.h"
#include "module_links.h"

namespace host
{
	// One FUN_004c6fb0 call (end false), or its end (end true)
	struct AutoLinkEvent
	{
		int32_t door;                 // -1: a picture of a view; else the record of the door whose second picture this is
		bool end;                     // true: the call's end (the door's Close Branch when it made or reused a branch)
		bool badPixel;                // the picture held an ID above the record count: nothing put in the tree, no door steps (its end still closes a re-opened door branch: INTENG BUG, FIXED)
		std::vector<int32_t> insert;  // the records to put in the tree, in order (a stand-in already turned into its door)
	};

	// Draws one view with the records whose shown flag is 1 and gives the 320 x 240 picture of IDs (record number + 1, 0 nothing). False: it could not draw (*why).
	typedef std::function<bool(AutoLinkView const &view, std::vector<uint8_t> const &shown, std::vector<uint32_t> *pixels, std::string *why)> AutoLinkPictureMaker;

	// FUN_004c6fb0 0x4c7317-0x4c73d2 (only called with Set Visibility's distance != 0): true when record b is CUT. Per axis, the gap is taken by the PLACES: b's place below a's,
	// (a's place + a's least) - (b's place + b's most); else (a tie too) (b's place + b's least) - (a's place + a's most); cut when any gap is above the distance (signed; a
	// negative distance cuts nearly everything). In 32-bit whole numbers that wrap, as inteng's.
	bool AutoLink_DistanceCut(PortalModule const &a, PortalModule const &b, int32_t visibleDistance);

	// The work for record a of a run, after it was made current (FUN_004c8930 0x4c8a2d-0x4c8a91 with FUN_004c7ab0 and FUN_004c6fb0). records: every record (the modules,
	// then the stand-ins); standsFor[n]: -1 for a module (it has a file object), else the door's record (DAT_00704330). shown: every record's drawn flag, kept across the
	// run (all 1 at its start: FUN_004c6380 0x4c6c41-0x4c6c66). The events are appended; *pictures counts the pictures drawn. False when a picture could not be drawn (*why):
	// the events of the pictures before it are kept.
	bool AutoLink_ModuleWork(std::vector<PortalModule> const &records, std::vector<int32_t> const &standsFor, int32_t a, int32_t visibleDistance, std::vector<uint8_t> *shown,
		AutoLinkPictureMaker const &picture, std::vector<AutoLinkEvent> *events, size_t *pictures, std::string *why);

	// Load from file's work for one record of the module file, after record a was made current (FUN_004c8be0 0x4c8f25-0x4c905a): a's door flags as the driver's, ONE picture from the
	// record's view (FUN_004c6fb0(0, the empty list), with its door steps), the flags back. The view: the record's place x 100 and the TRANSPOSE of its matrix (FUN_004b3710 swaps
	// 1/3, 2/6, 5/7 before the camera's +0x38 takes it, 0x4c8dd4; AutoLinkView.turn is +0x38 itself, as FUN_004b34b0 writes it for AutoLink's own views, 0x4c7f78).
	bool AutoLink_ViewWork(std::vector<PortalModule> const &records, std::vector<int32_t> const &standsFor, int32_t a, AutoLinkView const &view, int32_t visibleDistance,
		std::vector<uint8_t> *shown, AutoLinkPictureMaker const &picture, std::vector<AutoLinkEvent> *events, size_t *pictures, std::string *why);

	// The FRAME's part: the events of record a, in order, put into a's tree (its records as object numbers: objectIndexOf). Keeps the pictures begun and not ended (a door's
	// second picture runs inside the picture that met the door).
	class AutoLinkTreeWriter
	{
	public:
		// *added counts the nodes made. False when the event does not fit (an end with no picture begun, a record out of range): nothing is changed by it.
		bool Apply(LinkTree *tree, AutoLinkEvent const &event, std::vector<int32_t> const &objectIndexOf, size_t *added);

		// True when every picture begun has ended
		bool Balanced() const;

	private:
		struct Begun
		{
			LinkTree::AutoLinkPicture picture;
		};

		std::vector<Begun> begun;
	};
}
