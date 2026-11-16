// See frame_sound_index.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "frame_sound_index.h"

namespace model
{
	char const *SoundIndexWords(SoundIndexResult result)
	{
		switch (result)
		{
		case SoundIndexResult::Ok: return "done";
		case SoundIndexResult::OutOfRange: return "Index must be between 1 and 127";
		case SoundIndexResult::InUse: return "That index is already being used";
		}

		return "unknown answer";
	}

	namespace
	{
		bool Used(std::vector<int32_t> const &used, int32_t index)
		{
			for (int32_t u : used)
			{
				if (u == index)
				{
					return true;
				}
			}

			return false;
		}
	}

	int32_t FirstFreeSoundIndex(std::vector<int32_t> const &used)
	{
		int32_t index = kLeastSoundIndex;

		// on a match the index goes up by one and the list is looked through again (0x48146a-0x48147a)
		while (Used(used, index))
		{
			index++;
		}

		return index;
	}

	SoundIndexResult CheckNewSoundIndex(std::vector<int32_t> const &used, int32_t typed)
	{
		if (typed < kLeastSoundIndex || typed > kMostSoundIndex)
		{
			return SoundIndexResult::OutOfRange;
		}

		return Used(used, typed) ? SoundIndexResult::InUse : SoundIndexResult::Ok;
	}
}
