// texture_import_form.cpp: THE RIM IMPORT PAGE'S FORM, pure (texture_import_screen.h: its parts, their reading back, the picture endings; no Windows call, no
// editor state, so host_texture_import_form_test links only it). Written 2026-11-23 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
#include "texture_import_screen.h"

namespace
{
	// THE FORM'S PARTS, by their place in the spec (TextureImportScreen_Form makes them in this order; TextureImportScreen_Read reads them back so)
	size_t const kWords = 0, kNameEdit = 2, kSideFirst = 3, kModeFirst = 9, kSeeThrough = 15, kParts = 19;
	uint32_t const kSides[3] = { 128, 256, 512 };
	char const *const kSideWords[3] = { "128 x 128", "256 x 256", "512 x 512" };
	char const *const kModeWords[3] = { "Fit (all of it, bars round it)", "Crop (the middle that fills it)", "Stretch (all of it, out of shape)" };

	host::DialogWidget Part(host::DialogWidgetType type, host::DialogRect box, std::string const &text)
	{
		host::DialogWidget w;

		w.type = type;
		w.box = box;
		w.text = text;
		return w;
	}
}

bool host::TextureImportScreen_IsPicture(std::wstring const &path)
{
	size_t const dot = path.find_last_of(L'.');

	if (dot == std::wstring::npos || path.find_first_of(L"\\/", dot) != std::wstring::npos)
	{
		return false;
	}

	std::wstring ending = path.substr(dot);

	for (wchar_t &c : ending)
	{
		c = (c >= L'A' && c <= L'Z') ? (wchar_t)(c - L'A' + L'a') : c;
	}

	return ending == L".png" || ending == L".bmp";
}

host::DialogFormSpec host::TextureImportScreen_Form(std::string const &pictureWords, std::string const &name)
{
	// inteng's TEXT dialog's look (400 wide; a label 0x12 in, its edits 0x1e in at 0x12 a row; Ok and Cancel a quarter and a half across, 0x32 x 0x10),
	// widened to 460 for the three ways' words
	DialogFormSpec spec;

	spec.title = "Import texture";
	spec.width = 460;
	spec.widgets.push_back(Part(DialogWidgetType::Label, { 0x12, 0x12, 460 - 0x24, 0x24 }, "A texture for the open level, made from " + pictureWords +
		"\nand saved under graphics\\Custom: it is offered in Texture Map at once"));

	int32_t y = 0x3c;

	spec.widgets.push_back(Part(DialogWidgetType::Label, { 0x1e, y, 0x60, 0x12 }, "Name"));

	DialogWidget edit = Part(DialogWidgetType::Edit, { 0x80, y, 0x110, 0x12 }, name);

	edit.most = 32;   // RimSave_EntryName's most for a texture's name
	spec.widgets.push_back(edit);
	y += 0x1e;

	for (int s = 0; s < 3; s++)
	{
		DialogWidget box = Part(DialogWidgetType::Check, { 0x1e + s * 0x80, y, 12, 12 }, std::string());

		box.group = 0;
		box.checked = s == 0;   // 128, the default: the stock levels' habit (1,376 of the 1,377 textures their lists name); the retail game draws 256 and 512 at
		                        // their size too (plan R1, settled from its code 2026-11-23: BMHD's width and height taken as they are)
		spec.widgets.push_back(box);
		spec.widgets.push_back(Part(DialogWidgetType::Label, { 0x1e + s * 0x80 + 0x10, y, 0x6c, 0x12 }, kSideWords[s]));
	}

	y += 0x1e;

	for (int m = 0; m < 3; m++)
	{
		DialogWidget box = Part(DialogWidgetType::Check, { 0x1e, y, 12, 12 }, std::string());

		box.group = 1;
		box.checked = m == 0;   // Fit: nothing of the picture lost
		spec.widgets.push_back(box);
		spec.widgets.push_back(Part(DialogWidgetType::Label, { 0x1e + 0x10, y, 0x150, 0x12 }, kModeWords[m]));
		y += 0x14;
	}

	DialogWidget seeThrough = Part(DialogWidgetType::Check, { 0x1e, y + 4, 12, 12 }, std::string());

	seeThrough.checked = false;   // black bars, as the engine's default
	spec.widgets.push_back(seeThrough);
	spec.widgets.push_back(Part(DialogWidgetType::Label, { 0x1e + 0x10, y + 4, 0x150, 0x12 }, "Fit's bars see-through"));
	y += 0x24;

	DialogWidget ok = Part(DialogWidgetType::Button, { spec.width / 4, y, 0x32, 0x10 }, "Ok");
	DialogWidget cancel = Part(DialogWidgetType::Button, { spec.width / 2, y, 0x32, 0x10 }, "Cancel");

	ok.answer = kDialogOk;
	cancel.answer = kDialogEscape;
	spec.widgets.push_back(ok);
	spec.widgets.push_back(cancel);
	spec.height = y + 0x3c;
	return spec;
}

bool host::TextureImportScreen_Read(std::vector<DialogWidget> const &widgets, TextureImportAnswers *answers)
{
	if (!answers || widgets.size() != kParts || widgets[kWords].type != DialogWidgetType::Label || widgets[kNameEdit].type != DialogWidgetType::Edit ||
		widgets[kSeeThrough].type != DialogWidgetType::Check)
	{
		return false;
	}

	answers->name = widgets[kNameEdit].text;
	answers->side = 0;
	answers->mode = -1;

	for (int s = 0; s < 3; s++)
	{
		DialogWidget const &box = widgets[kSideFirst + (size_t)s * 2];

		answers->side = box.type == DialogWidgetType::Check && box.checked ? kSides[s] : answers->side;
	}

	for (int m = 0; m < 3; m++)
	{
		DialogWidget const &box = widgets[kModeFirst + (size_t)m * 2];

		answers->mode = box.type == DialogWidgetType::Check && box.checked ? m : answers->mode;
	}

	answers->transparentBars = widgets[kSeeThrough].checked;
	return answers->side != 0 && answers->mode >= 0;
}
