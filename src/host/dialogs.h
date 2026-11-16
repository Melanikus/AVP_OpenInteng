// dialogs.h: THE ONE PLACE inteng's dialogs are made from: the message box, the Yes / No confirm, the text dialog, the list dialog and the form toolkit (dialog_box.h, list_picker.h hold the traced layouts; this holds the open dialog, its answer, its drawing and
// its state line). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE (any mode):
//   host::Dialogs_Message("All modules have adjacent modules");                                       // FUN_00474030: the words and Ok
//   host::Dialogs_Confirm("Delete it?", [](bool yes) { ... });                                         // FUN_00474a40: Yes (Return) / No (Escape too, as 1 is not Yes)
//   host::Dialogs_Text("Visible distance in metres :", { field }, [](int answer, std::vector<std::string> const &texts) { ... });   // FUN_00474350: up to three edits with labels, Ok / Cancel; answer is kDialogOk or kDialogEscape
//   host::Dialogs_List("Select Sequence", rows, [](int place) { ... });                               // FUN_00475bf0: the rows shown sorted; place is the row's place in the rows given, -1 for Escape
//   host::Dialogs_Form(spec, [](int answer, std::vector<host::DialogWidget> const &widgets) { ... }); // the toolkit: window, title, labels, edits (10 letters), check boxes (radio by group), buttons; answer 3 = Ok / Return, 1 = Escape, a button's own answer
// While one is open nothing else takes a press or a key (it is modal, FUN_0057c800), and the editor's prompt stays 0. The editor calls Dialogs_SetScreen each frame, Dialogs_Press / Dialogs_Key for the window's messages, Dialogs_Draw for the panel pass and
// Dialogs_State for the state line (Bishop's harness contract: dialog, dialogkind, dialogrows, dialogrow1, dialogbuttons, dialogtext, and for forms dialogchecks and dialogfields).
#pragma once

#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "dialog_box.h"
#include "list_picker.h"
#include "panel_paint.h"

namespace host
{
	void Dialogs_Message(std::string const &text, std::function<void()> const &then = nullptr);
	void Dialogs_Confirm(std::string const &text, std::function<void(bool)> const &then);
	void Dialogs_Text(std::string const &text, std::vector<DialogField> const &fields, std::function<void(int32_t, std::vector<std::string> const &)> const &then);
	void Dialogs_List(std::string const &title, std::vector<std::string> const &rows, std::function<void(int32_t)> const &then);
	void Dialogs_Form(DialogFormSpec const &spec, std::function<void(int32_t, std::vector<DialogWidget> const &)> const &then);

	bool Dialogs_IsOpen();

	// The open list dialog is taken away WITHOUT its answer being given (for code that answers what the list asks in its own way: the Hierarchy hand-off of a picked object)
	void Dialogs_DismissList();

	// The box the dialogs are centred in (the view), each frame
	void Dialogs_SetScreen(int32_t x, int32_t y, int32_t width, int32_t height);

	// A left press at (x, y) of the screen's pixels, and a key: isChar for WM_CHAR (key is the letter), else a virtual key. Both return true when a dialog took it.
	bool Dialogs_Press(int32_t x, int32_t y);
	bool Dialogs_Key(bool isChar, uint32_t key);

	// What to paint: gray behind the window, then boxes and lines (the editor's panel pass takes both lists)
	void Dialogs_Draw(std::vector<PaintBox> *boxes, std::vector<PaintText> *lines, uint32_t grayColour, uint32_t litColour);

	// The state line's dialog fields; all empty when none is open
	struct DialogsState
	{
		std::string title, rows, row1, kind, buttons, text, checks, fields;
		std::string combos;     // a form's combo boxes: "<picked words>_<x> <y> <width> <height>" ... separated by |, the words with spaces as _ (the harness's click-combo presses one)
		std::string comborows;  // the rows of the combo whose list is open: "<words>_<x> <y> <width> <height>" separated by | (the harness's click-combo-row presses one)
		std::string cells;   // a list's rows on the PAGE SHOWN: "<row words>@x,y,width,height|..." (the harness's click-choice clicks one by its words)
	};

	DialogsState Dialogs_State();
}
