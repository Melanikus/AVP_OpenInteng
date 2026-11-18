// orbit_command.cpp: the harness's camera-orbit and camera-corner in the editor (orbit_command.h says what and why). Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "orbit_command.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "camera_orbit.h"
#include "editor_shared.h"   // theCamera
#include "level_objects.h"   // LevelObjects_Read
#include "level_view.h"      // FlyCamera, LevelView_ObjectBox, LevelView_ModuleBox
#include "move_rel_poly_edit.h"   // ObjectPlace_Get
#include "shape_edit.h"      // ShapeEdit_Read
#include "shape_model.h"
#include "level_file.h"
#include "object_list.h"     // host::LevelObject
#include "pick_game.h"       // PickGame_AtClick
#include "selection.h"

// construction_modes_screen.cpp: Light Sourcing's markers (the set's lights, numbered from 0)
bool LightMarkerBox(size_t light, int32_t mins[3], int32_t maxs[3]);
size_t LightMarkerUnderPointer(host::PickCamera const &camera, int32_t x, int32_t y, double *distance);

namespace
{
	size_t const kMostText = 256;
	size_t const kMostName = 64;
	double const kCornerSpots[4] = { 45.0, 135.0, 225.0, 315.0 };

	struct Subject
	{
		std::string name;
		std::string what;          // "the object" or "the module"
		bool isObject = false;
		uint8_t id[host::kSelectionIdBytes] = {};
		int32_t module = -1;
		int32_t light = -1;        // "light <n>": the set's light n - 1 (its marker is the subject)
		int32_t mins[3] = {}, maxs[3] = {};
	};

	// camera-corner's trial, one spot a frame: the spot is put, a frame is drawn from it, then the click in the middle of the view is asked who it picks
	struct CornerTrial
	{
		bool pending = false;
		Subject subject;
		int spot = 0;
		int frames = 0;
	} corner;

	bool Printable(std::string const &text)
	{
		for (char const c : text)
		{
			if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7e)
			{
				return false;
			}
		}

