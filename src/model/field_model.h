// The field model: what each number in a level file means, where it is, and what values it may take. Written 2026-11-14 by Melanikus
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT THIS IS. A level (.rif) is a tree of chunks. Each chunk has an 8-letter name, its size, and then its "payload": the numbers it
// holds. This file describes those numbers for the chunks a mapper edits: for each one its place in the payload, how it's stored, what
// it measures, the values the game accepts and why, and the value the old editor (Rebellion's inteng, 2000) gave a new one. The
// editor's panels are drawn from these tables, and every value a mapper types is checked against them before it's stored.
//
// WHERE THE FACTS COME FROM. Every layout and limit here was read from the game's own loader at the pinned AvPx commit, or, for what
// the old editor did, from a study of that editor's program. Each table entry says which. Nothing here reads a file or uses any of
// the game's globals: these are plain tables and small functions, so they can be tested on their own.
//
// HOW TO READ A NUMBER'S PLACE. "offset" counts bytes from the start of the payload. Some chunks hold lists whose length is in the
// file, so a field after a list has no fixed offset from the start. Those fields say where they're counted from instead (Anchor).
//
// ALL NUMBERS ARE LITTLE-ENDIAN, as the game wrote them on the PC.

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace model
{
	// How a field's bytes are stored.
	enum class Storage
	{
		Int32,      // a signed 32-bit whole number
		UInt32,     // an unsigned 32-bit whole number (usually a set of flags)
		Int16,      // a signed 16-bit whole number
		UInt16,     // an unsigned 16-bit whole number
		Byte,       // one unsigned byte
		Float32,    // a 32-bit floating-point number
		Float64,    // a 64-bit floating-point number
		ObjectId,   // 8 bytes naming another object in the level (two 32-bit numbers); 0,0 means "no object"
		IntTriple,  // three signed 32-bit numbers: a position or a corner of a box, x then y then z
		IntMatrix,  // nine signed 32-bit numbers: an orientation, row by row (the game's ChunkMatrix). Set by turning, not by number
	};

	// What a stored number measures, so the editor can show it in words a person understands.
	enum class Unit
	{
		Plain,              // a count, an index or a kind: no unit
		Flags,              // a set of on/off bits (BitName lists them)
		Choice,             // one of a list of named values (ChoiceName lists them)
		GameUnits,          // the game's distance unit, a millimetre, before the level's own scale
		TenthsOfASecond,    // 10 is one second
		Seconds,            // whole seconds
		Fixed16Seconds,     // seconds as 16.16 fixed point: 65536 is one second
		Percent,            // 100 is the normal amount
		HundredGrams,       // a mass: 10 is one kilogram
		TextNumber,         // the number of one of the level's text messages
		Fixed16,            // an amount as 16.16 fixed point: 65536 is one (a light's brightness: 65536 is 100%)
		Angle4096,          // an angle: 4096 is a full turn
		Milliseconds,       // thousandths of a second
		Pitch128,           // a change of pitch: 128 is a semitone, 1,536 an octave
		LightBrightness,    // a light's brightness: 65536 is 100%; inteng's light shows it as a percentage with two decimals (Brightness: %-6.2f%%, FUN_004ac2f0: value x 1/65536 x 100)
		LightSpread,        // a light's spread: 4096 is a full turn; inteng shows it in degrees with two decimals (Spread: %-6.2f deg: value x 0.087890625 = 360/4096)
		LightRange,         // a light's range in game units; inteng shows it in metres with two decimals (Range: %-6.2fm: the memory unit x 0.00001, the memory unit being the file's number x 100 x the level's scale)
		ListCount,          // how many items a list holds: it changes only when items are added or taken away, never by a number
	};

	// Who says which values are allowed. The editor refuses any value outside the range in every case, but the reason it gives differs.
	enum class Limit
	{
		None,            // the game takes any value here, and so does the editor
		Loader,          // the game's loader, at the pinned AvPx commit, refuses a level with a value outside the range
		LoaderPlanned,   // a loader fix that's written but not yet in the pinned commit will refuse it; the editor already does
		GameMeaning,     // the game takes any value, but only these mean something (others are clamped, or read as something else)
		Editor,          // the game would take more, but the editor keeps to the range the old editor or the game's code supports
	};

	// Where a field's offset is counted from.
	enum class Anchor
	{
		Start,           // bytes from the start of the payload
		AfterFirstList,  // bytes from the end of the chunk's first list (a strategy's: the list whose count is the kind's firstListCount
		                 // field; any other chunk's: the list whose count is at the kind's listCountAt)
		End,             // bytes back from the end of the payload: the field starts this many bytes before the end
	};

	// One number in a chunk's payload.
	struct Field
	{
		char const *name;     // the game's own name for it, as in its code
		char const *label;    // what the old editor called it ("" when it showed no label)
		Anchor anchor;
		unsigned offset;      // see Anchor
		Storage storage;
		Unit unit;
		uint32_t mask;        // 0: the whole number is the value. Otherwise only these bits are (the rest is kept as it was)
		Limit limit;
		double lowest;        // the smallest value allowed, as stored (after the mask), when limit isn't None
		double highest;       // the largest
		char const *why;      // in plain words: why the range is what it is (shown when a value is refused, and tested)
		bool hasCreation;     // whether the old editor gave a new one a known value
		double creation;      // that value, as stored
		char const *source;   // where the layout and the limit come from
		unsigned step;        // 0: any value in the range; else the value has to be a multiple of this (the old editor takes whole seconds for a fan: 65536)
		unsigned shift;       // for a masked field only: how far up the word the masked bits sit, and the value is those bits shifted down (0: the bits as they are, as every older field)
	};

	// A name for one bit of a field whose unit is Flags. Field names repeat across kinds ("flags" means something different in each),
	// so a name belongs to one kind and one field.
	struct BitName
	{
		int32_t kind;         // the strategy kind's number
		char const *field;    // the Field's name
		uint32_t bit;
		char const *label;    // in words, as the old editor showed it where it did
		bool alwaysSet;       // the editor always sets this bit when it saves (it's not a choice for the mapper)
	};

	// A name for one value of a field whose unit is Choice. A Choice field's dropdown offers exactly these values.
	struct ChoiceName
	{
		int32_t kind;
		char const *field;
		int32_t value;        // as stored (for a masked field, with its mask applied but not shifted)
		char const *label;
	};

	// How a strategy's size follows from what's in it. The game refuses a strategy whose payload isn't exactly that size.
	enum class SizeRule
	{
		Fixed,             // always fixedBytes
		AnySize,           // the game reads it as a plain block of any size (at least the 20 bytes every strategy has)
		Lift,              // fixedBytes, plus 8 per linked lift and 28 per lift elsewhere
		LinkSwitch,        // fixedBytes, plus 8 per linked switch
		MultiSwitch,       // fixedBytes, plus 12 per target and 8 per linked switch
		MissionObjective,  // fixedBytes, plus 12 per mission it alters and 12 per request target
		PointEffects,      // fixedBytes, plus 12 per point effect and 16 per target of each
	};

	// One kind of strategy (the AVPSTRAT chunk): the behaviour given to an object.
	struct StrategyKind
	{
		int32_t number;          // the kind's number, stored at payload +8
		char const *name;        // the game's name for the kind
		char const *label;       // the old editor's name for it
		SizeRule size;
		unsigned fixedBytes;     // the payload's size with every list empty
		Field const *fields;     // the kind's own fields (the fields every strategy shares are in StrategyBaseFields)
		size_t fieldCount;
		bool offered;            // the old editor let a mapper make one
		bool inStockLevels;      // at least one of the game's own levels has one
		int32_t writtenAs;       // the number the old editor writes for a new one (Airlock and LiftNoTel are written as a Lift)
	};

	// ---- The tables (strategy_fields.cpp) ----

	// The fields at the start of every strategy's payload.
	extern Field const StrategyBaseFields[];
	extern size_t const StrategyBaseFieldCount;

	extern StrategyKind const StrategyKinds[];
	extern size_t const StrategyKindCount;

	extern BitName const StrategyBits[];
	extern size_t const StrategyBitCount;

	extern ChoiceName const StrategyChoices[];
	extern size_t const StrategyChoiceCount;

	// The kind with that number, or 0 if the editor doesn't know it.
	StrategyKind const *FindStrategyKind(int32_t number);

	// Whether value is one of the named values of that kind's Choice field (what its dropdown offers).
	bool ChoiceIsNamed(int32_t kind, char const *field, int32_t value);

	// ---- The rules (strategy_rules.cpp) ----

	// What the size check found: the size is right, or the one reason it isn't. Each reason is a rule of the game's own loader, and
	// each has its own test, so a later rule can never hide a missing one by refusing the same file for a different reason.
	enum class SizeCheck
	{
		Right,          // the payload is exactly the size its kind and its counts add up to
		TooShort,       // shorter than the 20 bytes every strategy has
		CountCutOff,    // the payload ends inside one of its lists' counts
		CountNegative,  // a list's count is below zero
		CountTooBig,    // a list's count says more items than the rest of the payload could hold
		WrongSize,      // the counts are all readable, but the payload isn't the size they add up to
		NameCutOff,     // a name the chunk ends with runs to the end of the payload without its ending zero (chunks only)
	};

	// Checks a strategy's size exactly as the game's loader does. payload is the AVPSTRAT chunk's payload (after its 12-byte chunk
	// header), length its size in bytes. When every count could be read, *expected (if not 0) is set to the size they add up to.
	// A kind the game reads field by field has its exact size, even one the editor doesn't edit (Rebellion's other games' kinds). Any
	// other kind is read by the game as a block of any size, so its size is always right once it's at least 20 bytes.
	SizeCheck CheckStrategySize(uint8_t const *payload, size_t length, size_t *expected);

	// The reason, in plain words, for showing a mapper why a file was refused.
	char const *SizeCheckWords(SizeCheck check);

	// Whether the game would accept this strategy's size: CheckStrategySize finds it right.
	bool StrategySizeIsRight(uint8_t const *payload, size_t length);

	// Whether a value may be stored in a field. value is as stored (after the field's mask, for a masked field). For a field of
	// three numbers (IntTriple), it's one of them; for an object (ObjectId), one of its two halves.
	// When it may not, *why (if not 0) is set to the reason: one of the three below, or the field's own.
	bool ValueIsAllowed(Field const &field, double value, char const **why);

	// The reasons ValueIsAllowed gives that aren't a field's own.
	extern char const NotANumberReason[];    // the value is NaN or endless
	extern char const NotWholeReason[];      // a fraction, where the file holds a whole number
	extern char const DoesNotFitReason[];    // too big or too small for how the file stores it, or outside a masked field's bits

	// The reasons the setters give that aren't about the value itself.
	extern char const ListCountReason[];       // a list's count is set by adding or taking away its items, not by a number
	extern char const UnnamedChoiceReason[];   // a choice field's value is one its choices don't name
	extern char const NotThisKindsReason[];    // the field isn't one of this kind's (or it's the kind's own number)

	// Sets one field of a payload whose size has been accepted, after checking the value with ValueIsAllowed. Gives back false,
	// changing nothing, if the value isn't allowed (*why says why), or if the field doesn't fit in this payload.
	// A field of three numbers or an object can't be set from one number: use SetFieldAxis or SetFieldObject, which this refuses.
	bool SetField(StrategyKind const &kind, Field const &field, uint8_t *payload, size_t length, double value, char const **why);

	// Sets one of a three-number field's numbers (axis 0, 1 or 2: x, y or z), leaving the other two as they were.
	bool SetFieldAxis(StrategyKind const &kind, Field const &field, uint8_t *payload, size_t length, int axis, int32_t value,
	                  char const **why);

	// Sets an object field to the object named by its two halves, as the file stores them (first, then second).
	bool SetFieldObject(StrategyKind const &kind, Field const &field, uint8_t *payload, size_t length, uint32_t first,
	                    uint32_t second);

	// Reading a field: the exact inverses of the three setters above, for the editor's panels, which show a level's values. Each is held
	// to the same rules before it reads a byte: the field has to be one of this kind's (or one every strategy has, but not the kind's own
	// number), and it has to fit in this payload at its place. A value read is the level's, so it's hostile: it's given back exactly as
	// the file holds it (a float may be NaN, a choice may be one its list doesn't name) for the panel to show and flag, never fixed here.
	// (Added 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng.)
	//
	// Reads one field as one number, as stored: after the field's mask, not shifted, for a masked field. A list's count reads as the count
	// the file holds. A field of three numbers, an object or an orientation isn't one number, so this refuses them: the first two have
	// their own readers below. Gives back false, with *value left alone, when the field can't be read (*why says why when it's another
	// kind's field, as SetField does).
	bool ReadField(StrategyKind const &kind, Field const &field, uint8_t const *payload, size_t length, double *value, char const **why);

	// Reads one of a three-number field's numbers: axis 0, 1 or 2 (x, y or z).
	bool ReadFieldAxis(StrategyKind const &kind, Field const &field, uint8_t const *payload, size_t length, int axis, int32_t *value,
	                   char const **why);

	// Reads an object field's two halves, as the file stores them (first, then second). 0 and 0 mean no object.
	bool ReadFieldObject(StrategyKind const &kind, Field const &field, uint8_t const *payload, size_t length, uint32_t *first,
	                     uint32_t *second);

	// The old editor's upgrades of old files, done the same way when a level is read (see strategy_rules.cpp for each one).
	// Each works on a payload whose size StrategySizeIsRight has already accepted, and gives back whether it changed anything.
	bool UpgradeOldStrategy(uint8_t *payload, size_t length);

	// Sets the bits every strategy of its kind always has when it's saved (see strategy_rules.cpp). Gives back whether it changed
	// anything. Like the upgrades, it works on a payload whose size has been accepted.
	bool SetAlwaysSetStrategyBits(uint8_t *payload, size_t length);

	// Writes the payload of a new strategy of that kind, as the old editor made one: every field at its creation value, every list
	// empty, and the always-set bits set. A kind the old editor wrote as another (Airlock and LiftNoTel are Lifts with a flag) is
	// written that way. Gives back false, writing nothing, for a kind it didn't offer, or when capacity is too small; otherwise sets
	// *length to the payload's size.
	bool MakeNewStrategy(int32_t number, uint8_t *buffer, size_t capacity, size_t *length);

	// Where a field starts in this payload, for the field's anchor, or false if it would fall outside the payload.
	bool FieldPlace(StrategyKind const &kind, Field const &field, uint8_t const *payload, size_t length, size_t *place);

	// The width in bytes of a field's storage.
	unsigned StorageBytes(Storage storage);

	// The smallest and largest value a storage can hold. For a field of three numbers (IntTriple) it's each number's range; for an
	// object (ObjectId), each half's; for an orientation (IntMatrix), each of its nine numbers'. A field with no limit of its own
	// (Limit::None) takes anything in this range.
	void StorageRange(Storage storage, double *lowest, double *highest);

	// ---- Other chunks: lights, sounds and markers (chunk_fields.cpp, chunk_rules.cpp) ----

	// A kind of chunk whose numbers the editor edits directly. Its fields are all counted from the start of its payload, or, for a kind
	// whose payload starts with a name (nameFirst), from the byte after that name. Its names, if it has any, are in name_model.h.
	struct ChunkKind
	{
		char const *id;            // the chunk's 8-letter name
		char const *label;         // what it is, in words
		char const *parent;        // the chunk it has to be inside for the game to read it, or "" when the game reads it anywhere
		unsigned leastBytes;       // the smallest payload the game reads, from its registration (0 when it has none)
		unsigned fixedBytes;       // the payload's part that every one has: its fields, and a list's count when it has one
		int listCountAt;           // where its list's count is, or -1 when it has no list; the items follow the count
		unsigned listItemBytes;    // the size of one item of that list
		Field const *fields;
		size_t fieldCount;
		char const *reader;        // where the game reads it, at the pinned AvPx commit
		unsigned namesRequired;    // names straight after the fixed part, one after another, each ended by a zero byte, that the
		                           // game needs: each has to end inside the payload
		unsigned namesOptional;    // names after those that the game takes if they're there: if any bytes are left, the next name
		                           // has to end inside the payload too
		int sizeFieldAt;           // where a block that holds its own size starts, or -1 for none: the block's first number is its
		                           // size, which has to be exactly the rest of the payload. leastBytes then ends just after that
		                           // number, and fixedBytes is the whole size the editor writes
		bool nameFirst;            // the payload starts with a name: its letters, a zero, and zeros up to the next multiple of 4 bytes
		                           // (the game steps over "the name's length plus 4, rounded down to a multiple of 4"). The name's own
		                           // rules are in name_model.h, and it isn't counted in namesRequired. fixedBytes and every field's
		                           // offset then count from the byte after it, and the game's registration gives no smallest size
		                           // (leastBytes is 0): it reads the name, then each number, and refuses the level at the first that
		                           // isn't there
		unsigned trailerBytes;     // the numbers the game reads after its list's last item (0 when it has none, and for a kind with no
		                           // list): the game refuses the level when they aren't there, and a field counted from the end of the
		                           // list (Anchor::AfterFirstList) is one of them
		bool namesToEmpty;         // after namesRequired names come a list of names: the game reads them one after another until an empty
		                           // one (a zero byte where a name would start) or the end of the payload, and each has to end inside the
		                           // payload. namesOptional is 0 for such a kind
	};

	// A name for one bit of a chunk field whose unit is Flags, and for one value of a chunk field whose unit is Choice.
	struct ChunkBitName
	{
		char const *chunk;
		char const *field;
		uint32_t bit;
		char const *label;
	};

	struct ChunkChoiceName
	{
		char const *chunk;
		char const *field;
		int32_t value;
		char const *label;
	};

	extern ChunkKind const ChunkKinds[];
	extern size_t const ChunkKindCount;

	extern ChunkBitName const ChunkBits[];
	extern size_t const ChunkBitCount;

	extern ChunkChoiceName const ChunkChoices[];
	extern size_t const ChunkChoiceCount;

	// The kind with that id, or 0 if the editor doesn't know it. id is the chunk's 8-letter name as a level file holds it: exactly its
	// 8 bytes are compared, and no zero is needed after them (a shorter string ending in a zero simply matches nothing).
	ChunkKind const *FindChunkKind(char const *id);

	// The field of that kind with that name, or 0.
	Field const *FindChunkField(ChunkKind const &kind, char const *name);

	// Whether value is one of the named values of that chunk's Choice field. chunk is as for FindChunkKind: its 8 bytes are compared.
	bool ChunkChoiceIsNamed(char const *chunk, char const *field, int32_t value);

	// Checks a chunk's payload size as the game's reader does:
	// - a kind that starts with a name (nameFirst) has that name ending inside the payload, and then the numbers it has after the
	//   name's padding (fixedBytes) inside it too, or the game refuses the level;
	// - at least the registered smallest size (leastBytes). For a kind with no list and no self-sized block, that's its whole fixed
	//   part; a list kind's fixed part ends with the list's count, which the next rule checks is there;
	// - a list's count that's there, isn't negative, and doesn't claim more items than the bytes after it hold. The game reads only
	//   what the count says, and the numbers after the last item (trailerBytes); it ignores any bytes after those;
	// - a block that holds its own size, filling the rest of the payload exactly. A shorter block than the editor writes (fixedBytes)
	//   is allowed: the game leaves the rest at 0;
	// - names at the end, each ending inside the payload; for a kind with a list of names (namesToEmpty), the list ends at an empty name
	//   or at the end of the payload, and what comes after an empty name is ignored.
	SizeCheck CheckChunkSize(ChunkKind const &kind, uint8_t const *payload, size_t length);

	// The reason, in plain words, for showing a mapper why a chunk was refused.
	char const *ChunkSizeCheckWords(SizeCheck check);

	// Where a chunk's field starts in this payload, the twin of FieldPlace for strategies: from the start of the payload, or, for a kind
	// that starts with a name, from the byte after the name and its padding. Gives back false, leaving *place alone, when the field isn't
	// one of this kind's, when that name has no ending zero inside the payload, or when the field would fall outside the payload.
	// payload has to point at length real bytes: finding a place can read them (a leading name, a list's count), so a caller that may
	// hold no payload checks for that first, as the setters and the readers do.
	bool ChunkFieldPlace(ChunkKind const &kind, Field const &field, uint8_t const *payload, size_t length, size_t *place);

	// Sets one field of a chunk's payload, as SetField does for a strategy: the value is checked first, and nothing changes if it
	// isn't allowed (*why says why) or if the field doesn't fit in this payload, or if there is no payload (a null pointer, which is
	// refused before anything is read or written). A field of three numbers, an object or an orientation can't be set from one number.
	bool SetChunkField(ChunkKind const &kind, Field const &field, uint8_t *payload, size_t length, double value, char const **why);

	// Sets one of a three-number field's numbers (axis 0, 1 or 2), leaving the other two as they were.
	bool SetChunkFieldAxis(ChunkKind const &kind, Field const &field, uint8_t *payload, size_t length, int axis, int32_t value,
	                       char const **why);

	// Reading a chunk's fields: the inverses of SetChunkField and SetChunkFieldAxis, and the object field's reader, with the same rules as
	// ReadField, ReadFieldAxis and ReadFieldObject for a strategy: the field has to be one of the kind's own and fit in the payload at its
	// place, and what's read is the level's, so it's given back exactly as the file holds it (a fraction may be NaN, a choice may be one
	// its list doesn't name). A masked field is given with its mask applied, not shifted. On any refusal the value is left alone. *why is
	// never set (a read has no reason to show a mapper); the parameter is there so the signature is ReadField's. A field of three numbers,
	// an object or an orientation isn't one number: ReadChunkField refuses them, and the first two have their own readers below.
	// (Added 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng.)
	bool ReadChunkField(ChunkKind const &kind, Field const &field, uint8_t const *payload, size_t length, double *value, char const **why);
	bool ReadChunkFieldAxis(ChunkKind const &kind, Field const &field, uint8_t const *payload, size_t length, int axis, int32_t *value,
	                        char const **why);
	bool ReadChunkFieldObject(ChunkKind const &kind, Field const &field, uint8_t const *payload, size_t length, uint32_t *first,
	                          uint32_t *second);

	// Sets an object field of a chunk to the object named by its two halves, as the file stores them (first, then second): the chunk twin of
	// SetFieldObject, and the inverse of ReadChunkFieldObject. Gives back false, changing nothing, if the field isn't one of the kind's, isn't
	// an object field, or doesn't fit in the payload. (Added 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng.)
	bool SetChunkFieldObject(ChunkKind const &kind, Field const &field, uint8_t *payload, size_t length, uint32_t first, uint32_t second);

	// ---- Every family of chunk tables (chunk_families.cpp) ----

	// The chunks the editor edits are described in several tables: chunk_fields.cpp's, and one more for each family added since, each in
	// its own file. The editor's code that works on any chunk asks FindAnyChunkKind and never a family's own functions, so a new family is
	// added in ONE place (chunk_families.cpp) and every adapter has it at once.

	// The names a chunk's fields use: the lists of named bits and named choices of the family that holds the chunk.
	struct ChunkNames
	{
		ChunkBitName const *bits;
		size_t bitCount;
		ChunkChoiceName const *choices;
		size_t choiceCount;
	};

	// The kind with that id in any family, or 0 if the editor doesn't know it; id is as for FindChunkKind. When names isn't 0, it's set to
	// the names of the family the kind was found in, or to nothing (null lists, counts 0) when the kind isn't found.
	ChunkKind const *FindAnyChunkKind(char const *id, ChunkNames *names);

	// Which family holds a chunk. Every chunk the editor has a table for is in exactly ONE family, and chunk_families.cpp says so by name.
	// The families whose chunks are numbers counted from the start of the payload (Base, Acoustics) give back a ChunkKind through
	// FindAnyChunkKind. The others describe a chunk in PARTS (the numbers at its head, and the numbers of ONE item of a list, each counted
	// from an origin that depends on the payload) and can't be handed back as a ChunkKind: they are shown one item at a time, by code of
	// their own. (Added 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng.)
	enum class ChunkFamily
	{
		None,                // no family has a table for it
		Base,                // chunk_fields.cpp: lights, sounds, generators, particle generators, the player's start, the level's settings, module flags
		Acoustics,           // chunk_fields_acoustics.cpp: module and level acoustics, ambience, the light set's header and its scale
		Tracks,              // chunk_fields_tracks.cpp: OBJTRAK2, the track of a moving object
		AlternateLocations,  // chunk_fields_tracks.cpp: ALTLOCAT, the other places an object may start in
		Ai,                  // chunk_fields_ai.cpp: patrol paths, waypoints, AI modules, entry points, the modules a module sees, zones
	};

	// Whether that family has a table for the chunk with that id (id is as for FindChunkKind: its 8 bytes are compared; a null id is
	// nobody's). A chunk is to be claimed by ONE family: host_chunk_families_test fails if two claim one name, or if a name in a family's
	// own tables isn't claimed by that family.
	bool ChunkFamilyClaims(ChunkFamily family, char const *id);

	// The family that holds the chunk with that id, or None. Should two families claim it (a mistake the test above catches), the first
	// of Base, Acoustics, Tracks, AlternateLocations and Ai.
	ChunkFamily FindChunkFamily(char const *id);
}
