// See strategy_edit.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's
// own code".

#include "strategy_edit.h"

#include <exception>

namespace host
{
	EditAnswer ApplyStrategyEdit(File_Chunk *level, uint8_t const *id, int32_t kind, FieldEdit const &edit, PayloadStore *store, char const **why,
	                             StrategyFound *lookup)
	{
		if (why)
		{
			*why = 0;
		}

		if (lookup)
		{
			*lookup = StrategyFound::BadCall;
		}

		if (!id)
		{
			return EditAnswer::BadCall;
		}

		ObjectStrategy found;
		StrategyFound const answer = FindObjectStrategy(level, id, &found);

		if (lookup)
		{
			*lookup = answer;
		}

		if (answer != StrategyFound::Found)
		{
			return EditAnswer::NoPayload;
		}

		return ApplyStrategyPayloadEdit(store, found.chunk, kind, edit, why);
	}

	StrategyFound LookUpStrategyBytes(File_Chunk *level, PayloadStore const &store, uint8_t const *id, ObjectStrategy *out)
	{
		StrategyFound const answer = FindObjectStrategy(level, id, out);

		if (answer != StrategyFound::Found)
		{
			return answer;
		}

		// Running out of memory refuses the answer, with nothing in *out
		try
		{
			std::vector<unsigned char> held;

			if (!store.PayloadOf(out->chunk, &held))
			{
				out->chunk = 0;
				out->payload.clear();
				return StrategyFound::NoStrategyBytes;
			}

			out->payload.swap(held);
			return StrategyFound::Found;
		}
		catch (std::exception const &)
		{
			out->chunk = 0;
			out->payload.clear();
			return StrategyFound::NoMemory;
		}
	}
}
