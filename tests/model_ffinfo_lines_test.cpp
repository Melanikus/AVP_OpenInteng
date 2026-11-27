// The test for an exported level's ffinfo.txt lines, and adding them to a player's file (src/model/ffinfo_lines.h). Written 2026-11-27 by Melanikus (Frost)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Made by hand here, so it needs none of the game's files and
// GitHub's runner runs it; the game's own ffinfo.txt is model_ffinfo_lines_stock_test's (the team's runner copies it beside that test: {FASTFILE:ffinfo.txt}).
//   (1) a line: "<folder>\;<pack>", the '\' added
//   (2) every refusal of a line, by its own words: the folder's rule, its small first letter, its '/', its length; the pack's letters, ".FFL", length,
//       and a stock pack's name; and the edges taken (199 letters, 40 letters)
//   (3) added to a file: every byte of it kept, ours after it with its own line ending (CRLF, LF, none: CRLF), a last line with no ending given one first
//   (4) what's there already: the same line in other capitals isn't added again (the file comes back byte for byte); the same folder with another pack is
//       added (the owner's Q3: a level keeping a stock folder); the same pack under another folder is refused
//   (5) a player's file refused, each by its own words: over 64 KB, a NUL, Ctrl-Z, a tab, a byte over 126, a lone CR, a line of 250 letters (249 taken),
//       a file our lines would take over 64 KB; a line that isn't one FfInfo_Line makes; a name from the file in the words shown safely
// RED FIRST (2026-11-27): on a stand-in that refuses everything, 49 of the 50 checks failed. The one that passed compared two refused answers with each
// other; it now checks the file itself too.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "ffinfo_lines.h"

using namespace model;

typedef std::vector<uint8_t> Bytes;

static int checks = 0;
static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static Bytes B(std::string const &text)
{
	return Bytes(text.begin(), text.end());
}

static std::string Line(std::string const &folder, std::string const &pack)
{
	std::string line;
	std::string why;

	return FfInfo_Line(folder, pack, &line, &why) && why.empty() ? line : std::string("(refused: ") + why + ")";
}

// The line is refused, its words holding `words`, and nothing given back
static void LineRefused(std::string const &folder, std::string const &pack, std::string const &words, std::string const &what)
{
	std::string line = "x";
	std::string why;
	bool const made = FfInfo_Line(folder, pack, &line, &why);

	Check(!made && line.empty() && why.find(words) != std::string::npos, "(2) refused: " + what + " (\"" + why + "\")");
}

// Adds and gives back the new file, or "(refused: ...)" as bytes
static Bytes Add(Bytes const &existing, std::vector<std::string> const &lines, size_t *added)
{
	Bytes out(3, 'x');
	std::string why;
	*added = 99;

	return FfInfo_Add(existing, lines, &out, added, &why) && why.empty() ? out : B("(refused: " + why + ")");
}

// The file is refused for `words`, with nothing given back
static void AddRefused(Bytes const &existing, std::vector<std::string> const &lines, std::string const &words, std::string const &what)
{
	Bytes out(3, 'x');
	size_t added = 99;
	std::string why;
	bool const done = FfInfo_Add(existing, lines, &out, &added, &why);

	Check(!done && out.empty() && added == 0 && why.find(words) != std::string::npos, "(5) refused: " + what + " (\"" + why + "\")");
}

