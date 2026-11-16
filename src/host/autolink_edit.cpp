// See autolink_edit.h (and autolink_mlf.h for AutoLink_FromMlf). Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "autolink_edit.h"
#include "autolink_mlf.h"

#include <string.h>

#include <exception>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "level_store.h"
#include "shape_edit.h"
#include "autolink_pass.h"
#include "autolink_scene.h"
#include "module_links.h"
#include "object_base.h"
#include "object_edit.h"

namespace host
{
	namespace
	{
		// The gap between two modules' boxes on one axis (negative when they overlap), in memory units
		int64_t GapOnAxis(PortalModule const &a, PortalModule const &b, int axis)
		{
			int64_t const aHigh = (int64_t)a.place[axis] + a.extents[2 * axis];
			int64_t const aLow = (int64_t)a.place[axis] + a.extents[2 * axis + 1];
			int64_t const bHigh = (int64_t)b.place[axis] + b.extents[2 * axis];
			int64_t const bLow = (int64_t)b.place[axis] + b.extents[2 * axis + 1];
			int64_t const one = aLow - bHigh;
			int64_t const two = bLow - aHigh;

			return one > two ? one : two;
		}

		// FUN_004c7ab0's distance test: kept when no axis's gap is beyond the visibility distance (0 keeps all)
		bool WithinDistance(PortalModule const &a, PortalModule const &b, int32_t distance)
		{
			if (distance <= 0)
			{
				return true;
			}

			for (int axis = 0; axis < 3; axis++)
			{
				if (GapOnAxis(a, b, axis) > (int64_t)distance)
				{
					return false;
				}
			}

			return true;
		}

		// FUN_004c10c0: the module is made current; with "delete old" its old links are gone, else they are loaded. The root's "changed" is put on.
		std::unique_ptr<LinkTree> MakeCurrent(OpenLevelFile *open, AutoLinkScene const &scene, int32_t module, bool deleteOld)
		{
			std::unique_ptr<LinkTree> tree(new LinkTree(module));
			std::vector<LinkItem> old;

			if (!deleteOld && ReadModuleLinks(open->Level(), module, &old))
			{
				tree->Load(old.data(), (int)old.size(), [&scene](int32_t index) { return scene.moduleNumbers.count(index) != 0; });
			}

			tree->MarkChanged();
			return tree;
		}

		// What the pass saw from a module goes into its tree, kept within the visible distance (FUN_004c7ab0's cut, then FUN_004c4230's Normal Link). Gives the links added.
		size_t PutSeenInTree(AutoLinkScene const &scene, size_t from, std::set<int32_t> const &seen, int32_t distance, LinkTree *tree, size_t *cut)
		{
			size_t const before = tree->Count();

			for (int32_t const other : seen)
			{
				std::map<int32_t, size_t>::const_iterator at = scene.indexOf.find(other);

				if (at == scene.indexOf.end() || other == tree->RootModule())
				{
					continue;
				}

				if (!WithinDistance(scene.list[from].module, scene.list[at->second].module, distance))
				{
					(*cut)++;
					continue;
				}

				tree->Normal(other);
			}

			return tree->Count() > before ? tree->Count() - before : 0;
		}

		// The write (FUN_004c10c0's, when a module is left): every module of `trees` that has a link, in ONE new image and ONE loader read, then the module flag of each that went in.
		// NAMED: a module with no link at the end is left as it is, also with "delete old" (inteng would drop its old links and clear its flag; a missing link can make a module vanish in
		// game, so an old link is not thrown away for nothing).
		AutoLinkResult WriteTrees(OpenLevelFile *open, std::map<int32_t, std::unique_ptr<LinkTree> > const &trees, size_t *written, size_t *skipped, std::string *why)
		{
			std::vector<ModuleLinksFor> edits;

			for (std::map<int32_t, std::unique_ptr<LinkTree> >::const_iterator it = trees.begin(); it != trees.end(); ++it)
			{
				if (!it->second->HasLinks())
				{
					continue;
				}

				ModuleLinksFor edit;

				if (!ModuleIdOfIndex(open->Level(), it->first, edit.id))
				{
					(*skipped)++;
					continue;
				}

				for (LinkItem const &item : it->second->Flatten())
				{
					ModuleLinkItem const one = { item.objectIndex, item.branchNo, item.flags, item.spare };

					edit.items.push_back(one);
				}

				edits.push_back(edit);
			}

			if (edits.empty())
			{
				return AutoLinkResult::Done;
			}

			std::vector<bool> wrote;
			ObjectEditResult const result = SetModuleLinksMany(open, edits, &wrote);

			if (result != ObjectEditResult::Done)
			{
				if (why)
				{
					*why = ObjectEditWords(result);
				}

				return AutoLinkResult::WriteFailed;
			}

			for (size_t at = 0; at < wrote.size(); at++)
			{
				*written += wrote[at] ? 1 : 0;
				*skipped += wrote[at] ? 0 : 1;
			}

			for (size_t at = 0; at < edits.size(); at++)
			{
				if (at < wrote.size() && wrote[at])
				{
					SetModuleObjectFlag(open, edits[at].id, true);
				}
			}

			return AutoLinkResult::Done;
		}

		bool SameNameNoCase(std::string const &a, std::string const &b)
		{
			return a.size() == b.size() && _strnicmp(a.c_str(), b.c_str(), a.size()) == 0;
		}
	}

