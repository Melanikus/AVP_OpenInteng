// screen_names.h: what the editor DRAWS never shows a full path. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng (alpha.1, Bishop's privacy item: Hicks found
// Sound Edit's title "Sound Edit - C:\Users\<name>\..." and "Saved a copy: C:\Users\..." in pictures for the wiki, so the user's name was in every screenshot of them).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
//   ScreenNames_Cut(words)   the words with every full path in them cut to its last part: a file's NAME ("Invasion.RIF"), or a folder's when the path ends with \ or /
//                            ("C:\Users\me\sound\" -> "sound"). The rest of the words is kept as it is.
//
// What a full path is: the same rule as the screenshot's Shot_ScrubPaths (shot_context.cpp): a drive letter, a colon and \ or / ("C:\", "d:/"), not inside a word; or a network
// path's two backslashes and a name ("\\server\share\..."). Its folders are the parts that a \ or / follows (spaces inside a folder's name are part of it; a colon ends a part); the
// last part ends at a space, a colon, a quote, a character Windows never allows in a name, a tab, a line's end, a comma or a semicolon. A relative path ("avp_rifs\Invasion.RIF")
// holds no folder of the user's and is kept. The log file keeps full paths: this is for the screen only (the status line's words, the dialogs' texts, message boxes).
// HOSTILE WORDS: the text may come from a level or a file name anyone makes. One pass, every index checked against the length; nothing is ever read past the end.

#pragma once

#include <stddef.h>

#include <string>

namespace host
{
	namespace screen_names_detail
	{
		inline bool Letter(char c)
		{
			return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
		}

		inline bool LetterOrDigit(char c)
		{
			return Letter(c) || (c >= '0' && c <= '9');
		}

		inline bool Separator(char c)
		{
			return c == '\\' || c == '/';
		}

		inline bool PartEnds(char c)
		{
			return c == '"' || c == '\'' || c == '<' || c == '>' || c == '|' || c == '*' || c == '?' || c == '\r' || c == '\n' || c == '\t' || c == ',' || c == ';' || c == 0;
		}
	}

	inline std::string ScreenNames_Cut(std::string const &words)
	{
		using namespace screen_names_detail;

		std::string out;
		size_t const n = words.size();
		size_t i = 0;

		out.reserve(n);

		while (i < n)
		{
			bool const drive = i + 2 < n && Letter(words[i]) && words[i + 1] == ':' && Separator(words[i + 2]) && (i == 0 || !LetterOrDigit(words[i - 1]));
			bool const network = !drive && i + 2 < n && words[i] == '\\' && words[i + 1] == '\\' && !Separator(words[i + 2]) && !PartEnds(words[i + 2]) && words[i + 2] != ' ';

			if (!drive && !network)
			{
				out += words[i];
				i++;
				continue;
			}

			// the folders: each part a \ or / follows; the last of them is kept, for a path that ends with a separator
			size_t part = drive ? i + 3 : i + 2;
			size_t folder = part, folderEnd = part;

			for (;;)
			{
				size_t end = part;

				while (end < n && !PartEnds(words[end]) && !Separator(words[end]) && words[end] != ':')
				{
					end++;
				}

				if (end < n && Separator(words[end]))
				{
					if (end > part)
					{
						folder = part;
						folderEnd = end;
					}

					part = end + 1;
					continue;
				}

				break;
			}

			size_t end = part;

			while (end < n && !PartEnds(words[end]) && !Separator(words[end]) && words[end] != ' ' && words[end] != ':')
			{
				end++;
			}

			if (end > part)
			{
				out.append(words, part, end - part);   // the file's name
			}
			else if (folderEnd > folder)
			{
				out.append(words, folder, folderEnd - folder);   // a folder's path: its last folder's name
			}
			else
			{
				out.append(words, i, part - i);   // "C:\" alone: a drive names no one
			}

			i = end;
		}

		return out;
	}
}
