// simple_object_edit.h: Object Placement's Edit (E, Object > Edit) of an ORDINARY placed object: inteng's "Edit object properties" window (edobtype.dll's edit_simple_object, dialog 129 in the
// AvP mode, DAT_00915104 == 0). Written 2026-11-16 by Vasquez (Sonnet 5.5) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (every line read; FUN_ addresses are inteng.exe's, the DLL's are edobtype.dll's):
// - The Edit key / item (command 0x18, FUN_004d9af0 0x4dc212) calls the selected object's method +0x38, FUN_004e3400. With the game kind 0 (AvP: FUN_0057aa00(4) is false) it fills a struct
//   from the object's fields +0x44 type, +0x48 extra data, +0x4c flags, +0x50 mass (low 16 bits) and integrity (high 16 bits), +0x54 explosion, +0x58, calls edit_simple_object and copies the six
//   back (0x4e3515-0x4e35d5). The window is Cancel-able: Cancel changes nothing.
// - THE FIELDS OF THE OBJECT COME FROM THE FILE (FUN_004e0ed0-d, 0x4e0729-0x4e0ca3). The flags (+0x4c): marine 0x800, alien 0x1000 and predator 0x2000 from the object's OBJHEAD1 flags; the three
//   platform bits 0x8000 / 0x10000 / 0x20000 are all ON when the header's 0x4000 (platform load set) is clear, else the header's own; the difficulty bits 0x100000 / 0x200000 / 0x400000 (the
//   file keeps the OPPOSITE: not at this difficulty) from the header. The object's FIRST strategy (first AVPSTRAT of the first OBJPRJDT): a simple object (62; 1003 too) gives type (strategy
//   byte 20), extra data (24), mass (28, low 16 bits), integrity (32, low 16 bits) and explosion ((flags at 36 >> 4) & 3); with type AND extra both 0 the shape's name is looked up (below). A
//   legacy 53 gives nothing, any other strategy kind gives the defaults, which are type 0, extra 0, mass 50, integrity 2, explosion 0 (+0x50 starts as 0x20032). An object with NO strategy has
//   the flags 0xb800 (marine, alien, predator, PC) and the shape lookup.
// - THE SHAPE LOOKUP (FUN_004e44c0 0x4e4738-0x4e4755): the lower-cased name of the object's shape's outside file (the first SHPEXTFN of its SHPEXTFL, the folder and extension taken off) is looked
//   up in inteng's table at 0x5e7b58 (29 rows in his inteng.exe: read at run time, SimpleObject_SetShapeNames below; no copy in our source); a hit gives the type and extra data of the table, and flags |= 0x38000 (done for
//   any object whose shape has an outside file, hit or not).
// - THE WINDOW (edobtype.dll, FUN_10003de0 init, FUN_10004520 per type, FUN_100060e0 type changed, FUN_10005ae0 OK; the control ids are the resource's): "Type of Object" (the 17 names below, in that order,
//   the one whose value is the object's type picked, none when the value is in no entry), "Game mode" Marine / Alien / Predator and "Difficulty" Easy / Medium / Hard (ticked when the file's NOT bit
//   is clear), "Explosion" None / Small / Big / Molotov (0 to 3). By the type: Static, Furniture, Video Screen, Special Game Pickup Object and Placed light show "Mass (x100g)" and "Integrity" (numbers);
//   Weapon, Ammo, Medikit, Armour and Key show a list headed "Enter type" (the lists below); the others show neither. A type change only changes what is shown: Mass and Integrity keep what
//   was typed.
// - OK (FUN_10005ae0 / FUN_10006220): for the five with a Mass and Integrity, each is read as a number (sscanf "%d"); a number that is not the same as its own low 16 bits (negative, or above 65535)
//   is refused with a message "Mass value too big (>65535)" / "Integrity value too big (>65535)" (box title "Object Edit") and the window stays open. NOTE (inteng's own): text that is no number
//   at all leaves the value that is there, which is the TYPE's number (the function's own parameter): Static gives 0, Furniture 1 ... Copied. For the lists the pick (if any) becomes the extra data.
// - THE SAVE (FUN_004e1f90 0x4e222e-0x4e2c8c, run for the object when it is updated): the header's flags get the six bits of the game mode and difficulty and the three platform bits from the
//   window's flags, and 0x4000 ON. The strategy ONLY when the load marked the object (+0x5c, FUN_004e0ed0 0x4e0aaf-0x4e0bc0: no strategy, a legacy 53, a simple object 62 / 1003, the other
//   game's 2000-2002; checked at 0x4e2521 / 0x4e2808): none or a 53 is REPLACED (made again) by a simple object (62) of the window's type, extra data, mass & 0xffff, integrity | 0x80000000,
//   flags 1 and no target; a simple object (62 or 1003) is changed in place: type, extra data, mass (low 16 bits), integrity (high bit always on) and the explosion bits (0x30) of the flags
//   word. ANY OTHER strategy (a door, a switch, a lift ...) is left as it is: Edit then changes the file's header flags only. (Re-read 11-17 by Drake: this said every other kind was
//   replaced, and ours replaced a door's strategy by a simple object: OUR BUG, FIXED.)
//   Then THE PLACED LIGHT BY THE TYPE (0x4e2cd5-0x4e2dd8, whatever the strategy is; Frost 11-17, reassess/light-closeout-frost.md TB-L2): Type 16 ("Placed light") and no PLOBJLIT gives the
//   object inteng's default placed light (brightness 1.0, spread 0, range 1,000,000 / (100 x the scale), white up, black down, Omni, Runtime + No PreLight, Start on); any other Type takes a
//   PLOBJLIT the object has away. This is how the stock lamps were made: all 2047 are Type 16 with spread 0.
// - THE SAVE RUNS FOR EVERY PLACED SHAPE at Object Placement's Exit (0xa) and Save (0xe) (FUN_004d9af0 0x4d9d66 / 0x4d9eba: each object of the placement list, its +0xc; OP-130a), with the
//   fields its load gave it: SimpleObject_SaveAll. On the stock levels it changes nothing (Drake's survey, OP-130a). NAMED: an object with no strategy keeps its header's game-mode and
//   difficulty bits (inteng's load gives it 0xb800, so its save drops them); and the window's numbers for an object with another strategy (a door) are not kept for the sitting, so a
//   door given Type "Placed light" by Edit gets the light at the OK and loses it at that sitting's Exit (inteng: at its Exit, and loses it at the next sitting's).
//   Written only where the bytes differ, so a save that changes nothing leaves the level unchanged.
// - THE WINDOW'S PARTS AS THE RESOURCE HOLDS THEM (dialog 129, read 11-17): "Game mode" (Marine / Alien / Predator) and "Difficulty" (Easy / Medium / Hard) are HIDDEN in this mode, as for
//   the bad guy (style 0x48010003 without WS_VISIBLE, never shown by FUN_10003de0; Bishop's ruling 11-16): their flags come back unchanged. "Explosion" is shown. The type combo is a
//   drop-down list in the table's order (no CBS_SORT); "Enter type" (0x3e9) is an editable drop-down.
// NAMED POINTS:
//  - inteng writes a new strategy's explosion only on the next update; here it is written at once (the end result after any save).
//  - inteng writes the integrity word as (the window's number >> 16, arithmetic) | 0x80000000: a number of 32768 or more keeps junk in bits 16 to 30; here the word is (number & 0xffff) | 0x80000000.
//  - INTENG BUG, FIXED: Armour's list is filled from the Armour table (0x1001fa48) but its current row is looked up in the Medikit table (FUN_10004520 0x10004a4f: 0x1001fa28), so inteng
//    never shows an Armour object's row; here the Armour row is shown.
//  - inteng's "Enter type" box takes typed letters, but OK reads only a PICKED row (FUN_10005a50: CB_GETCURSEL); here it is a drop-down list, so the extra data changes the same ways.
//  - inteng keeps what the window was given in the object for the sitting even when its save will not write it (an object with another strategy); here such an object's window opens on
//    the file's values again (nothing of it reaches the file in either).
//  - The object's own chunks are changed in the editor's bytes (OpenLevelFile::SetPayload) and a replaced strategy is made by AssignStrategy (the loader reads the result first).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

