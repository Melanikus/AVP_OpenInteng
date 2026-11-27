// ffinfo_lines.cpp: an exported level's ffinfo.txt lines, and adding them to a player's file (ffinfo_lines.h says each rule and the line of Rebellion's
// FFSTDIO.CPP it comes from). Written 2026-11-27 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "ffinfo_lines.h"

#include "level_file_name.h"   // LevelFileName_WhyRefused: the game's own rule for a name from a level; LevelFileName_ForTheLog: one shown safely

namespace model
{
	namespace
	{
		char const kFfl[] = ".FFL";
		size_t const kFflLetters = 4;

		// A letter made small, by itself, so no locale can change it (everything compared here is 32 to 126)
		char Small(char c)
		{
			return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
		}

		// The same to the game, capitals aside (_stricmp, _strnicmp)
		bool SameAsTheGame(std::string const &a, std::string const &b)
		{
			if (a.size() != b.size())
			{
				return false;
			}

			for (size_t i = 0; i < a.size(); i++)
			{
				if (Small(a[i]) != Small(b[i]))
				{
					return false;
				}
			}

			return true;
		}

		// A name from the player's file, safe in words (the file is hostile): cut short, its odd letters shown as '?'
		std::string Shown(std::string const &name)
		{
			char line[211];

			LevelFileName_ForTheLog(name.c_str(), line, sizeof(line));
			return line;
		}

		// A stock pack's name, without ".FFL": Snd<N>, Tex<N> or common, in any capitals (the game's fastfile folder holds Snd1-51, Tex1-58 and common.ffl)
		bool IsStock(std::string const &base)
		{
			std::string small;

			for (char c : base)
			{
				small += Small(c);
			}

			if (small == "common")
			{
				return true;
			}

			if (small.size() > 3 && (small.compare(0, 3, "snd") == 0 || small.compare(0, 3, "tex") == 0))
			{
				for (size_t i = 3; i < small.size(); i++)
				{
					if (small[i] < '0' || small[i] > '9')
					{
						return false;
					}
				}

				return true;
			}

			return false;
		}

		// Why a folder can't be a line's (A LINE OF OURS), in words, or nothing when it can
		std::string FolderWhy(std::string const &folder)
		{
			if (folder.empty())
			{
				return "the folder is empty";
			}

			if (folder.size() > kFfInfoMostFolderLetters)
			{
				return "the folder is longer than 199 letters";
			}

			if (folder.find('/') != std::string::npos)
			{
				return "the folder has a '/' (the game asks for its files with '\\')";
			}

			char const *const refused = LevelFileName_WhyRefused(folder.c_str());

			if (refused)
			{
				return std::string("the folder breaks the game's rule for a name from a level: ") + refused;
			}

			if (folder[0] < 'a' || folder[0] > 'z')
			{
				return "the folder must start with a small letter (the game skips a line that doesn't start with a letter, and keeps a capital's pack loaded for the whole game)";
			}

			return std::string();
		}

		// Why a pack's name can't be a line's, in words, or nothing when it can
		std::string PackWhy(std::string const &pack)
		{
			if (pack.empty())
			{
				return "the pack's name is empty";
			}

			if (pack.size() < kFflLetters || pack.compare(pack.size() - kFflLetters, kFflLetters, kFfl) != 0)
			{
				return "the pack's name must end with .FFL";
			}

			std::string const base = pack.substr(0, pack.size() - kFflLetters);

			if (base.empty())
			{
				return "the pack's name has nothing before .FFL";
			}

			for (char c : base)
			{
				bool const plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';

				if (!plain)
				{
					return "the pack's name may hold only letters, digits and _ before .FFL";
				}
			}

			if (pack.size() > kFfInfoMostPackLetters)
			{
				return "the pack's name is longer than 40 letters";
			}

			if (IsStock(base))
			{
				return "the pack's name is a stock pack's (Snd<N>, Tex<N> or common), which the game would take for that one";
			}

			return std::string();
		}

		// One line of a file, as the game reads it: up to its LF, a CR before that taken off (text mode)
		struct FileLine
		{
			std::string text;
			bool ended = false;   // it has a line ending
		};

		// A line of ours, taken apart: false unless it's exactly what FfInfo_Line makes
		bool OursApart(std::string const &line, std::string *folder, std::string *pack)
		{
			size_t const semi = line.find(';');

			if (semi == std::string::npos || semi == 0 || line[semi - 1] != '\\')
			{
				return false;
			}

			*folder = line.substr(0, semi - 1);
			*pack = line.substr(semi + 1);

			std::string remade;
			std::string why;

			return FfInfo_Line(*folder, *pack, &remade, &why) && remade == line;
		}

		bool No(std::vector<uint8_t> *out, size_t *added, std::string *why, std::string const &words)
		{
			out->clear();
			*added = 0;
			*why = words;
			return false;
		}
	}

