// module_links_clean.cpp: FUN_004c90c0, the links that name no module taken out at Module Linking's start (module_links_clean.h says what inteng does).
// Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "module_links_clean.h"

#include <set>

#include "level_chunks.h"
#include "level_file.h"
#include "object_edit.h"

namespace host
{
	bool ModuleLinks_DropDangling(std::vector<LinkItem> *items, std::function<bool(int32_t)> const &isModule, size_t *dropped)
	{
		if (dropped)
		{
			*dropped = 0;
		}

		if (!items || !isModule)
		{
			return false;
		}

		std::vector<LinkItem> &a = *items;
		size_t const count = a.size();
		size_t kept = 0;
		size_t gone = 0;
		bool changed = false;

		for (size_t i = 0; i < count; i++)
		{
			// 0x4c91f3-0x4c923d: the object the item names, and whether it is one of the modules
			if (isModule(a[i].objectIndex))
			{
				if (kept != i)
				{
					a[kept] = a[i];   // 0x4c92c8-0x4c92df: FUN_00430320 packs it down
				}

				kept++;
				continue;
			}

			// 0x4c9244-0x4c925e: dropped; every branch_no of the array above the number kept so far is lowered by one
			gone++;
			changed = true;

			for (size_t j = 0; j < count; j++)
			{
				if (a[j].branchNo > (int32_t)kept)
				{
					a[j].branchNo--;
				}
			}
		}

		// 0x4c928e-0x4c92a2: each kept item's branch_no held to the number kept
		for (size_t j = 0; j < kept; j++)
		{
			if (a[j].branchNo > (int32_t)kept)
			{
				a[j].branchNo = (int32_t)kept;
				changed = true;
			}
		}

		a.resize(kept);   // 0x4c92ab-0x4c92f5: the count, or none kept: the VMDARRAY goes

		if (dropped)
		{
			*dropped = gone;
		}

		return changed;
	}

	bool ModuleLinks_CleanLevel(OpenLevelFile *open, ModuleLinksCleaned *done, std::string *why)
	{
		ModuleLinksCleaned ignored;
		std::string ignoredWhy;

		done = done ? done : &ignored;
		why = why ? why : &ignoredWhy;
		*done = ModuleLinksCleaned();

		if (!open || !open->Level())
		{
			*why = "no level is open";
			return false;
		}

		// FUN_004be4f0 less the placed objects (+0x2c & 0x400, 0x4c9141): the modules
		std::set<int32_t> modules;

		ModuleIndexes(open->Level(), &modules);

		auto const isModule = [&modules](int32_t index) { return modules.count(index) != 0; };
		std::vector<ModuleLinksFor> edits;
		std::vector<int32_t> emptied;

		for (int32_t const module : modules)
		{
			std::vector<LinkItem> items;
			size_t dropped = 0;

			if (!ReadModuleLinks(open->Level(), module, &items) || !ModuleLinks_DropDangling(&items, isModule, &dropped))
			{
				continue;
			}

			done->modules++;
			done->itemsDropped += dropped;

			if (items.empty())
			{
				emptied.push_back(module);
				continue;
			}

			ModuleLinksFor edit;

			if (!ModuleIdOfIndex(open->Level(), module, edit.id))
			{
				continue;
			}

			for (LinkItem const &item : items)
			{
				ModuleLinkItem const one = { item.objectIndex, item.branchNo, item.flags, item.spare };

				edit.items.push_back(one);
			}

			edits.push_back(edit);
		}

		if (!edits.empty())
		{
			ObjectEditResult const result = SetModuleLinksMany(open, edits, nullptr);

			if (result != ObjectEditResult::Done)
			{
				*why = ObjectEditWords(result);
				return false;
			}
		}

		for (int32_t const module : emptied)
		{
			bool had = false;
			ObjectEditResult const result = DropModuleLinks(open, module, &had);

			if (result != ObjectEditResult::Done)
			{
				*why = ObjectEditWords(result);
				return false;
			}

			done->emptied += had ? 1 : 0;
		}

		return true;
	}
}
