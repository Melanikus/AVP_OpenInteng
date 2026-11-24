// shapes_start.h: SHAPES PART 3, the player's maps found and searched for object shapes, on the disk. Written 2026-11-23 by Melanikus (Frost) for
// AVP_OpenInteng, on Bishop 2's order (alpha.6, with Vasquez's picker), to Drake's spec (AvP-docs/director/plan-first-launch-shapes.md, "The owner's new
// rule"). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY (the owner, 11-23): "Open Inteng will save a list of map shas that it already searched assets from in his ini, whenever any new map not in its ini is
// found it'll scan it too". The old avptools gave the community its object shapes (shape_rifs); Open Inteng makes them from the game's own maps instead
// (Option C, shape_extract.h), into <game>\shape_rifs, never over a file there.
// WHAT IS HERE (no editor state; tests/host_shapes_start_test.cpp drives it on real folders):
//   - THE MAPS FOUND: every .rif at the top of <data>\avp_rifs, <data>\avp_rifs\Custom, <data>\avp_rifs\OpenedByEditor and <data>\avp_huds, in that order;
//   - THE STOCK SHAPES MISSING from <data>\shape_rifs, by the identity table's names (one listing, nothing read);
//   - THE FOUND FORM'S FACTS and whether to ask (first_launch_ui.h);
//   - THE READ AND THE SEARCH for Drake's trigger (shapes_trigger.h): each map read once, hashed on that same handle, and those very bytes searched.
// The editor's part (the form, one map a frame, the list written, the level reopened) is shapes_start_screen.h.
#pragma once

#include <stddef.h>

#include <string>
#include <vector>

#include "first_launch_ui.h"   // FirstLaunchFacts
#include "shapes_trigger.h"    // ShapesTriggerHooks

namespace host
{
	size_t const kShapesStartMostAFolder = 4096;   // .rif files taken from one folder; any more are refused in one line (a folder may hold anything)

	// THE MAPS FOUND: each folder first held to the data folder's rule (game_folder.h GameFolder_WhyNotPlainBelow: every folder from the data folder down
	// plain, never a link or a junction on the way), then its files listed: a plain file (never a folder or a reparse point) whose name ends in ".rif" in any
	// capitals, by its long name (a short 8.3 name never counts), each folder's names in a fixed order (sorted, any case). A missing folder is no refusal.
	// refused: "<folder below the data folder>: why" for a folder not looked in, or names past kShapesStartMostAFolder, or a name Windows' code page can't say.
	std::vector<std::string> ShapesStart_MapsFound(std::string const &dataFolder, std::vector<std::string> *refused);

	// THE STOCK SHAPES MISSING: how many of the identity table's names (shape_identity.h, every stock outside shape) have no file of that name, in any capitals,
	// in <data>\shape_rifs. One listing; no file is read, so it says only that a name is there, not its bytes (the search, writing CREATE_NEW, judges bytes).
	// Every one of them when the folder isn't there or can't be listed
	size_t ShapesStart_StockShapesMissing(std::string const &dataFolder);

	// THE SHAPES THE LIST STANDS FOR (Drake 2026-11-23, Bishop's card for beta.0, the owner testing "like a fresh player would"): whether <data>\shape_rifs
	// holds at least one shape file (a plain file, never a folder or a link, whose name ends in ".rif" in any capitals). The ini's [ShapesScanned] list says
	// which maps were searched, but what they gave lives in that folder: a player who deletes it (or a search whose writes were all refused) would keep a list
	// saying "searched" over nothing, and no map would ever be searched again. False when the folder isn't there or holds no shape file; true when it can't be
	// listed (Windows refused: nothing is known, so the list stands, as it did), with *whyNotListed then saying why, for the editor's log
	bool ShapesStart_ShapeFilesThere(std::string const &dataFolder, std::string *whyNotListed = nullptr);

	// THE FOUND FORM'S FACTS (first_launch_ui.h FirstLaunchFacts): the stock check of the data folder (shape_extract_files.h ShapeExtract_StockCheck: its counts,
	// the stock files not Steam's own, the ones missing), and shapesToWrite: the stock shapes missing, plus one for each map found that isn't Steam's own (a
	// custom map, a stock name with other bytes: what shapes it holds is known only once it is searched). Nonzero: there is something to search.
	FirstLaunchFacts ShapesStart_Facts(std::string const &programFolder, std::string const &dataFolder, bool fromIni, std::vector<std::string> const &maps);

	// WHETHER THE FORM ASKS (Drake's spec; the owner: "Ask again next start"): only when no list is kept yet (the player never said Yes) and there is something
	// to search (shapesToWrite above). Once a list is kept, no form: a map not on it is searched without asking
	bool ShapesStart_ShouldAsk(bool listKept, FirstLaunchFacts const &facts);

	// THE READ AND THE SEARCH, the trigger's two hooks: read is ShapeExtract_ReadMapFile (a plain file on a disk, never a link, at most its most, hashed on the
	// handle it is read from), its bytes held; search is ShapeExtract_BytesToFolder on exactly those bytes, into shapeFolder, and then lets them go. A search
	// of a path other than the one just read (the trigger never asks one) is refused, never read again.
	class ShapesStartSearch
	{
	public:
		explicit ShapesStartSearch(std::string const &shapeFolder);
		ShapesStartSearch(ShapesStartSearch const &) = delete;              // (its hooks point at it: a copy would read into one and search the other)
		ShapesStartSearch &operator=(ShapesStartSearch const &) = delete;

		ShapesTriggerHooks Hooks();

	private:
		std::string shapeFolder;
		std::string heldPath;
		std::vector<char> held;
	};
}
