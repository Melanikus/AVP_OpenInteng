// special_mode.h: Object Placement's commands for the five special objects as the editor runs them: Create AVP generator (0x34), AVP Bad Guy (0x35), Player starting position (0x3a), Sound (0x37),
// Particle Generator (0x39), Edit (0x18), Delete (0x16) and, on the sounds screen, Find (0x19) of the CURRENT special object. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// inteng makes a new object at the view and makes it the current one (FUN_004d64a0); Edit calls the current object's own dialog (vtable +0x38); Delete takes it away. The special objects are not
// rows of the editor's object list, so the mode keeps which one is current. The dialogs are shown as one prompt per field, in the dialog's order (the UI is ours; what is asked, the limits, the units and
// the dependent fields are the dialogs'). Every fact is READ and written in AvP-docs\engine\inteng\edobtype-dialogs.md (the DLL's sound, generator and bad guy dialogs) and
// object-placement-reads.md (inteng.exe's particle generator dialog, constructors and save methods).
// NAMED DEVIATIONS: (1) the name of a new object is the one typed in inteng's "Name Object" dialog (FUN_004d53d0, before Create: the editor's NameObjectDialog); the file's name field holds 63 plain
// letters (inteng's edit takes 200), so a longer name is refused when the object is made. (2) The Marine "Texturing ID" is a number (0 = Random): its names come from hnpcmarine.txt, which is not in the tool folder. (3) The particle generator's Sound button and parent
// click are not done. (4) The wav name is typed, not chosen from a list. (5) The Play button is not done.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "special_objects_edit.h"

class OpenLevelFile;

namespace host
{
	enum class SpecialAsk
	{
		None,
		PickObject,    // Find: which of the level's special objects is the current one
		Field,         // Edit: the field mode->step of the object's dialog
	};

	struct SpecialStep
	{
		bool prompt = false;
		std::string title;
		std::string answer;
		std::string defaultText;
		bool passOn = false;     // not this module's: the editor does what it did before (Delete or Find with no special object to work on)
		bool form = false;       // Edit: the object's dialog opens as ONE window with all its fields (SpecialMode_FormItems), and `answer` says what was wrong with the last answer ("" when nothing was)
	};


	struct SpecialMode
	{
		bool haveCurrent = false;
		SpecialKind kind = SpecialKind::Generator;
		size_t number = 0;

		SpecialPlace where;                 // the view's place and turn: the caller sets it before a Create
		std::string createName;             // the name typed in inteng's "Name Object" dialog (FUN_004d53d0) before a Create; the caller sets it (the player start takes none)
		double levelScale = 1.0;
		bool useForm = false;               // the editor opens Edit as one window (SpecialMode_FormItems); a test drives it as typed answers
		bool soundsOnly = false;            // the sounds screen: Find lists the sounds and nothing else

		SpecialAsk ask = SpecialAsk::None;
		size_t step = 0;                    // the field of the dialog that is open
		std::vector<std::string> list;      // the lines of the chooser the question shows
		std::vector<SpecialObject> found;   // the objects Find lists
		SpecialObject draft;                // the object being edited
	};

	// One control of an object's dialog: kind 0 an edit (its text), 1 a check box ("y" / "n"), 2 a choice (the choice's name, typed or picked)
	struct SpecialFormItem
	{
		std::string label;
		int kind = 0;
		std::string value;
	};

	// The window of the draft being edited: its title (edobtype.dll's: "Generator Settings", "Edit object properties", "Sound Edit - <name>") and its controls in the dialog's order. false when no Edit is open.
	bool SpecialMode_FormItems(SpecialMode const &mode, std::string *title, std::vector<SpecialFormItem> *items);

	// OK: every control's text in the order of the items is applied to the draft (a text out of range stops at that control: the step says which, the form opens again with the draft as it stands), then the object is written
	SpecialStep SpecialMode_FormAnswer(OpenLevelFile *file, SpecialMode *mode, std::vector<std::string> const &values);

	// OK of a window that works the object out itself (the Bad Guy's, bad_guy_edit.h): the object is written as it is given (no form rules applied) and the Edit ends, as FormAnswer's end does
	SpecialStep SpecialMode_FormWrite(OpenLevelFile *file, SpecialMode *mode, SpecialObject const &object);

	bool SpecialMode_Does(int32_t command);

	SpecialStep SpecialMode_Command(OpenLevelFile *file, SpecialMode *mode, int32_t command);
	SpecialStep SpecialMode_Typed(OpenLevelFile *file, SpecialMode *mode, std::string const &typed);
	void SpecialMode_Cancel(SpecialMode *mode);
	void SpecialMode_Reset(SpecialMode *mode);

	// Puts the current special object at this place (file units), for Snap To Vertex; the answer's words say what happened
	SpecialStep SpecialMode_MoveCurrent(OpenLevelFile *file, SpecialMode *mode, int32_t const place[3]);

	// Sets the parent of the current particle generator to the object with this 8-byte ID ("Click on parent for generator", FUN_004e7100; Object Placement's Q key)
	SpecialStep SpecialMode_SetParent(OpenLevelFile *file, SpecialMode *mode, uint8_t const id[8]);

	// The words for an object in a chooser: "Particle generator: sparking light"
	std::string SpecialObject_Words(SpecialObject const &object);
}
