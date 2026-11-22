// The test for Load > Browse... and a SHORTCUT (src/host/video_mode.cpp VideoMode_Browse; Vasquez's Video mode review, the NIT; Drake 11-22). Windows' Open dialog, asked
// without OFN_NODEREFERENCELINKS, gives back a picked shortcut (.lnk) as its TARGET, and checks that target is there itself first: for a target on \\server\..., the dialog
// contacts that computer before any check of ours sees the path. With the flag, the .lnk comes back as itself, and the drop's check (video_drop.h, the one list of endings)
// refuses it as no video.
// HOW: the dialog is modal and needs someone at the screen, so it is not driven here. This reads VideoMode_Browse's flags from the source (as editor_start_test checks the
// editor's start), and runs the real check on a .lnk path. Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include <stdio.h>

#include <string>
#include <vector>

#include "video_drop.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static std::string FileText(char const *path)
{
	std::string text;
	FILE *file = nullptr;

	if (fopen_s(&file, path, "rb") != 0 || !file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0 && text.size() < (8u << 20))
	{
		text.append(block, got);
	}

	fclose(file);
	return text;
}

int main()
{
	std::string const source = FileText("src/host/video_mode.cpp");
	size_t const browse = source.find("void host::VideoMode_Browse(void *owner)");
	size_t const flagsAt = browse == std::string::npos ? std::string::npos : source.find("open.Flags =", browse);
	std::string const flags = flagsAt == std::string::npos ? std::string() : source.substr(flagsAt, source.find(';', flagsAt) - flagsAt);

	Check(!flags.empty(), "VideoMode_Browse's dialog flags are found: " + flags);
	Check(flags.find("OFN_NODEREFERENCELINKS") != std::string::npos, "they hold OFN_NODEREFERENCELINKS: a picked shortcut comes back as itself, never as its target");
	Check(flags.find("OFN_FILEMUSTEXIST") != std::string::npos && flags.find("OFN_NONETWORKBUTTON") != std::string::npos, "and still OFN_FILEMUSTEXIST and OFN_NONETWORKBUTTON");

	// the .lnk then meets the drop's own check, which takes only the one list of video endings
	std::wstring path;
	std::string why;
	bool const taken = host::VideoDrop_Check({ L"C:\\Users\\someone\\Videos\\clip.mp4.lnk" }, 1, &path, &why);

	Check(!taken && path.empty() && !why.empty(), "a picked shortcut, clip.mp4.lnk, is refused by the drop's check, nothing taken (" + why + ")");

	printf("%s: %d failed\n", checksFailed ? "FAILED" : "ok", checksFailed);
	return checksFailed ? 1 : 0;
}