	bool FfInfo_Line(std::string const &folder, std::string const &pack, std::string *line, std::string *why)
	{
		line->clear();
		why->clear();

		std::string refused = FolderWhy(folder);

		if (refused.empty())
		{
			refused = PackWhy(pack);
		}

		if (!refused.empty())
		{
			*why = refused;
			return false;
		}

		*line = folder + "\\;" + pack;
		return true;
	}

	bool FfInfo_Add(std::vector<uint8_t> const &existing, std::vector<std::string> const &lines, std::vector<uint8_t> *out, size_t *added, std::string *why)
	{
		out->clear();
		*added = 0;
		why->clear();

		if (existing.size() > kFfInfoMostFileBytes)
		{
			return No(out, added, why, "the player's ffinfo.txt is over 64 KB, more than any we add to (stock's is 3,819 bytes)");
		}

		// THE PLAYER'S FILE, byte by byte, as text mode reads it (ffinfo_lines.h, ADDED TO A PLAYER'S FILE)
		std::vector<FileLine> fileLines;
		bool anyCrLf = false;
		bool anyLf = false;
		FileLine now;

		for (size_t i = 0; i < existing.size(); i++)
		{
			uint8_t const c = existing[i];

			if (c == '\r')
			{
				if (i + 1 >= existing.size() || existing[i + 1] != '\n')
				{
					return No(out, added, why, "the player's ffinfo.txt has a CR not followed by LF, on line " + std::to_string(fileLines.size() + 1) +
						" (the game would read it as part of a pack's name)");
				}

				continue;   // the LF next ends the line
			}

			if (c == '\n')
			{
				if (i > 0 && existing[i - 1] == '\r')
				{
					anyCrLf = true;
				}
				else
				{
					anyLf = true;
				}

				now.ended = true;
				fileLines.push_back(now);
				now = FileLine();
				continue;
			}

			if (c < 32 || c > 126)
			{
				return No(out, added, why, "the player's ffinfo.txt has a byte the game's reader can't take (" + std::to_string(c) + ") on line " +
					std::to_string(fileLines.size() + 1) + ": a NUL ends a line there, Ctrl-Z (26) ends the file, and the game's own file has none of them");
			}

			if (now.text.size() >= kFfInfoMostLineLetters)
			{
				return No(out, added, why, "the player's ffinfo.txt has a line of 250 letters or more (line " + std::to_string(fileLines.size() + 1) +
					"): the game's own reader would overflow on it before it reached ours (stock's longest is 55)");
			}

			now.text += (char)c;
		}

		bool const lastHasNoEnding = !now.text.empty();

		if (lastHasNoEnding)
		{
			fileLines.push_back(now);
		}

		// The file's own line ending: CRLF when it has any (stock's), else LF, and CRLF for a file with none at all
		std::string const ending = anyCrLf || !anyLf ? "\r\n" : "\n";

		// What the game takes from it (a line starting with a letter, cut at its first ';'), to compare ours with
		struct Taken
		{
			std::string folder;
			std::string pack;
		};
		std::vector<Taken> taken;

		for (FileLine const &l : fileLines)
		{
			char const first = l.text.empty() ? 0 : l.text[0];
			size_t const semi = l.text.find(';');

			if (((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z')) && semi != std::string::npos)
			{
				taken.push_back({ l.text.substr(0, semi), l.text.substr(semi + 1) });
			}
		}

		std::string toAdd;
		size_t count = 0;

		for (size_t k = 0; k < lines.size(); k++)
		{
			Taken mine;

			if (!OursApart(lines[k], &mine.folder, &mine.pack))
			{
				return No(out, added, why, "line " + std::to_string(k + 1) + " to add isn't a line FfInfo_Line makes");
			}

			mine.folder += '\\';

			bool there = false;

			for (Taken const &t : taken)
			{
				bool const samePack = SameAsTheGame(t.pack, mine.pack);
				bool const sameFolder = SameAsTheGame(t.folder, mine.folder);

				if (samePack && sameFolder)
				{
					there = true;
					break;
				}

				if (samePack)
				{
					return No(out, added, why, "the pack " + mine.pack + " is already listed for another folder, " + Shown(t.folder) +
						" (the game takes two lines with one pack as one, and would never load it for the second)");
				}
			}

			if (there)
			{
				continue;
			}

			toAdd += lines[k] + ending;
			taken.push_back(mine);
			count++;
		}

		size_t const total = existing.size() + (count && lastHasNoEnding ? ending.size() : 0) + toAdd.size();

		if (total > kFfInfoMostFileBytes)
		{
			return No(out, added, why, "the player's ffinfo.txt would be over 64 KB with our lines");
		}

		out->assign(existing.begin(), existing.end());

		if (count)
		{
			if (lastHasNoEnding)
			{
				out->insert(out->end(), ending.begin(), ending.end());
			}

			out->insert(out->end(), toAdd.begin(), toAdd.end());
		}

		*added = count;
		return true;
	}
}
