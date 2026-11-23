// shapes_trigger.h: THE SEARCH OF NEW MAPS FOR OBJECT SHAPES, one map a frame (the owner, 2026-11-23: "whenever any new map not in its ini is found it'll scan
// it too"; the spec: AvP-docs/director/plan-first-launch-shapes.md, "The owner's new rule", 2 to 4). Written 2026-11-23 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Pure: the reading and the search are given to it (the host gives
// Frost's ShapeExtract_ReadMapFile and ShapeExtract_LevelToFolder), so it is tested without files.
//
// WHAT IT DOES with the maps found (every .rif under the data folder's avp_rifs and avp_huds, and a level the editor opens, in a fixed order) and the list
// already kept (shapes_scanned.h): each Step takes ONE map, so the editor keeps drawing:
//   - its SHA-256 (the map read as hostile input); a map that can't be read is named, and not put on the list (it has no SHA-256);
//   - on the list already, or met earlier in this run (two copies of one map): nothing more;
//   - else it is searched (shapes written into shape_rifs, never over a file there), and its SHA-256 goes on the list, even when the search failed (a
//     map cut short or with a bad chunk), so it isn't searched again at every start: a changed map has another SHA-256, so it is searched again then;
//   - the list holds at most kShapesScannedMost: a new map past that is named, not searched.
// At the end, the list to write (when it changed) and the status line's words: "Shapes: 2 new maps scanned, 5 new shapes in shape_rifs", with the maps
// that failed or couldn't be read named once.
#pragma once

#include <stddef.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "shapes_scanned.h"

namespace host
{
	// What one map's search gave (the host fills it from Frost's ShapeLevelReport)
	struct ShapesMapSearch
	{
		size_t written = 0;        // new shape files written into shape_rifs
		bool failed = false;       // the search failed (why says how); the map still goes on the list
		std::string why;
	};

	struct ShapesTriggerHooks
	{
		// (const FIRST in these two template arguments: Snyk Code's parser can't read "std::string const &" there and leaves the whole file out)
		std::function<std::string(const std::string &path, std::string *sha256)> read;   // "" with *sha256 (64 small hex) when read, else why not
		std::function<ShapesMapSearch(const std::string &path)> search;                 // the map searched for shapes
	};

	class ShapesTrigger
	{
	public:
		// The list as the ini holds it, and the maps found, in the order they are to be taken
		void Begin(ShapesScannedList const &list, std::vector<std::string> const &paths);

		// One map; false when every map has been taken (then nothing happens)
		bool Step(ShapesTriggerHooks const &hooks);

		// A MAP FOUND LATER (a level the editor opens after the start: Open, Open with, a reopen; Frost 11-23, the spec's 2): taken after the maps still to be
		// taken, by the same rules (read as hostile input, its SHA-256 judged). A path still waiting is not added twice; one already taken is added again, since
		// a level saved since has another SHA-256 (the owner: a changed map is searched again). Done() is false again until it is taken.
		void Add(std::string const &path);

		bool Done() const;
		bool Changed() const;                                // a map was added: the ini's list is to be written
		std::vector<ShapesScannedEntry> const &List() const;   // the list, the maps added after the old ones
		std::string Words() const;                           // the status line's words at the end ("" when nothing new was met)
		size_t Scanned() const;
		size_t Written() const;

	private:
		std::vector<ShapesScannedEntry> list;
		std::set<std::string> known;
		std::vector<std::string> paths;
		size_t next = 0;
		size_t scanned = 0;
		size_t written = 0;
		bool changed = false;
		std::vector<std::string> named;   // the maps that failed, couldn't be read, or didn't fit on the list: "<file>: <why>"
	};
}
