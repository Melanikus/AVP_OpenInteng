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

	// TYPED PLACEMENT (Dietrich 2026-11-23, Bishop's card for beta.0; the owner: "Yes, set the known type"). A new object of a KNOWN shape is made as the game
	// object its kind says, not as a static one. This says which: the game's Type and ExtraData for those kind words.
	// THE CLOSED LIST: only the pickups Steam's 63 levels place, measured by AvP-docs tools/dietrich/kind-fields-survey.js on 2026-11-23:
	//   - health (Type 4), armour (5) and field charge (15), ExtraData 0;
	//   - a weapon (Type 2): pulse rifle 0, smartgun 2, flamethrower 3, SADAR 5, grenade launcher 6, minigun 7, marine pistol 22, frisbee launcher 23;
	//   - its ammo (Type 3): pulse rifle 0, smartgun 2, flamethrower 3, SADAR 5, grenade launcher 6, minigun 7, pulse grenade 8.
	// Every one of those stock placements carries the fields the maker gives a new object anyway: mass 50, integrity 2, flags 1, no target (object_make.h).
	// The kind must be EXACTLY the words ShapeExtract_KindWords gives one of them. Any other words give false: "static", "furniture", "unknown", "special pickup"
	// (its stock placement needs a target in its own level), a light, a track, words from anywhere else. Then the object stays static (Type 0).
	// So whatever words come in, only a pair from this list ever comes out.
	struct ShapePlacedAs
	{
		int32_t type = 0;    // the AVPSTRAT's Type (its payload's +20)
		int32_t extra = 0;   // its ExtraData (+24)
	};

	bool ShapeIdentity_PlacedAs(std::string const &kind, ShapePlacedAs *as);

	// What a new object of that row's shape is made as, in words for the status line and the placement picker (null: a shape the table doesn't know):
	//   "placed as health (the game's Type 4, ExtraData 0)"
	//   "placed as static (Type 0), as stock places it"                                   (the kind is "static")
	//   "placed as static (Type 0): its stock kind is furniture, which placement doesn't set"   (any other kind)
	//   "placed as static (Type 0): no stock object places it"                          (a row with no kind)
	//   "placed as static (Type 0): its shape is not one the editor knows"               (null)
	std::string ShapeIdentity_PlacedAsWords(ShapeIdentityRow const *row);
}