class File_Chunk;      // the game's chunk library
class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

namespace host
{
	// The window's globals (edobtype.dll DAT_10022648 / 1002264c / 10022640 / 10022644 / 10022654, the struct's flags)
	struct SimpleObjectDraft
	{
		int32_t type = 0;
		int32_t extra = 0;
		int32_t mass = 50;
		int32_t integrity = 2;
		int32_t explosion = 0;
		uint32_t flags = 0;       // inteng's +0x4c: bits 0x800 0x1000 0x2000 0x8000 0x10000 0x20000 0x100000 0x200000 0x400000
		int32_t strategyKind = 0; // the object's first strategy's kind (0 none)
	};

	enum class SimpleObjectResult
	{
		Done,
		BadCall,
		NoObject,        // no object has that ID
		NoHeader,        // the object has no header, or the store has no bytes for it
		NotPlaced,       // the object is a module: a simple object is not made on it (NAMED: AssignStrategy refuses it; inteng would)
		CantBuild,       // the strategy could not be put together
		LoaderRefused,   // the game's loader would not take the changed level: nothing changed
	};

	// What an answer means, plain words: static text, never null
	char const *SimpleObjectWords(SimpleObjectResult result);

	// The object's fields as inteng loads them (above). *out is set whatever happens.
	SimpleObjectResult SimpleObject_Read(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, SimpleObjectDraft *out);

