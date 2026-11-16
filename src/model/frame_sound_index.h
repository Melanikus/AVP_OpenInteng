// frame_sound_index.h: the index rules of inteng's Hierarchy-mode "New sound" command (0x4d, FUN_00481440). Hierarchy mode, slice 3. Written 2026-11-15 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT IS. The hierarchy's sounds are kept in a list, each with an INDEX (the number a frame names to play it); New sound (out\dump-all-2.txt "== FUN_00481440"):
// the prompt "Select sound index" is first offered the SMALLEST index from 1 up that no sound has (0x481461-0x48147a); the typed number must be 1 to 127 ("Index must be
// between 1 and 127", 0x4814b9) and not used by any sound ("That index is already being used", 0x4814cd); the new sound is added at the END of the list with its first flag
// bit cleared (+0x14 &= ~1) and the level is marked changed (DAT_00637134 = 1). The sound's own data (which wav) is chosen by Edit sound 0x4e (FUN_004816f0 -> FUN_004ebd60),
// UNREAD. Delete 0x4f (FUN_00481620: a chooser "Delete which sound?", the sound is destroyed (FUN_004ebd20) and leaves the list) and Apply / Clear frame sound 0x50 / 0x51
// (FUN_00479de0 / FUN_00479e90: put the chosen sound, or none, on every frame whose range holds the current frame, FUN_004075c0) are READ in outline only; their frame
// structure (a record of 0x50 bytes with the range at +0x44 / +0x48) is not modelled here.
#pragma once

#include <stdint.h>

#include <vector>

namespace model
{
	int32_t const kLeastSoundIndex = 1;
	int32_t const kMostSoundIndex = 127;

	enum class SoundIndexResult
	{
		Ok,
		OutOfRange,   // not 1 to 127
		InUse,        // another sound has it
	};

	char const *SoundIndexWords(SoundIndexResult result);

	// The index the prompt offers: the smallest from 1 up that is not in `used`
	int32_t FirstFreeSoundIndex(std::vector<int32_t> const &used);

	// Whether a typed index is taken, in inteng's order: the range first, then whether it is used
	SoundIndexResult CheckNewSoundIndex(std::vector<int32_t> const &used, int32_t typed);
}
