// dialog_box.h: inteng's MESSAGE, CONFIRM and TEXT dialogs (the small windows its screens put up with FUN_0057c800): what each shows, where its parts are, and what each way of leaving it answers. Written 2026-11-16 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT inteng DOES (read 2026-11-16 in dump-all-1/2/4.txt and listing-all-1b/2/4.txt; the list dialog, FUN_00475bf0, is list_picker.h):
// - FUN_0057c800 is the run of every such window (modal): it puts the window in the MIDDLE of the screen, ((screen width - window width) / 2, (screen height - window height) / 2) at 0x57c80e-0x57c82d (the "(10, 10)" the callers show is
//   not an argument: the function takes none; a list dialog as wide as the screen only looks to be at 10), then loops on the mouse and the keys until the window has an answer, and gives it back. A left press goes to the control under it
//   (control method +8 finds it, +0xc presses it, the window's method +4 turns the control's number into the answer); the Return key gives the window's default answer (its short at +0x1e); the Escape key (DAT_00d87a45, 0x57cc91-0x57cca4)
//   gives 1. FUN_0057d2b0 closes the window after.
// - MESSAGE (FUN_00474030(text), table 0x5ad5c8): a 400 x 200 window with the text at (0x12, 0x12), 0x172 wide and 100 high, one button "Ok" at (200, 0x96), 0x32 x 0x10. The button answers 3 (method 0x474150: control 1 -> 3), Return 3, Escape 1.
// - CONFIRM (FUN_00474a40(text), table 0x5ad608): the same window with two buttons, "Yes" at (100, 0x96) and "No" at (200, 0x96), both 0x32 x 0x10. Yes answers 3, No answers 2 (method 0x474b90: control 1 -> 3, control 2 -> 2), Return 3, Escape 1.
// - TEXT (FUN_00474350(text, label 1, text 1, label 2, text 2, label 3, text 3), table 0x5ad5e8): a 400 x 0xfa window with the text at (0x12, 0x12), 0x172 wide and 0x50 high; up to three edit fields, 0x96 x 0x12, at (0x1e, 100), (0x1e, 0x7c) and
//   (0x1e, 0x94) each with its label to the right at x = 0xbe (a field has a label only when one is given; a field is there only when its label is given), the first field in focus; buttons "Ok" at (100, 0xbe) and "Cancel" at (200, 0xbe),
//   0x32 x 0x10. Ok answers 3 (method 0x4747a0: control 8 -> 3), Cancel 1 (control 9), Return 3, Escape 1. The answer is read from the fields after.
// - FORM (the toolkit every other dialog is built from, 0x57xxxx): FUN_0057d170(width, height, ...) the window, FUN_0057e2f0(title) its title, FUN_0057d400(x, y, w, h, text) a label, FUN_0057de30(x, y, w, h) an edit box of up to 10 letters (FUN_0057d8e0 with 10;
//   the list dialog's text field is FUN_0057d8e0 with 100), FUN_0057d670(x, y, state) a check box (the radio groups of the Light properties dialog are check boxes that the dialog's own event method keeps exclusive), FUN_0057d830(x, y, w, h, text) a button.
//   All places are from the window's top left. Return answers 3, Escape 1, a button its own answer. Track Properties (FUN_004f4090) and Light properties (FUN_004b0470) are forms.
// NAMED: the colours, the font, the size of a check box (DAT_00915210 is set at run time and not read: 12 here) and the focus keys (Tab, the Up and Down keys) are ours.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

namespace host
{
	struct DialogRect
	{
		int32_t x = 0, y = 0, width = 0, height = 0;   // in the screen's pixels
	};

	enum class DialogKind
	{
		Message,
		Confirm,
		Text,
		Form,
	};

	// The toolkit's parts of a form
	enum class DialogWidgetType
	{
		Label,
		Edit,
		Check,
		Button,
		Combo,   // a drop-down list (a combo box of the DLL's dialogs): the box shows the picked item's words; a press opens the list under it, a press on a row picks it
	};

