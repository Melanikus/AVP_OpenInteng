// sequence_names.h: the NAMES of a hierarchy's sequences, from the game's own enums (avpx 3dc/avp/sequences.h, HModelSequenceTypes and the sub-sequence enums of each type: ARSS_*, MHSS_*, PHSS_* ...). A sequence of a model
// is numbered (type, sub-sequence); the Hierarchy mode's lists show the number AND the name so a person can tell "10.2" is the marine HUD's Standard Fire. Generated 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng from
// sequences.h; AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <string>

namespace model
{
	// The type's name ("Marine HUD"), or "" when the game has no such type
	char const *SequenceTypeName(int type);

	// The sub-sequence's name ("Standard Fire"), or "" when the game has none with that number for that type (a model may hold numbers the game's enum does not name)
	char const *SequenceSubName(int type, int sub);

	// "Marine HUD: Standard Fire", "Marine HUD: 12" when only the type is known, "7.3" when neither
	std::string SequenceWords(int type, int sub);
}
