// message_numbers.h: which message numbers a mapper's film can take, and the words the Save dialog says about each. Written 2026-11-22 by
// Melanikus (Frost) for AVP_OpenInteng, Video mode (alpha.5; Dietrich's interface, Bishop 2's rules). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code". Pure: no Windows, no files.
//
// A Message strategy's number N is both a film, FMVs\message<N>.smk, and a text line (the game's string N - 1 + TEXTSTRING_LEVELMSG_001), and the
// game plays only 1 to 450 (kMostMessageNumber, src/model/smk_header.h; AvPx strategy_chunk.cpp MostLevelMessages). What each number is, in the
// stock game, is read from the game's own data by tools/make-message-numbers.js into message_numbers_table.cpp (numbers and kinds only: no
// Rebellion text is in our source), never typed in.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace model
{
	enum class MessageNumberKind
	{
		Clean,         // no stock level uses it, no stock film, and its line is Rebellion's placeholder or empty: the best to take
		OldLine,       // no stock level uses it, no stock film, but the console prints an old Rebellion line when it plays
		StockFilm,     // a stock film no stock level plays: replacing it breaks nothing in the stock game
		UsedByStock,   // a stock level plays it: a film with this number plays there too
		Refused        // not a number the game plays (below 1 or past 450)
	};

	struct MessageNumberInfo
	{
		int32_t number;
		MessageNumberKind kind;
		std::string words;     // the Save dialog's row for it, letter for letter
	};

	// The kind of every number 1 to kMostMessageNumber, in order, one letter each (S used by a stock level, F a stock film no level uses,
	// C clean, O old line), written by tools/make-message-numbers.js (message_numbers_table.cpp)
	extern char const kMessageNumberKinds[];

	// THE STOCK LEVELS THAT PLAY EACH S NUMBER (alpha.6: Video mode's Save asks, naming the level), written by the same tool: the level files as the
	// game names them, ';' between, ascending by number
	struct MessageNumberLevels
	{
		int32_t number;
		char const *levels;
	};

	extern MessageNumberLevels const kMessageNumberLevels[];
	extern size_t const kMessageNumberLevelCount;

	// The stock level files that play a number, ', ' between, for the Save question ("Derelict.RIF"); "" for a number no stock level plays
	std::string MessageNumbers_StockLevels(int32_t number);

	MessageNumberInfo MessageNumbers_Info(int32_t number);

	// Every number a mapper can take, best first: the clean ones ascending, then the old-line ones ascending, then the stock films no level uses
	// ascending. A number a stock level uses is never in it (MessageNumbers_Info still says what it is).
	std::vector<MessageNumberInfo> MessageNumbers_FreeList();
}
