// shape_search.h: THE LOOKUP ORDER, where the editor finds a shape's file <name>.rif that the level doesn't hold yet (Quick-create's import: shape_import.h).
// Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng, Frost's piece of Option C (Bishop 2's order). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// THE ORDER, the first folder holding <name>.rif wins:
//   1. ShapeFolder= in AVP_OpenInteng.ini ([Editor]), when it is set: the mapper's own folder;
//   2. <game>\Tools\shape_rifs: the old tools' package, where the original install put it (only ever read);
//   3. <game>\shape_rifs: where Option C writes the shapes it makes from the game's own levels (plan-first-launch-shapes.md).
// (Before: ShapeFolder=, else shape_rifs beside the program. When the editor runs from <game>\Tools, as it belongs, that folder is 2.)
// That order is Quick-create's import. The outside shapes' SILENT REFRESH looks in 1 and 2 only, never 3 (ShapeSearch_RefreshOrder below says why).
// EACH FOLDER IS JUDGED AS IT IS LOOKED IN, never only at the start (Option C writes <game>\shape_rifs later in the same sitting):
//   - a folder that isn't there is passed over without a word;
//   - a folder is REFUSED when it is a link or a junction, a file, not a whole path on a drive (SaveFolder_WhyNotUsable, the data folder's own rule), on a
//     network drive (opening it would hand another computer the user's login), or, for 2 and 3, when a folder on the way down from the game's folder is a link
//     (GameFolder_WhyNotPlainBelow);
//   - in a folder that is taken, the file is judged and read on one handle (ShapeImport_Read: a plain file on a disk, not a link, one name, at most
//     kShapeImportMostBytes).
// FAIL CLOSED: a refused folder, or a file that is there but refused, STOPS the lookup with words naming that folder; it never falls through to a later one
// (so a refused ShapeFolder= is never silently replaced by the game's own shapes). Only a missing folder, or a folder without the file, goes on to the next.
#pragma once

#include <string>
#include <vector>

namespace host
{
	struct ShapeSearchFolder
	{
		std::string path;    // the folder, a whole path (no last slash)
		std::string label;   // what the status line calls it: the words never show a folder of the user's ("ShapeFolder=", "the game's shape_rifs")
		std::string under;   // the game's folder it is inside, checked plain from there down; "" for the user's own ShapeFolder= (the user's own folders)
	};

	// The order: setting (ShapeFolder=, as the ini writes it) when it isn't empty, then <gameFolder>\Tools\shape_rifs and <gameFolder>\shape_rifs when gameFolder
	// isn't empty. A folder that is the same as one before it (any capitals, either slash) is left out
	std::vector<ShapeSearchFolder> ShapeSearch_Order(std::string const &setting, std::string const &gameFolder);

	// THE REFRESH'S ORDER (Frost's ruling, 2026-11-23): the outside shapes' silent update (every level's open, Open with, Take scale: OutsideShapesUpdate) looks
	// in ShapeFolder= and <gameFolder>\Tools\shape_rifs only, NEVER in <gameFolder>\shape_rifs. Option C's files there are for importing a shape a level
	// doesn't hold. Option C writes a shape's FIRST variant under its plain name and the others as <name>_<level>.rif, so a refresh from that folder would
	// swap a level's own copy for another level's variant, silently, and the next save would keep it. A level's own copy is never swapped that way.
	// (A possible later step: read Option C's folder taking <name>_<level>.rif first for that level, then <name>.rif.)
	// AS BEFORE, named here, not changed: the old tools' package in Tools\shape_rifs matches 161 of the 192 outside shapes of the stock levels; the other 31
	// are shapes the level's editor changed afterwards, and the refresh brings those back to the package's (inteng's own load does the same).
	std::vector<ShapeSearchFolder> ShapeSearch_RefreshOrder(std::string const &setting, std::string const &gameFolder);

	// Why a folder can't be looked in, in words; "" when it can. *missing: true when it simply isn't there (it is passed over then, and the words are "")
	std::string ShapeSearch_FolderWhyNot(ShapeSearchFolder const &folder, bool *missing);

	enum class ShapeSearchResult
	{
		Found,           // *found is the folder that holds <name>.rif, and its file passes ShapeImport_Read
		BadName,         // the name isn't a plain one (shape_import.h): no folder is looked in
		NothingToLook,   // the order is empty (no ShapeFolder= and no game folder)
		NotFound,        // no folder of the order holds <name>.rif
		FolderRefused,   // a folder of the order that is there can't be used: the lookup stopped there
		FileRefused,     // <name>.rif is in a folder, but isn't a file this reads (a link, a second name, too big, ...): the lookup stopped there
	};

	// <name>.rif in the first folder of order that holds it. *found: that folder (Found), or the folder that stopped the lookup (FolderRefused, FileRefused).
	// *why: the words for the status line, naming folders by their labels only
	ShapeSearchResult ShapeSearch_Find(std::vector<ShapeSearchFolder> const &order, std::string const &name, ShapeSearchFolder *found, std::string *why);

	// The labels of the order, as words: "ShapeFolder=, the game's Tools\shape_rifs or the game's shape_rifs"
	std::string ShapeSearch_Labels(std::vector<ShapeSearchFolder> const &order);
}
