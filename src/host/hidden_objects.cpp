// hidden_objects.cpp: see hidden_objects.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "hidden_objects.h"

#include <string.h>

#include <array>
#include <iterator>
#include <set>

namespace host
{
	namespace
	{
		typedef std::array<uint8_t, kHiddenIdBytes> Id;

		std::set<Id> &Hidden()
		{
			static std::set<Id> hidden;
			return hidden;
		}

		unsigned version = 0;
		bool haveLast = false;
		Id last = {};

		Id MakeId(uint8_t const *bytes)
		{
			Id id;
			memcpy(id.data(), bytes, kHiddenIdBytes);
			return id;
		}

	}

	size_t HiddenObjects_Hide(uint8_t const *ids, size_t count)
	{
		if (!ids || count == 0)
		{
			return 0;
		}

		size_t added = 0;

		for (size_t i = 0; i < count; i++)
		{
			Id const id = MakeId(ids + i * kHiddenIdBytes);

			if (Hidden().size() < kHiddenObjectsMost && Hidden().insert(id).second)
			{
				added++;
			}
		}

		version++;

		// the last of the list is "the last hidden", even when it was hidden already
		haveLast = true;
		last = MakeId(ids + (count - 1) * kHiddenIdBytes);
		return added;
	}

	bool HiddenObjects_UnhideLast()
	{
		if (!haveLast)
		{
			return false;
		}

		Hidden().erase(last);
		version++;

		return true;
	}

	void HiddenObjects_ShowAll(bool forgetLast)
	{
		Hidden().clear();
		version++;

		if (forgetLast)
		{
			haveLast = false;
		}
	}

	void HiddenObjects_ForgetLast()
	{
		haveLast = false;
	}

	bool HiddenObjects_Is(uint8_t const *id)
	{
		return id && Hidden().count(MakeId(id)) != 0;
	}

	size_t HiddenObjects_Count()
	{
		return Hidden().size();
	}

	bool HiddenObjects_At(size_t index, uint8_t *id)
	{
		if (!id || index >= Hidden().size())
		{
			return false;
		}

		std::set<Id>::const_iterator at = Hidden().begin();

		std::advance(at, (std::ptrdiff_t)index);
		memcpy(id, at->data(), kHiddenIdBytes);
		return true;
	}

	bool HiddenObjects_Last(uint8_t *id)
	{
		if (!haveLast || !id)
		{
			return false;
		}

		memcpy(id, last.data(), kHiddenIdBytes);
		return true;
	}

	unsigned HiddenObjects_Version()
	{
		return version;
	}

	void HiddenObjects_Reset()
	{
		Hidden().clear();
		version++;
		haveLast = false;
	}
}
