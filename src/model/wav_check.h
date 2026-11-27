// wav_check.h: is a WAV file one the game plays? The sound import's check (Load sound... and a WAV dropped on Object Placement), made BEFORE anything is
// written. Written 2026-11-27 by Melanikus (Dietrich) for AVP_OpenInteng (AvP-docs director/design-dietrich-sound-import-export-map.md, section 4).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Pure: no Windows, no files. The caller reads the file.
//
// THE FILE IS HOSTILE: a mapper's WAV can come from anywhere, and once a map ships it, every player's game reads it. So the import refuses exactly what the game
// refuses, IN THE SAME ORDER AND THE SAME WORDS, and then what Steam AvP's own reader can't play too:
// - THE GAME'S RULES, 1 to 21: Drake's hardened reader, one list agreed with him for both (AvP game PR 2, drake-wavguard 813164b9: avpx/3dc/avp/win95/
//   wav_chunks.h WavHeader_Read and WavFormat_Refusal; the game logs "sound: refused the WAV <name>: <reason>"). The first rule that fails gives the reason.
//   The words are his, letter for letter; where the file's own number helps a mapper, it is added AFTER them in brackets, so the rule's words stay the same.
//   DEBT: copied here because the editor's avpx pin (d5d44b97) is from before that PR; once the pin holds it, this calls WavFormat_Refusal instead.
//   The chunks between the format and the sound are stepped over by their length ALONE, as the game's FindWavDataChunk steps and Rebellion's own reader
//   does (Psndplat.c:1643-1655): neither steps the pad byte a WAV puts after a chunk of an odd length. So a file whose odd chunk has its pad is refused here as
//   both games refuse it (rule 9: they find no data chunk), and one without the pad is taken as both take it. Stepping any other way would make the import
//   disagree with both. (5 stock sounds have chunks there, all even: fact, 4 bytes; smpl, 60.)
// - THEN STEAM AvP'S (Rebellion's own reader, Psndplat.c LoadWavFromFastFile, as the Assembla copy branches/reb holds it), the import's alone: a WARNING, not
//   a refusal, for more than 250,000 bytes of sound: Rebellion's SOUND_MAXSIZE (psndplat.h:231; Psndplat.c:1666-1690) refuses bigger. It is read from
//   Rebellion's source, not yet from Steam's own exe, so the file still loads (AvPx plays it). Every stock sound is under it (the biggest: 158,427 bytes,
//   Nost03's jcountdown.wav).
// Nothing past the bytes given is ever read: every step is held to them, in 64-bit sums.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace model
{
	uint32_t const kWavMostDataBytes = 64u * 1024u * 1024u;   // the game's limit (openal_audio.cpp kMaxWavDataBytes): over it, refused (rule 11)
	uint32_t const kWavSteamMostDataBytes = 250000;           // Rebellion's SOUND_MAXSIZE (psndplat.h:231): over it, Steam AvP plays nothing (a warning)
	uint32_t const kWavSlowestRate = 100;                     // the sound system's own limits, FREQUENCY_MINPLAT and FREQUENCY_MAXPLAT (psndplat.h:241-242),
	uint32_t const kWavFastestRate = 100000;                  // DirectSound's DSBFREQUENCY_MIN and _MAX: Rebellion's reader takes the same (rule 17)

	// What the check found in a WAV it takes
	struct WavFacts
	{
		uint16_t channels = 0;       // 1 or 2
		uint16_t bits = 0;           // 8 or 16
		uint32_t rate = 0;           // samples a second
		uint32_t dataBytes = 0;      // the sound's own bytes
		bool overSteamLimit = false; // dataBytes over kWavSteamMostDataBytes, so Steam AvP won't play it (a warning)
	};

	// True when the bytes are a WAV both games play (above), with what it holds in *facts. False with *why: the first rule that fails, in the game's words
	// ("its samples are neither 8 nor 16 bits"), with the file's own number after them when it has one ("(this one: 24 bits)")
	bool WavCheck(uint8_t const *bytes, size_t size, WavFacts *facts, std::string *why);
}
