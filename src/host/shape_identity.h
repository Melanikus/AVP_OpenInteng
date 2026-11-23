// shape_identity.h: WHAT A SHAPE IS, known by its SHA-256 (Option C, the owner's design, 2026-11-23: "we extract those with a deterministic algorithm from
// stock file and... check their SHA and say 'This is a medpack' or 'a pulse rifle' based on extracted chunk signature"; "we don't want to say 'Oh doh, your
// medpack is this chair'"). Written 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT IS KNOWN: shape_extract.h makes each outside shape a stock level holds into its file, deterministically: the same stock bytes give the same file,
// so the same SHA-256, on every Steam install. The table (shape_identity_data.cpp) lists, for every such file made from Steam's own levels, its SHA-256, its
// name, and what the game makes of the objects placing it in those levels (shape_extract.h ShapeExtract_Placements), with the evidence ("health: 186 of 186
// placements"). EXACT MATCHES ONLY: a file whose SHA-256 is in the table is that shape; any other file (a stranger's map, an edited stock level) is UNKNOWN,
// and the mapper names it. No likeness is ever guessed, so a chair can never come out as a medpack. Many rows can name one object: every stock variant is
// its own row.
//
// ANCHORED: a row whose geometry (points, vertex and polygon normals, polygons, texture coordinates) is exactly that of one of Rebellion's own shape files
// we hold (the old tools' package, on our machine only) is "anchored": named by it, its name confirmed. The anchors stay on our side; only the rows ship.
// Every row is confirmed once at release time, by its evidence and a look.
//
// THE STOCK FILES: the same table lists the 63 levels of avp_rifs and the 23 files of avp_huds as Steam ships them, by size and SHA-256 (made from a copy
// checked against Steam's depot manifests outside the repo: the owner's ruling, "SHA-1 stays out of the repo"), so the first-launch scan can say which of a
// player's files are Steam's own bytes.
//
// MADE BY tests/scenarios/verify_shape_identities ("make" prints the table; "check" says whether the table in the build is what it would make), never by
// hand. Pure: no file, no Windows.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace host
{
	struct ShapeIdentityRow
	{
		char const *sha256;      // of the extracted file: 64 small hex letters
		char const *fileName;    // its plain name ("medbox01.rif"), or a conflict's (<name>_<level>.rif)
		char const *kind;        // the commonest kind of the objects placing it ("health"), or "" when no object places it
		char const *evidence;    // "186 of 186 placements", with any disagreement ("; furniture 4"), or "no object places it"
		bool anchored;           // its geometry is exactly one of Rebellion's own shape files' of that name
		char const *levels;      // the stock levels holding it, ';'-separated ("avp_rifs/Derelict.RIF;avp_rifs/E3demo.rif")
	};

	struct ShapeIdentityStockFile
	{
		char const *folder;      // "avp_rifs" or "avp_huds"
		char const *fileName;    // in small letters ("derelict.rif")
		uint64_t size;
		char const *sha256;
	};

	extern ShapeIdentityRow const kShapeIdentityRows[];
	extern size_t const kShapeIdentityRowCount;
	extern ShapeIdentityStockFile const kShapeIdentityStockFiles[];
	extern size_t const kShapeIdentityStockFileCount;

	// The row whose SHA-256 this is (64 small hex letters), or null: unknown
	ShapeIdentityRow const *ShapeIdentity_Find(std::string const &sha256);

	// The stock file of that folder and name (any capitals), or null when Steam ships no file of that name there
	ShapeIdentityStockFile const *ShapeIdentity_StockFile(std::string const &folder, std::string const &fileName);
}
