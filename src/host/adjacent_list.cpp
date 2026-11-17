// See adjacent_list.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "adjacent_list.h"

#include <algorithm>

namespace host
{
	AdjacentList::AdjacentList() : current(-1), changed(false)
	{
	}

	int32_t AdjacentList::Current() const
	{
		return current;
	}

	std::vector<int32_t> const &AdjacentList::Members() const
	{
		return members;
	}

	bool AdjacentList::Changed() const
	{
		return changed;
	}

	void AdjacentList::SetCurrent(int32_t module, std::vector<int32_t> const &entries)
	{
		members.clear();

		if (module < 0)
		{
			current = -1;
			changed = false;
			return;
		}

		// FUN_004045b0: the entries in their order, each one a module (one that names none is not here)
		members = entries;
		current = module;
		changed = false;
	}

	void AdjacentList::MarkWritten()
	{
		changed = false;
	}

	void AdjacentList::Clear()
	{
		current = -1;
		members.clear();
		changed = false;
	}

	AdjacentAdd AdjacentList::Add(int32_t picked)
	{
		if (current < 0)
		{
			return AdjacentAdd::NoCurrent;
		}

		if (picked < 0)
		{
			return AdjacentAdd::NoPick;
		}

		if (picked == current)
		{
			return AdjacentAdd::IsCurrent;
		}

		if (std::find(members.begin(), members.end(), picked) != members.end())
		{
			return AdjacentAdd::AlreadyThere;
		}

		members.push_back(picked);
		changed = true;
		return AdjacentAdd::Added;
	}

	AdjacentDelete AdjacentList::Delete(int32_t picked)
	{
		const std::vector<int32_t>::iterator found = picked < 0 ? members.end() : std::find(members.begin(), members.end(), picked);

		if (found == members.end())
		{
			return AdjacentDelete::NotThere;
		}

		members.erase(found);
		changed = true;
		return AdjacentDelete::Deleted;
	}
}