	// The window's answer written into the object (above). Nothing is changed when the answer is not Done.
	SimpleObjectResult SimpleObject_Write(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, SimpleObjectDraft const &draft);

	// Object Placement's Exit and Save (OP-130a, above): every placed object (header flag 0x400) read as its load reads it and written by the save. *saved: how many were saved; *changed: how
	// many of those had a byte changed. The first refusal stops it (the objects before it are saved).
	SimpleObjectResult SimpleObject_SaveAll(OpenLevelFile *store, size_t *saved, size_t *changed);

	// ---- the window
	struct SimpleObjectEdit
	{
		uint8_t id[8] = { 0 };
		SimpleObjectDraft draft;
		std::string massText;       // the two number boxes' texts, kept across type changes
		std::string integrityText;
		bool open = false;
	};

	struct SimpleFormItem
	{
		std::string label;
		std::string value;   // an edit's / list's text, a check box's y / n
		int kind = 0;        // 0 a number box, 1 a check box, 2 a combo box (choices), 3 a group of radio buttons (choices)
		std::vector<std::string> choices;   // a combo's rows, or the radio group's labels, in the DLL's order
	};

	// What the window shows for a type (FUN_10004520): the list headed "Enter type" and its rows, and whether the Mass and Integrity boxes are there
	struct SimpleLayout
	{
		bool extra = false;
		std::vector<std::string> extraChoices;
		bool mass = false;
	};

	SimpleLayout SimpleEdit_LayoutFor(int32_t type);

	// The type a typed or picked word names (a name in any capitals, or its number): false when it names none
	bool SimpleEdit_TypeOfWord(std::string const &word, int32_t *type);

	// The row of the extra data list of a type whose value this is (-1 for none)
	int SimpleEdit_ExtraRow(int32_t type, int32_t extra);

	// Starts the window on the object: false (nothing open) when the object can't be read
	bool SimpleEdit_Begin(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, SimpleObjectEdit *edit);

	// What the window shows now: its title and its controls in order
	void SimpleEdit_Items(SimpleObjectEdit const &edit, std::string *title, std::vector<SimpleFormItem> *items);

	enum class SimpleAnswer
	{
		Reshow,    // (not used since the window's combo box changes what is shown at once: SimpleEdit_LayoutFor)
		Message,   // a refused number: *words is the message (box title "Object Edit"), then show the window again
		Done,      // OK taken and written
		Failed,    // OK taken but the object could not be written: *words says why
	};

	// OK pressed with the controls' values in the order of SimpleEdit_Items for the type the FIRST value names (a type with no list has no second value, one with no Mass no number boxes). Hostile text: every value is read to at most 63 letters.
	SimpleAnswer SimpleEdit_Answer(OpenLevelFile *store, File_Chunk *level, SimpleObjectEdit *edit, std::vector<std::string> const &values, std::string *words);

	// The names and numbers of the lists, for the tests: the types, and the extra data of a type (empty for a type with none)
	struct SimpleChoice { char const *name; int32_t value; };
	size_t SimpleTypeChoices(SimpleChoice const **list);
	size_t SimpleExtraChoices(int32_t type, SimpleChoice const **list);

	// inteng's shape-name table (0x5e7b58), READ AT RUN TIME from his inteng.exe (inteng_shapes.h IntengShapeNames_Read; Bishop's ruling V-31 11-16: a table in the exe's data is never
	// copied into our source). editor_main.cpp gives it once at start; until then, or when inteng.exe was not read, there is no table and no shape's type is guessed (the start's status says why).
	struct SimpleShapeName
	{
		std::string name;
		int32_t type = 0;
		int32_t extra = 0;
	};
	void SimpleObject_SetShapeNames(std::vector<SimpleShapeName> const &names);
	bool SimpleObject_ShapeNamesKnown();

	// the type and extra data of a lower-case shape name from that table; false for a name not in it (or no table)
	bool SimpleShapeGuess(std::string const &lowerName, int32_t *type, int32_t *extra);
}
