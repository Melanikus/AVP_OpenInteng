// object_base.h: inteng's Construction > Object > Set Base Module on the open level. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (the case 0x140 of FUN_004452b0, out\menu-reads-1-full.txt:6175-6290; also out\map-1-strings.txt:7373-7415): only when exactly one object
// is selected and the word at +0x2c of its record has 0x400 clear, it takes bit 0x100 out of that word for EVERY object (every RBOBJECT of the file), then
// puts bit 0x100 in the selected object's word.
// THE WORD: that record's +0x2c is the object header's flags: OBJHEAD1's first number. The same word is the one FUN_004e1f90 puts 0x400 and 0x4000 in
// (menu-reads-3.txt:495-562), and the game names its bits OBJECT_FLAG_BASE_OBJECT 0x100, OBJECT_FLAG_MODULE_OBJECT 0x200 and OBJECT_FLAG_PLACED_OBJECT 0x400
// (win95/chunk_object.hpp:16-18). So the test is "not a placed object", and the effect is "this object is the level's base object, and no other is".
// (That the record is the header is read from these three agreeing facts: the decompile hides the call's object.)
//
// HOW: each header's flags are changed through the edit store (OpenLevelFile::SetPayload: the chunk keeps its size). Nothing is rebuilt.
// What the level's bytes hold is hostile: a header too short to hold the flags is left alone.
#pragma once

#include <stdint.h>

class OpenLevelFile;

namespace host
{
	enum class BaseObjectResult
	{
		Done,
		NothingOpen,     // no level is open
		NoName,          // the ID is null or all 0
		NoSuchObject,    // no object in the level has that ID
		IsPlaced,        // the object is a placed object (its flag 0x400): inteng does nothing for it
		CantChange,      // a header's bytes couldn't be changed
	};

	// What an answer means, in plain words for the editor's status line: static text, never null
	char const *BaseObjectWords(BaseObjectResult result);

	// Makes the object whose 8-byte ID is id the base object. How many headers changed (flag 0x100 taken out of others, put in this one) is in *changed, if not null.
	BaseObjectResult SetBaseObject(OpenLevelFile *file, uint8_t const *id, int *changed);

	// Puts the module flag (OBJECT_FLAG_MODULE_OBJECT, 0x200) into the header word of the object whose 8-byte ID is id (on), or takes it out (off): what
	// inteng's writing of a module's links does to the module's object (FUN_004c3b90 / FUN_004c10c0, menu-reads-4.txt:0x4c13b0 sets it when the module
	// has links, 0x4c1410 clears it when the changed tree has none). Written 2026-11-15 by Melanikus (Drake). A placed object is refused (IsPlaced), as
	// a module is never placed. A same-size edit through the edit store.
	BaseObjectResult SetModuleObjectFlag(OpenLevelFile *file, uint8_t const *id, bool on);
}
