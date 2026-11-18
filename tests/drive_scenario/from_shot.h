// from_shot.h: the harness's "from-shot <png>" (Bishop 11-18, the owner: "the point of my printscreen with location is ... also to help you testing", and "make inteng take a map and coordinates
// so you can open it for me in a specific place"): a screenshot of the editor (#78) becomes a scenario's starting point by the EDITOR'S OWN -shot (src/host/open_at.h): the editor opens the
// shot's map from its data folder (only when the file's sha256 is the shot's), with the shot's camera, screen and selected object. The harness only finds the picture (a * in its name: the
// newest file that matches) and hands it over; the editor reads it, as hostile input (shot_read.h), and refuses in words.
// "drive_scenario.exe --shot-to-scenario <png>" prints the line and what it stands for: the -at form, the same numbers (shot_read.cpp, the editor's own reader, taken in whole here).
// Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#pragma once

#include <stdio.h>

#include <string>
#include <vector>

#include <windows.h>

#include "png_meta.h"
#include "shot_read.h"
#include "png_meta.cpp"    // the harness is one source file (build-harness.ps1 names one object), as drive_editor.cpp is taken in
#include "shot_read.cpp"

namespace fromshot
{
	// The newest plain file whose name matches the pattern (a * in the file's part), or the plain file itself; empty with *why when there is none
	inline std::string ShotFile(std::string const &pattern, std::string *why)
	{
		if (pattern.empty() || pattern.size() >= MAX_PATH)
		{
			*why = "no shot named, or its path is too long";
			return std::string();
		}

		size_t const slash = pattern.find_last_of("\\/");
		std::string const folder = slash == std::string::npos ? std::string() : pattern.substr(0, slash + 1);
		WIN32_FIND_DATAA found = {};
		HANDLE const search = FindFirstFileA(pattern.c_str(), &found);
		std::string best;
		FILETIME bestTime = {};

		if (search == INVALID_HANDLE_VALUE)
		{
			*why = "no file matches " + pattern;
			return std::string();
		}

		do
		{
			if (found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
			{
				continue;
			}

			if (best.empty() || CompareFileTime(&found.ftLastWriteTime, &bestTime) > 0)
			{
				best = folder + found.cFileName;
				bestTime = found.ftLastWriteTime;
			}
		} while (FindNextFileA(search, &found));

		FindClose(search);

		if (best.empty())
		{
			*why = "no plain file matches " + pattern;
		}

		return best;
	}

	// The scenario lines a shot stands for: the -at form of its place (the editor's -shot does the same and checks the map's sha256 too)
	inline bool Lines(std::string const &pattern, std::string *png, std::vector<std::string> *lines, std::string *why)
	{
		host::ShotPlace place;

		*png = ShotFile(pattern, why);

		if (png->empty() || !host::ShotRead_File(png->c_str(), &place, why))
		{
			return false;
		}

		std::string const name = place.map.size() > 4 && _stricmp(place.map.c_str() + place.map.size() - 4, ".rif") == 0 ? place.map.substr(0, place.map.size() - 4) : place.map;

		lines->clear();
		lines->push_back("from-shot " + *png);
		lines->push_back("# which stands for (without the sha256 check): start-args " + name + " -at " + std::to_string(place.pos[0]) + " " + std::to_string(place.pos[1]) + " " + std::to_string(place.pos[2]) +
			" " + std::to_string(place.turn) + " " + std::to_string(place.look) + " \"" + place.screen + "\"");
		lines->push_back(place.selected.empty() ? "# nothing selected" : "# and the object " + place.selected + " selected");
		lines->push_back("# the level " + place.map + " of the editor's data folder, sha256 " + place.sha256);
		return true;
	}
}
