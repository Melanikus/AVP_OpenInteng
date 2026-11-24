// The test for the RIM import page's form (src/host/texture_import_screen.h: TextureImportScreen_Form, TextureImportScreen_Read, TextureImportScreen_IsPicture),
// pure: no Windows call, no editor. Written 2026-11-23 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//   THE FORM: titled Import texture, its words name the picture, its name box starts with the name given, 128 and Fit ticked and see-through not, Ok and Cancel
//   answering Ok and Escape, every part inside the window, no two parts over each other; 128 x 128 the one size ticked as it opens (Bishop 2, 2026-11-23:
//   the stock levels' habit; 256 and 512 offered plainly, the retail game drawing them at their size, plan R1 settled from its code)
//   READ BACK: as it opens, 128 / Fit / no; ticked otherwise, 512 / Stretch / yes, the name typed; a list that isn't the form's (one part missing) is refused
//   THE ENDINGS: .png, .bmp, .jpg and .jpeg, any capitals; .gif, .jpe, .mp4, no ending, and a dot in a folder's name only, refused (.jpg and .jpeg: Drake
//   2026-11-24, the owner's "Add JPEG in beta.0"; red on 2143de03, PNG and BMP only)
// The program gives back a non-zero result if any check failed.

#include <stdio.h>

#include <string>
#include <vector>

#include "texture_import_screen.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static bool Inside(DialogRect const &r, DialogFormSpec const &spec)
{
	return r.x >= 0 && r.y >= 0 && r.x + r.width <= spec.width && r.y + r.height <= spec.height;
}

static bool Over(DialogRect const &a, DialogRect const &b)
{
	return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

int main()
{
	puts("the RIM import page's form");

	DialogFormSpec const spec = TextureImportScreen_Form("wall.png", "wall");

	Check(spec.title == "Import texture" && spec.widgets.size() == 19, "the form: Import texture, 19 parts (" + std::to_string(spec.widgets.size()) + ")");

	if (spec.widgets.size() != 19)
	{
		printf("SOME CHECKS FAILED\n");
		return 1;
	}

	Check(spec.widgets[0].type == DialogWidgetType::Label && spec.widgets[0].text.find("wall.png") != std::string::npos &&
		spec.widgets[0].text.find("graphics\\Custom") != std::string::npos, "its words name the picture and where the texture goes: \"" + spec.widgets[0].text + "\"");
	Check(spec.widgets[2].type == DialogWidgetType::Edit && spec.widgets[2].text == "wall" && spec.widgets[2].most == 32, "its name box starts with the name given, 32 letters at most");

	bool inside = true, apart = true;

	for (size_t i = 0; i < spec.widgets.size(); i++)
	{
		inside = inside && Inside(spec.widgets[i].box, spec);

		for (size_t j = i + 1; j < spec.widgets.size(); j++)
		{
			apart = apart && !Over(spec.widgets[i].box, spec.widgets[j].box);
		}
	}

	Check(inside, "every part inside the window (" + std::to_string(spec.width) + " x " + std::to_string(spec.height) + ")");
	Check(apart, "no two parts over each other");
	Check(spec.widgets[17].type == DialogWidgetType::Button && spec.widgets[17].text == "Ok" && spec.widgets[17].answer == kDialogOk &&
		spec.widgets[18].type == DialogWidgetType::Button && spec.widgets[18].text == "Cancel" && spec.widgets[18].answer == kDialogEscape, "Ok answers Ok, Cancel answers Escape");

	Check(spec.widgets[3].checked && !spec.widgets[5].checked && !spec.widgets[7].checked, "128 x 128 is the size ticked as it opens, 256 and 512 not");

	// READ BACK
	TextureImportAnswers answers;

	Check(TextureImportScreen_Read(spec.widgets, &answers) && answers.name == "wall" && answers.side == 128 && answers.mode == 0 && !answers.transparentBars,
		"as it opens it reads 128, Fit, bars black, the name given");

	std::vector<DialogWidget> changed = spec.widgets;

	changed[2].text = "wall01";
	changed[3].checked = false;   // 128
	changed[7].checked = true;    // 512
	changed[9].checked = false;   // Fit
	changed[13].checked = true;   // Stretch
	changed[15].checked = true;   // see-through
	Check(TextureImportScreen_Read(changed, &answers) && answers.name == "wall01" && answers.side == 512 && answers.mode == 2 && answers.transparentBars,
		"ticked otherwise it reads 512, Stretch, see-through, the name typed");

	std::vector<DialogWidget> shorter(spec.widgets.begin(), spec.widgets.end() - 1);

	Check(!TextureImportScreen_Read(shorter, &answers), "a list of parts that isn't the form's is refused (then nothing is imported)");

	// THE ENDINGS
	Check(TextureImportScreen_IsPicture(L"C:\\pics\\wall.png") && TextureImportScreen_IsPicture(L"C:\\pics\\WALL.BMP") && TextureImportScreen_IsPicture(L"C:\\a.Png"),
		".png and .bmp are taken, any capitals");
	Check(TextureImportScreen_IsPicture(L"C:\\pics\\photo.jpg") && TextureImportScreen_IsPicture(L"C:\\pics\\PHOTO.JPEG") && TextureImportScreen_IsPicture(L"C:\\a.Jpg"),
		".jpg and .jpeg are taken too, any capitals (Drake 2026-11-24, the owner's \"Add JPEG in beta.0\")");
	Check(!TextureImportScreen_IsPicture(L"C:\\pics\\wall.gif") && !TextureImportScreen_IsPicture(L"C:\\pics\\wall.jpe") && !TextureImportScreen_IsPicture(L"C:\\clips\\a.mp4") &&
		!TextureImportScreen_IsPicture(L"C:\\pics\\wall") && !TextureImportScreen_IsPicture(L"C:\\my.png\\wall") && !TextureImportScreen_IsPicture(L"C:\\my.jpg\\wall"),
		".gif, .jpe, .mp4, no ending, and a dot only in a folder's name are not");

	printf(checksFailed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return checksFailed ? 1 : 0;
}
