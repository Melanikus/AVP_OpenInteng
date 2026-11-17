// autolink_runner.h: AutoLink > Module / Environment run on a WORKER THREAD while the view goes on (the owner's ruling, Melanikus 19:1x: "Make sure ours is multithreaded by the way, so
// doesn't lock the view while pictures draw"; NAMED: inteng held the whole program). The worker owns the renderer (Drake's AutoLinkRenderer: a Direct3D 9 device of its own) and the records'
// shown flags, and works each module out with AutoLink_ModuleWork (autolink_run.h); the FRAME is the one writer of the link trees (AvP-docs\director\autolink-interfaces-2026-11-16.md
// sections 4, 5, 5b). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (FUN_004c8930, row ML-A-1, read on listing-all-2 0x4c8930-0x4c8ae0): the setup (FUN_004c6380: the records, every one drawn); for each record in NUMBER order that has a file
// object (the modules) and, given a list, is in it: a right press stops the loop (0x4c899c, checked at the TOP of each record); FUN_004c10c0(i, deleteOld, 0) makes it current (the old current
// written when it changed; i's links loaded from its VMDARRAY, or with deleteOld its VMDARRAY destroyed and its tree empty; changed put on, 0x4c14df); the counter "%03d" i; its pictures
// (autolink_run.h). After the loop: FUN_004c6dd0, the module kept made current again, the object list redrawn.
// WHAT THIS DOES: the same steps; the trees are held in memory and written ONCE at the end (AutoLink_WriteTrees: one image, one loader read; each module is made current once in a run, so
// writing it when left and reading it back is the same as holding it). NAMED: a module left with no link is not written (autolink_edit.h).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "autolink_edit.h"
#include "autolink_views.h"

class OpenLevelFile;

namespace host
{
	struct AutoLinkRunCounts
	{
		size_t modules = 0;       // modules made current
		size_t pictures = 0;      // pictures drawn
		size_t badPictures = 0;   // pictures with a bad pixel ("unexpected colour found")
		size_t linksAdded = 0;    // nodes the pictures put in the trees
		size_t written = 0;       // modules whose links were written
		size_t skipped = 0;       // modules with links that could not be written (no MODULEDT)
		int32_t lastObject = -1;  // the last module made current (Load from file leaves it current: FUN_004c8be0 does not make the old one current again)
	};

	class AutoLinkRunner
	{
	public:
		AutoLinkRunner();
		~AutoLinkRunner();   // a run still on is cancelled and its worker joined (nothing written)

		// FUN_004c8930's start, on the frame's thread: the records (AutoLink_Records), the modules to run (every module, or those whose object number is in picks, in NUMBER order), then the
		// worker. visibleDistance: Set Visibility's (DAT_00704378, metres x 100000; 0 cuts nothing). False with *why when nothing can run (no level, no module, a run already on).
		bool Start(OpenLevelFile *open, std::vector<int32_t> const *picks, bool deleteOld, double levelScale, int32_t visibleDistance, std::string *why);

		// LOAD FROM FILE's start (0xa3, FUN_004c8be0): the module file's records in its order, each a module (object number) and a view (AutoLink_ViewWork says how the record's numbers
		// become one); each is made current with its links KEPT (FUN_004c10c0(i, 0, 0), 0x4c8f0d), so a module named twice takes its second view into what the first gave (inteng writes
		// it when left and reads it back: here its tree is flattened and read back the same way). False with *why when nothing can run.
		struct FileView
		{
			int32_t object;
			AutoLinkView view;
		};

		bool StartFromFile(OpenLevelFile *open, std::vector<FileView> const &views, double levelScale, int32_t visibleDistance, std::string *why);

		bool Running() const;

		// The right press (inteng: checked at the top of each record): the worker stops before its next module; the module in hand is finished and every link made so far stays
		void Cancel();

		// What the frame shows: each module begun (the counter, record numbers), the pictures drawn since the last Step (at most kAutoLinkShowKept, the oldest let go), and whether the
		// worker is done (finished: then End)
		struct Frame
		{
			std::vector<int32_t> counters;
			std::vector<std::vector<uint32_t> > pictures;
			bool finished = false;
		};

		// The frame's step: everything the worker posted so far is put into the trees
		void Step(Frame *out);

		// After finished: the worker joined, every module's tree written (AutoLink_WriteTrees). *why says why the run stopped early (cancelled, a picture that could not be drawn) or why
		// the write failed.
		AutoLinkResult End(OpenLevelFile *open, AutoLinkRunCounts *counts, std::string *why);

	private:
		struct Parts;
		std::unique_ptr<Parts> parts;

		AutoLinkRunner(AutoLinkRunner const &) = delete;
		AutoLinkRunner &operator=(AutoLinkRunner const &) = delete;
	};

	size_t const kAutoLinkShowKept = 16;   // pictures waiting for the display, at most (the grid shows the latest; the read-back never waits for the screen)
}