		return true;
	}

	// a plain decimal: an optional -, 1 to 7 digits, then optionally a . and 1 to 3 digits; within [least, most]
	bool Number(std::string const &word, double least, double most, double *out)
	{
		size_t k = word.size() > 0 && word[0] == '-' ? 1 : 0;
		size_t digits = 0, fraction = 0;

		while (k < word.size() && word[k] >= '0' && word[k] <= '9' && digits < 8)
		{
			k++, digits++;
		}

		if (k < word.size() && word[k] == '.')
		{
			k++;

			while (k < word.size() && word[k] >= '0' && word[k] <= '9' && fraction < 4)
			{
				k++, fraction++;
			}

			if (fraction == 0)
			{
				return false;
			}
		}

		if (digits == 0 || digits > 7 || fraction > 3 || k != word.size())
		{
			return false;
		}

		double const value = atof(word.c_str());

		if (!(value >= least && value <= most))
		{
			return false;
		}

		*out = value;
		return true;
	}

	// the subject by its name: a placed object in play, else a module; its box
	bool Find(std::string const &name, Subject *out)
	{
		std::vector<host::LevelObject> objects;

		out->name = name;

		if (LevelObjects_Read(&objects))
		{
			for (host::LevelObject const &one : objects)
			{
				if (!one.name || name != one.name)
				{
					continue;
				}

				out->what = "the object";
				out->isObject = true;
				memcpy(out->id, one.id, host::kSelectionIdBytes);

				if (LevelView_ObjectBox(one.id, out->mins, out->maxs))
				{
					return true;   // in play: the game's own block's box
				}

				// not in play (far from the camera): its place in the file and its shape's extents, times the level's scale (the turn left out: a shape's extents are round its own middle
				// near enough for the camera to look at it, and the trial's pick says whether it does); the camera put there brings it into play
				OpenLevelFile *const open = LevelObjects_OpenForEdit();
				model::ShapeModel shape;
				std::string why;
				int32_t place[3];
				double const scale = local_scale > 0.0 ? local_scale : 1.0;

				if (open && open->Level() && host::ObjectPlace_Get(open, open->Level(), one.id, place) == host::MoveRelPolyEditResult::Done &&
					host::ShapeEdit_Read(open->Level(), open, one.id, &shape, nullptr, &why) == host::ShapeEditResult::Done)
				{
					int32_t const low[3] = { shape.extents.minX, shape.extents.minY, shape.extents.minZ };
					int32_t const high[3] = { shape.extents.maxX, shape.extents.maxY, shape.extents.maxZ };
					bool fits = true;

					for (int k = 0; k < 3; k++)
					{
						double const a = (place[k] + (double)low[k]) * scale, b = (place[k] + (double)high[k]) * scale;

						fits = fits && fabs(a) < 16777216.0 && fabs(b) < 16777216.0 && a <= b;
						out->mins[k] = fits ? (int32_t)floor(a) : 0;
						out->maxs[k] = fits ? (int32_t)ceil(b) : 0;
					}

					if (fits)
					{
						out->what = "the object (its box from the file: not in play when asked)";
						return true;
					}
				}

				return false;
			}
		}

		int32_t const modules = LevelView_ModuleCount();

		for (int32_t m = 0; m < modules; m++)
		{
			std::string moduleName;
			int32_t world[3];

			if (LevelView_ModuleSite(m, &moduleName, world) && moduleName == name && LevelView_ModuleBox(m, out->mins, out->maxs))
			{
				out->what = "the module";
				out->module = m;
				return true;
			}
		}

		// "light <n>" (Dietrich 2026-11-18, Bishop: an ordinary light has no name): the set's light n, counted from 1 as face-light and Light Sourcing's words count; its marker's box
		// (LightMarkerBox). Looked for after the objects and modules, so a name that meant one before still means it.
		if (name.size() > 6 && name.size() <= 12 && name.compare(0, 6, "light ") == 0 && name.find_first_not_of("0123456789", 6) == std::string::npos && name[6] != '0')
		{
			long const number = atol(name.c_str() + 6);

			if (number >= 1 && number <= 1000000 && LightMarkerBox((size_t)(number - 1), out->mins, out->maxs))
			{
				out->what = "the light's marker";
				out->light = (int32_t)(number - 1);
				return true;
			}
		}

		return false;
	}

	// camera-corner's test for a light: the click in the middle meets its marker and nothing nearer on the game's line (Light Sourcing's own rule, PickTheLight in light_screen.cpp)
	bool MiddlePicksTheLight(int32_t light, host::PickReport const &picked, int32_t viewWidth, int32_t viewHeight)
	{
		host::PickCamera camera;
		double distance = 0.0;

		if (!PickGame_Camera(viewWidth, viewHeight, &camera) || LightMarkerUnderPointer(camera, viewWidth / 2, viewHeight / 2, &distance) != (size_t)light)
		{
			return false;
		}

		return !((picked.found == host::PickFound::Module || picked.found == host::PickFound::Object) && (double)picked.distance < distance);
	}

	void Put(host::OrbitCamera const &camera)
	{
		theCamera->x = camera.place[0];
		theCamera->y = camera.place[1];
		theCamera->z = camera.place[2];
		theCamera->turn = camera.turn;
		theCamera->look = camera.look;
		theCamera->roll = 0;
		FlyCamera_Hold(theCamera);
	}

	// the camera line the harness waits for, and the words
	void Said(char const *verb, Subject const &subject, double around, double up, double used, std::string const &spot, std::string *words)
	{
		char line[160];

		_snprintf_s(line, sizeof(line), _TRUNCATE, "%d %d %d %d look %d roll %d", theCamera->x, theCamera->y, theCamera->z, theCamera->turn, theCamera->look, theCamera->roll);
		*words = std::string(verb) + " " + subject.name + ": " + subject.what + "'s box, " + std::to_string((int)floor(around + 0.5)) + " round, " + std::to_string((int)floor(up + 0.5)) + " up, " +
			std::to_string((int)floor(used + 0.5)) + " from its centre" + spot + ": camera " + line + " (the box " + std::to_string(subject.mins[0]) + ".." + std::to_string(subject.maxs[0]) + " " +
			std::to_string(subject.mins[1]) + ".." + std::to_string(subject.maxs[1]) + " " + std::to_string(subject.mins[2]) + ".." + std::to_string(subject.maxs[2]) + ")";
		printf("AVP_OpenInteng: camera: %s\n", line);
		fflush(stdout);
	}
}

