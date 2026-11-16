// autolink_scene.h: the level's modules as AutoLink's pass and the adjacency generator want them (autolink_pass.h / autolink_portal.h): each module that has a shape the model reads, with its
// place, extents, points, corners and normals in inteng's memory units (file number x 100 x the level's scale). Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code". (Moved out of autolink_edit.cpp so the adjacency generator reads the same scene.)
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "autolink_pass.h"

class OpenLevelFile;

namespace host
{
	struct AutoLinkScene
	{
		std::set<int32_t> moduleNumbers;                              // every module's object number
		std::map<int32_t, std::unique_ptr<PortalShape> > shapes;      // the storage the modules' shapes point into
		std::vector<AutoLinkModule> list;                             // the modules that have a shape, in the file's order
		std::map<int32_t, size_t> indexOf;                            // object number -> its place in list
		std::map<int32_t, std::string> names;                         // object number -> its name
		std::map<int32_t, std::vector<int32_t> > fileLocations;       // object number -> x, y, z as the file holds them
	};

	// Reads the open level's modules into *scene (emptied first). Changes nothing.
	void AutoLinkScene_Build(OpenLevelFile *open, double levelScale, AutoLinkScene *scene);
}
