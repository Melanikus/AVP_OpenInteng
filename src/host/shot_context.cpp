// shot_context.cpp: see shot_context.h. Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng (#78). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "shot_context.h"

#include <stdio.h>

namespace host
{
	namespace
	{
		bool Letter(char c)
		{
			return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
		}

		bool LetterOrDigit(char c)
		{
			return Letter(c) || (c >= '0' && c <= '9');
		}

		bool Separator(char c)
		{
			return c == '\\' || c == '/';
		}

		// where a path's part always ends: a quote, a character Windows never allows in a name, a line's end, a tab, a comma or a semicolon
		bool PartEnds(char c)
		{
			return c == '"' || c == '\'' || c == '<' || c == '>' || c == '|' || c == '*' || c == '?' || c == '\r' || c == '\n' || c == '\t' || c == ',' || c == ';' || c == 0;
		}

		void AddNumber(std::string *out, long long n)
		{
			*out += std::to_string(n);
		}

		void AddMetres(std::string *out, int32_t millimetres)
		{
			char text[48];

			_snprintf_s(text, sizeof(text), _TRUNCATE, "%.3f", (double)millimetres / 1000.0);
			*out += text;
		}

		void AddText(std::string *out, char const *field, std::string const &text, size_t most)
		{
			*out += ",\"";
			*out += field;
			*out += "\":";
			*out += Shot_JsonString(Shot_ScrubPaths(text), most);
		}

		void AddList(std::string *out, char const *field, std::vector<std::string> const &list, size_t most, size_t first = 0)
		{
			*out += ",\"";
			*out += field;
			*out += "\":[";

			for (size_t i = first; i < list.size() && i - first < most; i++)
			{
				*out += i > first ? "," : "";
				*out += Shot_JsonString(Shot_ScrubPaths(list[i]), kShotMostNameBytes);
			}

			*out += "]";
		}

		std::string SafePart(std::string const &text, char const *ifEmpty)
		{
			std::string safe;

			for (char const c : text)
			{
				if (safe.size() >= 48)
				{
					break;
				}

				safe += LetterOrDigit(c) || c == '_' || c == '-' ? c : '_';
			}

			bool anyLetter = false;

			for (char const c : safe)
			{
				anyLetter = anyLetter || LetterOrDigit(c);
			}

			return anyLetter ? safe : std::string(ifEmpty);
		}
	}

