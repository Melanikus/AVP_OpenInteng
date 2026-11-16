// hierarchy_name.h: the name rule of inteng's Hierarchy-mode "Copy Hierarchy" command (0x49, FUN_0047b170). Hierarchy mode, slice 4. Written 2026-11-15 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT IS. Copy Hierarchy (out\dump-all-2.txt "== FUN_0047b170"): with NO hierarchy selected (view +0x20 is 0) nothing happens, no message. Otherwise the prompt "Type
// name of new hierarchy" is offered with the last name typed (DAT_00606824); on OK an EMPTY name does nothing (0x47b1e4); else every OBJCHIER chunk of the file is looked at
// in order, and the first whose name equals the typed one ignoring case (FUN_005909e0, a case-blind compare) stops the command with the message "That name is already in
// use" (0x47b28d); an OBJCHIER with no object (FUN_00482d30 gives 0) is skipped. When no name matches, the selected hierarchy's chunk payload (its bytes after the 12-byte
// heading) is copied as a new OBJCHIER chunk (FUN_00482ab0) and the copy is named (FUN_00483250). The chunk copy is the host's; this file is the name rule only.
#pragma once

#include <string>
#include <vector>

namespace model
{
	enum class HierarchyNameResult
	{
		Ok,        // the copy is made under this name
		Empty,     // an empty name: inteng does nothing, silently
		InUse,     // another hierarchy has the name, ignoring case: "That name is already in use"
	};

	char const *HierarchyNameWords(HierarchyNameResult result);

	// ASCII case-blind equality, as the compare of 0x47b25b does (the exe's own compare is on bytes; letters A-Z match a-z)
	bool SameNameIgnoringCase(std::string const &a, std::string const &b);

	// `existing` is the name of every OBJCHIER that has one, in file order
	HierarchyNameResult CheckCopyName(std::vector<std::string> const &existing, std::string const &typed);
}
