// See harness_select.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (checkpoint 1's select step). AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".

#include "harness_select.h"

#include <string.h>

#include "selection.h"

namespace host
{
	static_assert(kObjectIdBytes == kSelectionIdBytes, "the list and the selection know an object by the same 8 bytes");

	HarnessSelectAnswer HarnessSelect_Row(bool nobodyWatching, ObjectList *list, uint64_t row, bool add)
	{
		if (!nobodyWatching)
		{
			return HarnessSelectAnswer::Ignored;
		}

		// The row comes from outside: it's checked against the list's own count before anything is read
		if (!list || row >= (uint64_t)list->Count())
		{
			return HarnessSelectAnswer::Refused;
		}

		const uint8_t *const id = list->Id((size_t)row);

		if (!id)
		{
			return HarnessSelectAnswer::Refused;
		}

		Selected picked = {};

		picked.kind = SelectedKind::Object;
		memcpy(picked.objectId, id, kSelectionIdBytes);

		// The selection first: it refuses an ID that's all 0, and then nothing has changed, the list's chosen row included
		if (!(add ? Selection_Toggle(picked) : Selection_Set(picked)))
		{
			return HarnessSelectAnswer::Refused;
		}

		// The list holds at most kObjectListMost rows (2^20), so the row fits the list's own number
		list->Choose((int32_t)row);
		return HarnessSelectAnswer::Selected;
	}

	std::vector<std::string> HarnessSelect_PanelLines(const PaintText *lines, size_t count)
	{
		std::vector<std::string> out;
		size_t const shown = lines ? count : 0;

		for (size_t i = 0; i < shown; i++)
		{
			out.push_back("AVP_OpenInteng: panel " + std::to_string(i) + ": " + PanelPaint_SafeText(lines[i].text));
		}

		out.push_back("AVP_OpenInteng: panel lines " + std::to_string(shown));
		return out;
	}
}
