// list_order.h: the order inteng's list dialog shows its rows in (FUN_00475bf0 0x476403-0x476670: each row is put in an ascending list by FUN_0053ed50, a byte-by-byte
// comparison, after the ones that are not above it: ascending, and rows that compare equal keep the order they were given in). Written 2026-11-16 by Melanikus (Dietrich) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// SAFETY (hostile input, Vasquez's find through Bishop, 2026-11-16): the rows come from the level (object names and the like), so a level can hold any number of them. inteng's own
// way, one insertion at a time, takes time growing with the square of the count: a level with 100,000 objects froze the Find and Select lists. The same order is made here by a stable
// sort of the rows' places (n log n), never by moving the rows themselves.
#pragma once

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

namespace host
{
	// The places of the rows in the order the dialog shows them: order[k] is the place, in rows, of the k-th row shown
	inline std::vector<size_t> ListOrder(std::vector<std::string> const &rows)
	{
		std::vector<size_t> order(rows.size());

		for (size_t i = 0; i < order.size(); i++)
		{
			order[i] = i;
		}

		std::stable_sort(order.begin(), order.end(), [&rows](size_t a, size_t b) { return rows[a].compare(rows[b]) < 0; });
		return order;
	}
}
