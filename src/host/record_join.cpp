// See record_join.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "record_join.h"

#include <stddef.h>

#include <exception>
#include <utility>

namespace host
{
	bool JoinRecord(FieldRecord *record, FieldRecord *more, size_t mostFields)
	{
		if (!record || !more || record == more)
		{
			return false;
		}

		// A record whose list and values don't agree is a mistake in a builder: it is never joined
		if (record->list.size() != record->values.size() || more->list.size() != more->values.size())
		{
			return false;
		}

		if (record->list.size() + more->list.size() > mostFields)
		{
			return false;
		}

		size_t const listBefore = record->list.size();
		size_t const sourcesBefore = record->sources.size();

		try
		{
			// The sources of both are as long as their lists first, so a field of *more is at the same place in both
			while (record->sources.size() < record->list.size())
			{
				record->sources.push_back(FieldSource());
			}

			// *more is moved in first: its words stay where they are, and the descriptors copied below point to them
			record->parts.push_back(std::move(*more));
			FieldRecord &kept = record->parts.back();

			*more = FieldRecord();

			while (kept.sources.size() < kept.list.size())
			{
				kept.sources.push_back(FieldSource());
			}

			record->list.insert(record->list.end(), kept.list.begin(), kept.list.end());
			record->values.insert(record->values.end(), kept.values.begin(), kept.values.end());
			record->sources.insert(record->sources.end(), kept.sources.begin(), kept.sources.begin() + (std::ptrdiff_t)kept.list.size());
			return true;
		}
		catch (std::exception const &)
		{
			// Out of memory: what was added is taken back, so the record is as it was (the moved-in record is lost, which is all that is lost)
			record->list.resize(listBefore);
			record->values.resize(listBefore);
			record->sources.resize(sourcesBefore);
			return false;
		}
	}
}
