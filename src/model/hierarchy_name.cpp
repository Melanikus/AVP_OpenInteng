// See hierarchy_name.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "hierarchy_name.h"

namespace model
{
	char const *HierarchyNameWords(HierarchyNameResult result)
	{
		switch (result)
		{
		case HierarchyNameResult::Ok: return "done";
		case HierarchyNameResult::Empty: return "no name typed";
		case HierarchyNameResult::InUse: return "That name is already in use";
		}

		return "unknown answer";
	}

	namespace
	{
		char Lower(char c)
		{
			return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
		}
	}

	bool SameNameIgnoringCase(std::string const &a, std::string const &b)
	{
		if (a.size() != b.size())
		{
			return false;
		}

		for (size_t i = 0; i < a.size(); i++)
		{
			if (Lower(a[i]) != Lower(b[i]))
			{
				return false;
			}
		}

		return true;
	}

	HierarchyNameResult CheckCopyName(std::vector<std::string> const &existing, std::string const &typed)
	{
		if (typed.empty())
		{
			return HierarchyNameResult::Empty;
		}

		for (std::string const &name : existing)
		{
			if (SameNameIgnoringCase(name, typed))
			{
				return HierarchyNameResult::InUse;
			}
		}

		return HierarchyNameResult::Ok;
	}
}