int main()
{
	std::string const ours = "sound\\Custom\\MyLevel\\;Custom_MyLevel_Snd.FFL";

	// (1)
	Check(Line("sound\\Custom\\MyLevel", "Custom_MyLevel_Snd.FFL") == ours, "(1) the line is the folder, a '\\', a ';' and the pack: " + Line("sound\\Custom\\MyLevel", "Custom_MyLevel_Snd.FFL"));
	Check(Line("graphics\\Custom\\My Level", "Custom_My_Level_Tex.FFL") == "graphics\\Custom\\My Level\\;Custom_My_Level_Tex.FFL", "(1) a folder with a space, as stock's have");

	// (2)
	std::string const folder199 = "sound\\" + std::string(193, 'a');
	std::string const pack40 = std::string(36, 'P') + ".FFL";
	Check(folder199.size() == kFfInfoMostFolderLetters && Line(folder199, "Custom_X.FFL") == folder199 + "\\;Custom_X.FFL", "(2) a folder of 199 letters is taken");
	Check(pack40.size() == kFfInfoMostPackLetters && Line("sound\\x", pack40) == "sound\\x\\;" + pack40, "(2) a pack of 40 letters is taken");
	LineRefused("", "Custom_X.FFL", "folder", "an empty folder");
	LineRefused(folder199 + "a", "Custom_X.FFL", "199", "a folder of 200 letters");
	LineRefused("Sound\\Custom\\X", "Custom_X.FFL", "small", "a folder starting with a capital (the game would keep its pack loaded for good)");
	LineRefused("1sound\\x", "Custom_X.FFL", "small", "a folder starting with a digit (the game would never read the line)");
	LineRefused("sound/Custom/X", "Custom_X.FFL", "'/'", "a folder with '/'");
	LineRefused("sound\\..\\X", "Custom_X.FFL", "rule", "a folder going up");
	LineRefused("sound\\X\\", "Custom_X.FFL", "rule", "a folder given with its last '\\' (an empty part)");
	LineRefused("sound\\X;Y", "Custom_X.FFL", "rule", "a folder with a ';'");
	LineRefused("sound\\x", "", "pack", "an empty pack");
	LineRefused("sound\\x", ".FFL", "pack", "a pack that is only \".FFL\"");
	LineRefused("sound\\x", "Custom_X.ffl", ".FFL", "a pack ending \".ffl\"");
	LineRefused("sound\\x", "Custom_X", ".FFL", "a pack with no \".FFL\"");
	LineRefused("sound\\x", "Custom-X.FFL", "letters, digits and _", "a pack with a '-'");
	LineRefused("sound\\x", "Custom X.FFL", "letters, digits and _", "a pack with a space");
	LineRefused("sound\\x", "x\\Custom_X.FFL", "letters, digits and _", "a pack with a folder in it");
	LineRefused("sound\\x", "P" + pack40, "40", "a pack of 41 letters");
	LineRefused("sound\\x", "Snd25.FFL", "stock", "stock's Snd25.FFL");
	LineRefused("sound\\x", "tex3.FFL", "stock", "stock's Tex3.FFL in small letters");
	LineRefused("sound\\x", "Common.FFL", "stock", "stock's common.ffl");

	// (3) added to a file
	std::string const stockish = "Graphics\\Common;Tex1.FFL\r\nsound\\Env\\Temple;Snd25.FFL\r\nsound\\Npc\\Queen;Snd50.FFL\r\n";
	size_t added = 0;
	Bytes after = Add(B(stockish), { ours }, &added);
	Check(after == B(stockish + ours + "\r\n") && added == 1, "(3) a CRLF file keeps every byte, and gets the line at its end with CRLF");

	Check(Add(B("sound\\a;Snd1.FFL\nsound\\b;Snd2.FFL\n"), { ours }, &added) == B("sound\\a;Snd1.FFL\nsound\\b;Snd2.FFL\n" + ours + "\n") && added == 1,
		"(3) an LF file gets it with LF");
	Check(Add(Bytes(), { ours }, &added) == B(ours + "\r\n") && added == 1, "(3) an empty file gets it with CRLF, as stock's");
	Check(Add(B("sound\\a;Snd1.FFL\r\nsound\\b;Snd2.FFL"), { ours }, &added) == B("sound\\a;Snd1.FFL\r\nsound\\b;Snd2.FFL\r\n" + ours + "\r\n") && added == 1,
		"(3) a last line with no ending gets the file's own first");
	Check(Add(B("sound\\a;Snd1.FFL"), { ours }, &added) == B("sound\\a;Snd1.FFL\r\n" + ours + "\r\n") && added == 1, "(3) and a one-line file with none, CRLF");
	Check(Add(B("; a comment\r\n\r\n"), { ours }, &added) == B("; a comment\r\n\r\n" + ours + "\r\n") && added == 1,
		"(3) a line the game skips (not starting with a letter) and an empty one are kept as they are");

	// (4) what's there already
	Bytes again = Add(after, { ours }, &added);
	Check(after == B(stockish + ours + "\r\n") && again == after && added == 0, "(4) the line already there: nothing added, the file byte for byte as it was (Install can run twice)");
	Check(Add(B("SOUND\\CUSTOM\\MYLEVEL\\;custom_mylevel_snd.ffl\r\n"), { ours }, &added) == B("SOUND\\CUSTOM\\MYLEVEL\\;custom_mylevel_snd.ffl\r\n") && added == 0,
		"(4) and so in other capitals");
	std::string const temple = Line("sound\\env\\Temple", "Custom_MyLevel_Snd.FFL");
	Check(Add(B(stockish), { temple }, &added) == B(stockish + temple + "\r\n") && added == 1,
		"(4) the owner's Q3: a level keeping stock's env\\Temple gets its own pack after stock's Snd25 (" + temple + ")");
	AddRefused(B("sound\\Custom\\Other\\;Custom_MyLevel_Snd.FFL\r\n"), { ours }, "already", "the same pack listed under another folder");
	std::string const second = Line("graphics\\Custom\\MyLevel", "Custom_MyLevel_Tex.FFL");
	Check(Add(B(stockish), { ours, second }, &added) == B(stockish + ours + "\r\n" + second + "\r\n") && added == 2, "(4) two lines go in, in their order");
	Check(Add(B(stockish), { ours, ours }, &added) == B(stockish + ours + "\r\n") && added == 1, "(4) the same line twice in the list goes in once");
	AddRefused(B(stockish), { ours, Line("sound\\Custom\\Other", "Custom_MyLevel_Snd.FFL") }, "already", "two of ours with one pack under two folders");

	// (5) a player's file refused
	AddRefused(Bytes(kFfInfoMostFileBytes + 1, 'a'), { ours }, "64 KB", "a file over 64 KB");
	AddRefused(B(std::string("sound\\a;Snd1.FFL\r\n") + '\0' + "x\r\n"), { ours }, "byte", "a NUL");
	AddRefused(B("sound\\a;Snd1.FFL\r\n\x1a\r\n"), { ours }, "byte", "Ctrl-Z (text mode ends the file there)");
	AddRefused(B("sound\\a;Snd1.FFL\t\r\n"), { ours }, "byte", "a tab");
	AddRefused(B("sound\\\xe9;Snd1.FFL\r\n"), { ours }, "byte", "a byte over 126");
	AddRefused(B("\xef\xbb\xbfsound\\a;Snd1.FFL\r\n"), { ours }, "byte", "a UTF-8 mark at the start (Notepad's)");
	AddRefused(B("sound\\a;Snd1.FFL\rsound\\b;Snd2.FFL\r\n"), { ours }, "CR", "a CR not followed by LF");
	AddRefused(B("sound\\a;Snd1.FFL\r\n" + std::string(250, 'a') + "\r\n"), { ours }, "250", "a line of 250 letters");
	{
		Bytes ok249 = B("sound\\a;Snd1.FFL\r\n" + std::string(249, 'a') + "\r\n");
		Check(Add(ok249, { ours }, &added).size() == ok249.size() + ours.size() + 2 && added == 1, "(5) a line of 249 letters is taken");
	}
	AddRefused(B(std::string(kFfInfoMostFileBytes - 10, '\n')), { ours }, "64 KB", "a file our line would take over 64 KB");
	AddRefused(B(stockish), { "sound\\x;Snd1.FFL" }, "isn't a line", "a line FfInfo_Line wouldn't make (no '\\', a stock pack)");
	AddRefused(B(stockish), { "Sound\\X\\;Custom_X.FFL" }, "isn't a line", "a line with a capital first letter");
	AddRefused(B(stockish), { ours + "\r\n" }, "isn't a line", "a line with its own line ending");
	AddRefused(B("sound\\a*b\\;Custom_MyLevel_Snd.FFL\r\n"), { ours }, "sound\\a?b\\", "a name from the file in the words, its odd letter shown as '?'");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