	char const *AutoLinkWords(AutoLinkResult result)
	{
		switch (result)
		{
		case AutoLinkResult::Done: return "done";
		case AutoLinkResult::NothingOpen: return "no level is open";
		case AutoLinkResult::WriteFailed: return "a module's links could not be written";
		}

		return "the links could not be made";
	}

	AutoLinkResult AutoLink_Run(OpenLevelFile *open, std::vector<int32_t> const *sources, bool deleteOld, double levelScale, int32_t visibleDistance, AutoLinkCounts *counts, std::string *why)
	{
		AutoLinkCounts none;
		AutoLinkCounts &total = counts ? *counts : none;

		total.sources = total.portals = total.views = total.linksAdded = total.cut = total.written = total.skipped = 0;

		if (!open || !open->Level())
		{
			return AutoLinkResult::NothingOpen;
		}

		try
		{
			AutoLinkScene scene;

			AutoLinkScene_Build(open, levelScale > 0.0 ? levelScale : 1.0, &scene);

			AutoLinkPass pass;

			pass.Build(scene.list);

			std::vector<int32_t> runList;

			if (sources)
			{
				runList = *sources;
			}
			else
			{
				for (AutoLinkModule const &entry : scene.list)
				{
					runList.push_back(entry.number);
				}
			}

			std::set<int32_t> done;
			std::map<int32_t, std::unique_ptr<LinkTree> > trees;

			for (int32_t const source : runList)
			{
				std::map<int32_t, size_t>::const_iterator found = scene.indexOf.find(source);

				if (found == scene.indexOf.end() || !done.insert(source).second)
				{
					continue;
				}

				total.sources++;

				std::set<int32_t> seen;
				size_t views = 0, portals = 0;

				pass.SeenFrom(found->second, &seen, &views, &portals);
				total.views += views;
				total.portals += portals;

				std::unique_ptr<LinkTree> tree = MakeCurrent(open, scene, source, deleteOld);

				total.linksAdded += PutSeenInTree(scene, found->second, seen, visibleDistance, tree.get(), &total.cut);
				trees[source] = std::move(tree);
			}

			return WriteTrees(open, trees, &total.written, &total.skipped, why);
		}
		catch (std::exception const &)
		{
			if (why)
			{
				*why = "out of memory";
			}

			return AutoLinkResult::WriteFailed;
		}
	}

	AutoLinkResult AutoLink_FromMlf(OpenLevelFile *open, std::vector<MlfRecord> const &records, double levelScale, int32_t visibleDistance, MlfCounts *counts, std::string *why)
	{
		MlfCounts none;
		MlfCounts &total = counts ? *counts : none;

		total.records = records.size();
		total.used = total.unknown = total.linksAdded = total.cut = total.written = total.skipped = 0;

		if (!open || !open->Level())
		{
			return AutoLinkResult::NothingOpen;
		}

		try
		{
			double const scale = levelScale > 0.0 ? levelScale : 1.0;
			AutoLinkScene scene;

			AutoLinkScene_Build(open, scale, &scene);

			AutoLinkPass pass;

			pass.Build(scene.list);

			std::map<int32_t, std::unique_ptr<LinkTree> > trees;

			for (MlfRecord const &record : records)
			{
				// the module of that name whose place is within 1 unit of the record's (the level's file numbers or memory units, see autolink_mlf.h)
				int32_t module = -1;

				for (std::map<int32_t, std::string>::const_iterator it = scene.names.begin(); it != scene.names.end() && module < 0; ++it)
				{
					if (!SameNameNoCase(it->second, record.name))
					{
						continue;
					}

					PortalModule const &place = scene.list[scene.indexOf[it->first]].module;
					std::vector<int32_t> const &file = scene.fileLocations[it->first];
					bool inMemory = true, inFile = true;

					for (int axis = 0; axis < 3; axis++)
					{
						int64_t const a = (int64_t)record.place[axis] - place.place[axis];
						int64_t const b = (int64_t)record.place[axis] - file[axis];

						inMemory = inMemory && a >= -1 && a <= 1;
						inFile = inFile && b >= -1 && b <= 1;
					}

					if (inMemory || inFile)
					{
						module = it->first;
					}
				}

				if (module < 0)
				{
					total.unknown++;
					continue;
				}

				// the view: the place times 100 (0x4c8d2f-0x4c8dd8) and the matrix; a place that would not fit is not a view
				int32_t viewPlace[3];
				bool fits = true;

				for (int axis = 0; axis < 3; axis++)
				{
					int64_t const value = (int64_t)record.view[axis] * 100;

					fits = fits && value >= INT32_MIN && value <= INT32_MAX;
					viewPlace[axis] = (int32_t)value;
				}

				if (!fits)
				{
					total.unknown++;
					continue;
				}

				std::set<int32_t> seen;

				pass.SeenByCamera(viewPlace, record.turn, &seen);
				total.used++;

				std::map<int32_t, std::unique_ptr<LinkTree> >::iterator held = trees.find(module);

				if (held == trees.end())
				{
					held = trees.insert(std::make_pair(module, MakeCurrent(open, scene, module, false))).first;
				}

				total.linksAdded += PutSeenInTree(scene, scene.indexOf[module], seen, visibleDistance, held->second.get(), &total.cut);
			}

			return WriteTrees(open, trees, &total.written, &total.skipped, why);
		}
		catch (std::exception const &)
		{
			if (why)
			{
				*why = "out of memory";
			}

			return AutoLinkResult::WriteFailed;
		}
	}
}
