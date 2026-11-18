// host_dialog_box_test: inteng's text boxes' sizes and places (dialog_box.h InDialog::OpenText). Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng (the Strategy box batch, Bishop's
// Q9c: number boxes 400 x 200 as inteng's). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// ONE field with no label is inteng's FUN_00474170 / FUN_004747f0 / FUN_00474bc0: 400 x 200, the field at 0x1e, 100, 300 x 0x12, Ok at 100, 0x96 and Cancel at 200, 0x96 (0x32 x 0x10).
// Fields with labels are FUN_00474350's: 400 x 250, each field 0x96 wide at 0x1e with its label at 0xbe, the rows 100, 0x7c, 0x94, Ok and Cancel at 0xbe. Both in the middle of the screen.

#include <stdio.h>

#include <string>
#include <vector>

#include "dialog_box.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

static bool Is(host::DialogRect const &r, int32_t x, int32_t y, int32_t w, int32_t h)
{
	return r.x == x && r.y == y && r.width == w && r.height == h;
}

static std::string Words(host::DialogRect const &r)
{
	return std::to_string(r.x) + " " + std::to_string(r.y) + " " + std::to_string(r.width) + " " + std::to_string(r.height);
}

int main()
{
	host::DialogRect const screen = { 0, 0, 1008, 729 };
	auto const width = [](std::string const &text) { return (int32_t)text.size() * 8; };

	// one field, no label: inteng's number box
	{
		host::InDialog box;
		host::DialogField field;

		field.text = "0";
		box.OpenText("Select alternate location group number\n(0 means no group)", { field }, screen, width);

		host::DialogRect const w = box.Window();

		Check(w.width == 400 && w.height == 200, "one field with no label: the window is 400 x 200 (0x47419c), it is " + Words(w));
		Check(w.x == (1008 - 400) / 2 && w.y == (729 - 200) / 2, "and in the middle of the screen (FUN_0057c800 0x57c80e), at " + Words(w));
		Check(box.Fields().size() == 1 && Is(box.Fields()[0].edit, w.x + 0x1e, w.y + 100, 300, 0x12), "its field at 0x1e, 100, 300 x 0x12 (0x47420c)" +
			(box.Fields().empty() ? std::string() : ", it is " + Words(box.Fields()[0].edit)));
		Check(box.Buttons().size() == 2 && Is(box.Buttons()[0].box, w.x + 100, w.y + 0x96, 0x32, 0x10) && Is(box.Buttons()[1].box, w.x + 200, w.y + 0x96, 0x32, 0x10),
			"Ok at 100, 0x96 and Cancel at 200, 0x96, 0x32 x 0x10 (0x474248 / 0x474287)" + (box.Buttons().size() < 2 ? std::string() : ", they are " + Words(box.Buttons()[0].box) + " and " + Words(box.Buttons()[1].box)));
		Check(box.TextArea().height == 0x28, "its text area 0x28 high (0x4741d6), it is " + std::to_string(box.TextArea().height));
	}

	// fields with labels: FUN_00474350's box
	{
		host::InDialog box;
		host::DialogField a, b;

		a.label = "x";
		b.label = "y";
		box.OpenText("Two numbers", { a, b }, screen, width);

		host::DialogRect const w = box.Window();

		Check(w.width == 400 && w.height == 0xfa, "fields with labels: the window is 400 x 250 (0x47437c), it is " + Words(w));
		Check(box.Fields().size() == 2 && Is(box.Fields()[0].edit, w.x + 0x1e, w.y + 100, 0x96, 0x12) && Is(box.Fields()[0].labelBox, w.x + 0xbe, w.y + 100, 0x96, 0x12) &&
			Is(box.Fields()[1].edit, w.x + 0x1e, w.y + 0x7c, 0x96, 0x12), "each field 0x96 wide at 0x1e, its label at 0xbe, the rows 100 and 0x7c (0x4743fd / 0x474438 / 0x4744eb)");
		Check(box.Buttons().size() == 2 && box.Buttons()[0].box.y == w.y + 0xbe, "Ok at 0xbe");
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return checksFailed ? 1 : 0;
}