	struct DialogWidget
	{
		DialogWidgetType type = DialogWidgetType::Label;
		DialogRect box;                 // from the window's top left when given, in the screen's pixels once the form is open
		std::string text;               // a label's or button's words, an edit's contents
		bool checked = false;           // a check box
		int32_t group = -1;             // check boxes with the same group of 0 or more are exclusive (a radio group, kept by the dialog's own method in inteng)
		int32_t answer = 0;             // a button: what leaving by it answers
		size_t most = 10;               // an edit: the most letters (FUN_0057de30 gives 10)
		std::vector<std::string> items; // a combo: the rows, in the order given (the DLL's combo boxes are not sorted)
		int32_t selected = -1;          // a combo: the row picked (-1 none, the box then shows no words)
		bool listOpen = false;          // a combo: its list is open under it
		bool visible = true;            // a part that is not visible is not drawn, not pressed and not given to the harness
	};

	struct DialogFormSpec
	{
		std::string title;
		int32_t width = 400;
		int32_t height = 200;
		std::vector<DialogWidget> widgets;

		// Called after a combo's pick CHANGED, with the form's parts and the number of the combo (as the DLL's CBN_SELCHANGE handlers do): the parts may be changed in place (items, visibility, texts)
		std::function<void(std::vector<DialogWidget> &, size_t)> onChange;
	};

	// What a way of leaving answers (inteng's numbers)
	int32_t const kDialogEscape = 1;   // Escape, or Cancel
	int32_t const kDialogNo = 2;
	int32_t const kDialogOk = 3;       // Ok, Yes, Return

	struct DialogButton
	{
		std::string label;
		DialogRect box;
		int32_t answer = 0;
	};

	struct DialogField
	{
		std::string label;       // to the right of the edit; empty: no label
		std::string text;        // what the edit holds
		DialogRect edit;
		DialogRect labelBox;
	};

	class InDialog
	{
	public:
		// The text, the screen's place and size (the window goes in its middle), the width of a text (the editor's own letters). Text takes up to three fields (labels and first contents).
		void OpenMessage(std::string const &text, DialogRect screen, std::function<int32_t(std::string const &)> const &textWidth);
		void OpenConfirm(std::string const &text, DialogRect screen, std::function<int32_t(std::string const &)> const &textWidth);
		void OpenText(std::string const &text, std::vector<DialogField> const &fields, DialogRect screen, std::function<int32_t(std::string const &)> const &textWidth);
		void OpenForm(DialogFormSpec const &spec, DialogRect screen, std::function<int32_t(std::string const &)> const &textWidth);
		void Close();
		bool IsOpen() const { return open; }

		DialogKind Kind() const { return kind; }
		DialogRect Window() const { return window; }
		DialogRect TextArea() const { return textArea; }
		std::vector<std::string> const &Lines() const { return lines; }   // the text, broken to fit the text area
		std::string const &Text() const { return text; }
		std::vector<DialogButton> const &Buttons() const { return buttons; }
		std::vector<DialogField> const &Fields() const { return fields; }
		std::vector<DialogWidget> const &Widgets() const { return widgets; }   // a form's parts, in the screen's pixels
		std::string const &Title() const { return title; }
		size_t Focus() const { return focus; }

		// A press at (x, y): true when it ended the dialog, with the answer in *answer. A press on a field only gives it the focus.
		bool Press(int32_t x, int32_t y, int32_t *answer);

		// Return and Escape: always end the dialog (Return with 3, Escape with 1)
		int32_t Return() { return kDialogOk; }
		int32_t EscapeKey() { return kDialogEscape; }

		// Letters for the field in focus, Backspace, and the focus moved to the next field
		void Type(char letter);
		void Backspace();
		void NextField();

		// The box of a combo's list row (screen pixels), for the painting and the harness
		static DialogRect ComboRow(DialogWidget const &combo, size_t row);

	private:
		void Layout(DialogRect screen, int32_t width, int32_t height, std::function<int32_t(std::string const &)> const &textWidth);

		bool open = false;
		DialogKind kind = DialogKind::Message;
		std::string text;
		std::vector<std::string> lines;
		DialogRect window, textArea;
		std::vector<DialogButton> buttons;
		std::vector<DialogField> fields;
		std::vector<DialogWidget> widgets;
		std::string title;
		std::function<void(std::vector<DialogWidget> &, size_t)> changeHook;
		size_t focus = 0;   // a text dialog's field, or a form's widget (an edit)
	};
}
