// shape_extract_files.h: Option C on the disk: a map read as hostile input, and its shapes written into the game's shape_rifs folder. Written
// 2026-11-23 by Melanikus (Frost, the plan; Dietrich, the read) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The owner's rule (2026-11-23, AvP-docs director/plan-first-launch-shapes.md "The owner's new rule"): every map not yet searched is scanned, the maps
// of avp_rifs and avp_huds at start and every level opened, one map a frame. The first-launch screen keeps the list of maps searched (their SHA-256);
// this file is the one call it makes per map. The pure work is shape_extract.h's ShapeExtract_ScanMap.
#pragma once

#include <string>
#include <vector>

#include "shape_extract.h"

namespace host
{
	// A map's file read as HOSTILE input (Dietrich's All stock maps rules): opened without following a link, judged on that one handle (a file on a disk,
	// not a folder or a reparse point, one name only, 1 byte to 16 MB), read whole, and its SHA-256 worked out from those very bytes. "" when read, else
	// why not, in plain words ("missing" when there is no such file).
	std::string ShapeExtract_ReadMapFile(std::string const &path, std::vector<char> *bytes, std::string *sha256);

	// What one map gave
	struct ShapeLevelReport
	{
		bool read = false;                       // the bytes were read: sha256 is good, so the map goes on the list even when it failed
		std::string sha256;
		std::vector<std::string> written;        // the shape files written (new), by name
		std::vector<std::string> alreadyThere;   // shapes whose very bytes were there already, by name
		std::vector<std::string> skipped;        // copies not made, each "<name>: why"
		bool failed = false;                     // the map couldn't be read or scanned, or a shape couldn't be written
		std::string why;                         // when failed, why
		size_t filesRead = 0;                    // files of the shape folder read this call: only those of a shape's size, each at most once (Vasquez, 11-23)
	};

	// THE WRITER (the part of the one call after the read): a scanned map's shapes (shape_extract.h ShapeExtract_ScanMap) written into shapeFolder, as
	// ShapeExtract_LevelToFolder says below. mapName: the map file's own name ("Genshd1.RIF"), for a conflict's name and the provenance.
	bool ShapeExtract_WriteMapShapes(ShapeExtractMap const &map, std::string const &mapName, std::string const &shapeFolder, ShapeLevelReport *report);

	// THE ONE CALL PER MAP (Drake's trigger): the map at levelPath read (ShapeExtract_ReadMapFile), scanned (ShapeExtract_ScanMap), and each of its
	// shapes written CREATE_NEW into shapeFolder (the game's shape_rifs): the same name with the same bytes there already, nothing; the same name with
	// other bytes, <name>_<level> (then _2, ...); never over a file. The folder is refused if it is a link or a junction. Each shape is read back by the
	// import (ShapeImport_Build) before it is written. Each written shape gets its line in shapeFolder\shapes.txt (name, SHA-256, what it is from the
	// identity table or "unknown", the level and its SHA-256). False when the map failed (report->why says why). OUT OF MEMORY anywhere in it (a 32-bit
	// editor, a map at the limits: Vasquez's review, 11-23) fails the map in words, never the editor; report->read stays as it was (the bytes read or not).
	bool ShapeExtract_LevelToFolder(std::string const &levelPath, std::string const &shapeFolder, ShapeLevelReport *report);

	// THE SAME, FOR A MAP READ ALREADY (shapes part 3, Frost 11-23: the start's search, shapes_start.h): bytes the caller read with ShapeExtract_ReadMapFile
	// and hashed on that same handle, so the map searched is the very one whose SHA-256 goes on the list, with no second read in between. mapName: the
	// map file's own name ("Genshd1.RIF"). Scanned and written as ShapeExtract_LevelToFolder says; out of memory fails the map in words, as there.
	bool ShapeExtract_BytesToFolder(std::string const &mapName, std::vector<char> const &bytes, std::string const &shapeFolder, ShapeLevelReport *report);

	// THE STOCK CHECK (Drake's FOUND form: "63 levels and 23 HUD files; 2 aren't Steam's own: derelict.RIF, genshd1.RIF. Missing: hangar.RIF"): what a
	// data folder's avp_rifs and avp_huds hold, against the stock files of the identity table (shape_identity.h ShapeIdentity_StockFile)
	struct ShapeStockCheck
	{
		size_t levels = 0, levelsOwn = 0;      // avp_rifs: the .rif files read, and of them the ones that are Steam's own bytes
		size_t huds = 0, hudsOwn = 0;          // avp_huds: the same
		std::vector<std::string> notOwn;       // read, named as a stock file is, but not Steam's bytes: each by its name as found ("derelict.RIF")
		std::vector<std::string> missing;      // a stock file with no file of its name there: each by the table's name ("hangar.rif")
		std::vector<std::string> refused;      // "<folder>\\<name>: why": a .rif that couldn't be read (ShapeExtract_ReadMapFile's words), or a folder that can't be listed
	};

	// For avp_rifs and avp_huds of dataFolder: each .rif at the top level (a name ending in .rif, any capitals; folders under them are not looked in) read
	// with ShapeExtract_ReadMapFile, as the hostile input it is. A file is Steam's OWN when the table has a stock file of that folder and name AND its
	// size and SHA-256 are that row's. A file the table has no row for is no stock file: it is counted (levels or huds), never in notOwn. A refused one is
	// counted nowhere but refused, and its name is not missing (it is there). A folder that is missing has every stock file of it missing; one that is a
	// link or a junction is refused and not looked in. At most kShapeStockMostFiles .rif files a folder are read (a folder may hold anything): the rest
	// are refused in one line. Nothing is written.
	size_t const kShapeStockMostFiles = 4096;
	ShapeStockCheck ShapeExtract_StockCheck(std::string const &dataFolder);
}
