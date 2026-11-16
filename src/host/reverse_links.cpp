// See reverse_links.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "reverse_links.h"

#include <exception>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "level_file.h"
#include "level_chunks.h"
#include "module_links.h"
#include "object_base.h"
#include "object_edit.h"

namespace host
{
	namespace
	{
		// One touched module: its tree as it is now, and the number of links the level held for it
		struct Touched
		{
			std::unique_ptr<LinkTree> tree;
			size_t linksBefore;
			size_t linksNow;
		};

		size_t NodesIn(LinkTree const &tree)
		{
			return tree.Count();
		}
	}

	char const *ReverseLinksWords(ReverseLinksResult result)
	{
		switch (result)
		{
		case ReverseLinksResult::Done: return "done";
		case ReverseLinksResult::NothingOpen: return "no level is open";
		case ReverseLinksResult::WriteFailed: return "a module's links could not be written";
		}

		return "the reverse links could not be added";
	}

	ReverseLinksResult AddReverseLinks(OpenLevelFile *open, std::vector<int32_t> const *sources, ReverseLinksCounts *counts, std::string *why)
	{
		ReverseLinksCounts none;
		ReverseLinksCounts &total = counts ? *counts : none;

		total.walked = total.touched = total.changed = total.linksAdded = total.written = total.skipped = 0;

		if (!open || !open->Level())
		{
			return ReverseLinksResult::NothingOpen;
		}

		try
		{
			std::set<int32_t> modules;

			ModuleIndexes(open->Level(), &modules);

			std::map<int32_t, Touched> touched;
			std::vector<int32_t> order;   // the modules in the order they were first touched

			// Makes the module current: its tree is loaded from the level the first time (FUN_004c10c0 loads it whenever the module is made current, and what
			// it wrote when it left gives the same tree), and is marked changed (0x4c14df)
			auto makeCurrent = [&](int32_t module) -> LinkTree *
			{
				std::map<int32_t, Touched>::iterator found = touched.find(module);

				if (found == touched.end())
				{
					Touched entry;
					std::vector<LinkItem> items;

					entry.tree.reset(new LinkTree(module));

					if (ReadModuleLinks(open->Level(), module, &items))
					{
						entry.tree->Load(items.data(), (int)items.size(), [&modules](int32_t index) { return modules.count(index) != 0; });
					}

					entry.linksBefore = NodesIn(*entry.tree);
					entry.linksNow = entry.linksBefore;
					found = touched.insert(std::make_pair(module, std::move(entry))).first;
					order.push_back(module);
				}

				found->second.tree->MarkChanged();
				return found->second.tree.get();
			};

			std::vector<int32_t> walkList;

			if (sources)
			{
				walkList = *sources;
			}
			else
			{
				walkList.assign(modules.begin(), modules.end());
			}

			for (int32_t const source : walkList)
			{
				if (modules.count(source) == 0)
				{
					continue;
				}

				total.walked++;

				std::vector<LinkTree::ReverseVisit> visits;

				makeCurrent(source)->ReverseVisits(&visits);

				for (LinkTree::ReverseVisit const &visit : visits)
				{
					if (modules.count(visit.module) == 0)
					{
						continue;
					}

					makeCurrent(visit.module)->AddReversePath(visit.path);
				}
			}

			total.touched = order.size();

			// the writes (FUN_004c10c0's write when a module is left): every touched module with links in ONE new image and ONE loader read, then the
			// module flag of each (in when it has links, out when it has none), a same-size edit
			std::vector<ModuleLinksFor> edits;
			std::vector<int32_t> editModules;
			std::vector<uint8_t> noLinkIds;   // 8 bytes each: the modules that have no link
			std::vector<std::pair<int32_t, bool>> flags;

			for (int32_t const module : order)
			{
				Touched &entry = touched[module];
				ModuleLinksFor edit;

				entry.linksNow = NodesIn(*entry.tree);

				if (entry.linksNow != entry.linksBefore)
				{
					total.changed++;
					total.linksAdded += entry.linksNow > entry.linksBefore ? entry.linksNow - entry.linksBefore : 0;
				}

				if (!ModuleIdOfIndex(open->Level(), module, edit.id))
				{
					continue;
				}

				if (entry.tree->HasLinks())
				{
					for (LinkItem const &item : entry.tree->Flatten())
					{
						ModuleLinkItem const one = { item.objectIndex, item.branchNo, item.flags, item.spare };

						edit.items.push_back(one);
					}

					edits.push_back(edit);
					editModules.push_back(module);
				}

				flags.push_back(std::make_pair(module, entry.tree->HasLinks()));
			}

			std::vector<bool> wrote;
			ObjectEditResult const result = SetModuleLinksMany(open, edits, &wrote);

			if (result != ObjectEditResult::Done)
			{
				if (why)
				{
					*why = ObjectEditWords(result);
				}

				return ReverseLinksResult::WriteFailed;
			}

			for (size_t at = 0; at < wrote.size(); at++)
			{
				total.written += wrote[at] ? 1 : 0;
				total.skipped += wrote[at] ? 0 : 1;
			}

			for (std::pair<int32_t, bool> const &one : flags)
			{
				uint8_t id[8];

				if (ModuleIdOfIndex(open->Level(), one.first, id))
				{
					SetModuleObjectFlag(open, id, one.second);
				}
			}

			return ReverseLinksResult::Done;
		}
		catch (std::exception const &)
		{
			if (why)
			{
				*why = "out of memory";
			}

			return ReverseLinksResult::WriteFailed;
		}
	}
}