bool OrbitCommand_Run(std::string const &text, std::string *words)
{
	words->clear();

	if (text.empty() || text.size() > kMostText || !Printable(text))
	{
		*words = "camera-orbit: the words are not 1 to 256 printable letters";
		return false;
	}

	std::vector<std::string> fields;
	size_t from = 0;

	while (fields.size() < 6)
	{
		size_t const bar = text.find('|', from);

		fields.push_back(text.substr(from, bar == std::string::npos ? std::string::npos : bar - from));

		if (bar == std::string::npos)
		{
			break;
		}

		from = bar + 1;
	}

	bool const isCorner = !fields.empty() && fields[0] == "corner";
	bool const isOrbit = !fields.empty() && fields[0] == "orbit";

	if ((!isCorner && !isOrbit) || fields.size() < 2 || fields.size() > (isCorner ? 2u : 5u) || fields[1].empty() || fields[1].size() > kMostName)
	{
		*words = "camera-orbit: orbit|<name>[|<around>[|<up>[|<distance>]]] or corner|<name>, the name 1 to 64 letters";
		return false;
	}

	double around = host::kOrbitDefaultAround, up = host::kOrbitDefaultUp, distance = 0.0;

	if ((fields.size() > 2 && !Number(fields[2], -3600.0, 3600.0, &around)) || (fields.size() > 3 && !Number(fields[3], -89.0, 89.0, &up)) ||
		(fields.size() > 4 && !Number(fields[4], 0.0, host::kOrbitMostDistance, &distance)))
	{
		*words = "camera-orbit: around -3600 to 3600, up -89 to 89, distance 0 to 1000000 (millimetres), each a plain decimal";
		return false;
	}

	Subject subject;

	if (!theCamera)
	{
		*words = "camera-orbit: no level is in view";
		return false;
	}

	if (!Find(fields[1], &subject))
	{
		*words = "camera-orbit: no object or module named " + fields[1] + " is in play, nor is it a light of the level (light <n>, counted from 1)";
		return false;
	}

	host::OrbitCamera camera = {};
	double used = 0.0;

	if (!host::CameraOrbit_Place(subject.mins, subject.maxs, isCorner ? kCornerSpots[0] : around, isCorner ? host::kOrbitDefaultUp : up, isCorner ? 0.0 : distance, &camera, &used))
	{
		*words = "camera-orbit: " + subject.name + "'s box gives no place for the camera inside its limits";
		return false;
	}

	Put(camera);

	if (isCorner)
	{
		// the trial goes on in the frames (OrbitCommand_Frame), which say the camera line when it ends
		corner.pending = true;
		corner.subject = subject;
		corner.spot = 0;
		corner.frames = 0;
		*words = "camera-corner " + subject.name + ": trying the 4 upper diagonal spots";
		return true;
	}

	Said("camera-orbit", subject, around, up, used, std::string(), words);
	return true;
}

void OrbitCommand_Frame(int32_t viewWidth, int32_t viewHeight, std::string *words)
{
	words->clear();

	if (!corner.pending || !theCamera)
	{
		return;
	}

	// two frames after the spot was put: the last one drawn is from it
	if (++corner.frames < 2)
	{
		return;
	}

	host::PickReport const picked = PickGame_AtClick(viewWidth / 2, viewHeight / 2, viewWidth, viewHeight);
	Subject const &subject = corner.subject;
	bool const itsSubject = subject.light >= 0 ? MiddlePicksTheLight(subject.light, picked, viewWidth, viewHeight)
		: subject.isObject ? (picked.found == host::PickFound::Object && memcmp(picked.objectId, subject.id, host::kSelectionIdBytes) == 0)
		: (picked.found == host::PickFound::Module && picked.moduleIndex == subject.module);
	host::OrbitCamera camera = {};
	double used = 0.0;

	if (itsSubject)
	{
		host::CameraOrbit_Place(subject.mins, subject.maxs, kCornerSpots[corner.spot], host::kOrbitDefaultUp, 0.0, &camera, &used);
		corner.pending = false;
		Said("camera-corner", subject, kCornerSpots[corner.spot], host::kOrbitDefaultUp, used, ", spot " + std::to_string(corner.spot + 1) + " of 4 (" + std::to_string((int)kCornerSpots[corner.spot]) +
			" round): the click in the middle of the view picks it", words);
		return;
	}

	// the next spot that gives a place; none left: the first, said
	while (++corner.spot < 4)
	{
		if (host::CameraOrbit_Place(subject.mins, subject.maxs, kCornerSpots[corner.spot], host::kOrbitDefaultUp, 0.0, &camera, &used))
		{
			Put(camera);
			corner.frames = 0;
			return;
		}
	}

	corner.pending = false;
	host::CameraOrbit_Place(subject.mins, subject.maxs, kCornerSpots[0], host::kOrbitDefaultUp, 0.0, &camera, &used);
	Put(camera);
	Said("camera-corner", subject, kCornerSpots[0], host::kOrbitDefaultUp, used, ", no spot of the 4: the click in the middle picks it from none (the first spot is kept)", words);
}