	std::string Shot_ScrubPaths(std::string const &words)
	{
		std::string out;
		size_t const n = words.size();
		size_t i = 0;

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

			// the folders: each part followed by a \ or / (spaces inside a folder's name are part of it); the last part is the file's name
			size_t part = drive ? i + 3 : i + 2;

			for (;;)
			{
				size_t end = part;

				// a colon is never inside a name, so it ends the part: "x.rif and E:\..." is a file's name and words, then the next path, not one folder
				while (end < n && !PartEnds(words[end]) && !Separator(words[end]) && words[end] != ':')
				{
					end++;
				}

				if (end < n && Separator(words[end]))
				{
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

			out += "<path>\\";
			out.append(words, part, end - part);
			i = end;
		}

		return out;
	}

	std::string Shot_JsonString(std::string const &text, size_t most)
	{
		std::string out = "\"";
		size_t const length = text.size() < most ? text.size() : most;

		for (size_t i = 0; i < length; i++)
		{
			unsigned char const c = (unsigned char)text[i];

			if (c == '"' || c == '\\')
			{
				out += '\\';
				out += (char)c;
			}
			else if (c >= 0x20 && c < 0x7f)
			{
				out += (char)c;
			}
			else
			{
				char escaped[8];

				_snprintf_s(escaped, sizeof(escaped), _TRUNCATE, "\\u%04x", (unsigned)c);
				out += escaped;
			}
		}

		out += "\"";
		return out;
	}

	std::string Shot_NameOnly(std::string const &path)
	{
		size_t const last = path.find_last_of("\\/");

		return last == std::string::npos ? path : path.substr(last + 1);
	}

	std::string Shot_Json(ShotContext const &c)
	{
		std::string out = "{\"format\":1";

		out += ",\"privacy\":\"paths and user names removed\"";
		AddText(&out, "build", c.build, 64);
		AddText(&out, "map", Shot_NameOnly(c.map), kShotMostNameBytes);
		out += ",\"level_bytes\":";
		AddNumber(&out, (long long)c.levelBytes);
		AddText(&out, "level_sha256", c.levelSha256, 64);
		AddText(&out, "screen", c.screen, kShotMostNameBytes);
		out += ",\"pos\":{\"x\":";
		AddNumber(&out, c.pos[0]);
		out += ",\"y\":";
		AddNumber(&out, c.pos[1]);
		out += ",\"z\":";
		AddNumber(&out, c.pos[2]);
		out += "},\"pos_metres\":{\"x\":";
		AddMetres(&out, c.pos[0]);
		out += ",\"y\":";
		AddMetres(&out, c.pos[1]);
		out += ",\"z\":";
		AddMetres(&out, c.pos[2]);
		out += "},\"turn\":";
		AddNumber(&out, c.turn);
		out += ",\"look\":";
		AddNumber(&out, c.look);
		out += ",\"view_distance\":";
		AddNumber(&out, c.viewDistance);
		AddText(&out, "current", c.current, kShotMostNameBytes);
		AddList(&out, "selected", c.selected, kShotMostNames);
		AddList(&out, "visible", c.visible, kShotMostVisible);
		out += ",\"visible_more\":";
		AddNumber(&out, c.visible.size() > kShotMostVisible ? (long long)(c.visible.size() - kShotMostVisible) : 0);
		AddText(&out, "base", c.base, kShotMostNameBytes);
		AddList(&out, "picks", c.picks, kShotMostNames);
		AddList(&out, "branches", c.branches, kShotMostNames);
		AddText(&out, "light", c.light, kShotMostNameBytes);
		AddText(&out, "strategy", c.strategy, kShotMostNameBytes);
		out += ",\"strategy_kind\":";
		AddNumber(&out, c.strategyKind);
		AddText(&out, "hierarchy", c.hierarchy, kShotMostNameBytes);
		AddText(&out, "sequence", c.sequence, kShotMostNameBytes);
		out += ",\"key_frame\":";
		AddNumber(&out, c.keyFrame);
		AddText(&out, "status", c.status, kShotMostLineBytes);

		// the last lines only
		out += ",\"log\":[";

		size_t const first = c.log.size() > kShotMostLogLines ? c.log.size() - kShotMostLogLines : 0;

		for (size_t i = first; i < c.log.size(); i++)
		{
			out += i > first ? "," : "";
			out += Shot_JsonString(Shot_ScrubPaths(c.log[i]), kShotMostLineBytes);
		}

		out += "],\"window\":{\"width\":";
		AddNumber(&out, c.windowWidth);
		out += ",\"height\":";
		AddNumber(&out, c.windowHeight);
		out += "}";
		AddText(&out, "display", c.display, kShotMostNameBytes);
		AddText(&out, "gpu", c.gpu, kShotMostNameBytes);
		out += "}";
		return out;
	}

	std::vector<std::string> Shot_LastLines(std::string const &tail, bool cut, size_t most)
	{
		std::vector<std::string> lines;
		size_t start = 0;

		if (cut)
		{
			size_t const firstEnd = tail.find('\n');

			start = firstEnd == std::string::npos ? tail.size() : firstEnd + 1;
		}

		while (start < tail.size())
		{
			size_t end = tail.find('\n', start);

			end = end == std::string::npos ? tail.size() : end;

			std::string line = tail.substr(start, end - start);

			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}

			if (!line.empty())
			{
				lines.push_back(line);
			}

			start = end + 1;
		}

		if (lines.size() > most)
		{
			lines.erase(lines.begin(), lines.end() - (std::ptrdiff_t)most);
		}

		return lines;
	}

	std::string Shot_FileName(std::string const &map, ShotTime const &when, int number)
	{
		bool const timeGood = when.year >= 1601 && when.year <= 9999 && when.month >= 1 && when.month <= 12 && when.day >= 1 && when.day <= 31 && when.hour >= 0 && when.hour <= 23
			&& when.minute >= 0 && when.minute <= 59 && when.second >= 0 && when.second <= 59;

		if (number < 0 || number >= kShotNumbers || !timeGood)
		{
			return std::string();
		}

		std::string name = Shot_NameOnly(map);
		size_t const dot = name.find_last_of('.');

		if (dot != std::string::npos && dot > 0)
		{
			name.resize(dot);
		}

		char stamp[32];
		char more[8] = "";

		_snprintf_s(stamp, sizeof(stamp), _TRUNCATE, "%04d%02d%02d-%02d%02d%02d", when.year, when.month, when.day, when.hour, when.minute, when.second);

		if (number > 0)
		{
			_snprintf_s(more, sizeof(more), _TRUNCATE, "-%d", number);
		}

		return SafePart(name, "level") + "-" + stamp + more + ".png";
	}
}
