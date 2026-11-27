// The test for src/model's message numbers (message_numbers.h): which numbers a mapper's film can take, and the Save dialog's words about each.
// Written 2026-11-22 by Melanikus (Frost) for AVP_OpenInteng, Video mode (alpha.5). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHAT IT CAN'T CHECK HERE: that the table is what the game's own data makes. That data (language.txt, FMVs\ and the 63 stock levels) is
// Rebellion's, and never on a public machine, so GitHub can't run that check: tools/make-message-numbers.js --check, run from the game's folder, does, by hand,
// on the owner's PC, on the STOCK files (checked against Steam's own depot manifests; a mapper's own edited levels are not the stock game, and a
// player who gets his map has the stock levels). This test checks the table's own consistency (the counts the stock data gave, 2026-11-22: 118
// clean, 45 old-line, 2 stock films no level uses, 144 and 237; nothing past 450) and the rules and words built on it. (The 11-21 lists, 44
// old-line and 103 / 144 / 237, were read from a folder holding the owner's own edited Derelict, where 103 is dropped and 1 is used.) It runs none of the editor and reads no file. The program gives back a non-zero
// result if any check failed. Each check's words say what it proves.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "message_numbers.h"
#include "smk_header.h"

using namespace model;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static int CountOf(char kind)
{
	int count = 0;

	for (char const *k = kMessageNumberKinds; *k; k++)
	{
		count += *k == kind ? 1 : 0;
	}

	return count;
}

int main()
{
	printf("message numbers\n");

	// The table itself: one letter for every number the game plays, and only the four letters
	size_t const length = strlen(kMessageNumberKinds);
	bool lettersKnown = true;

	for (size_t i = 0; i < length; i++)
	{
		lettersKnown = lettersKnown && strchr("SFCO", kMessageNumberKinds[i]) != nullptr;
	}

	Check(length == static_cast<size_t>(kMostMessageNumber), "the table has one kind for every number the game plays, 1 to " + std::to_string(kMostMessageNumber) + " (it has " + std::to_string(length) + ")");
	Check(lettersKnown, "and every kind is S, F, C or O");
	Check(CountOf('C') == 118, "118 numbers are free and clean, as the stock data gave (the table has " + std::to_string(CountOf('C')) + ")");
	Check(CountOf('O') == 45, "45 are free with an old Rebellion line (" + std::to_string(CountOf('O')) + ")");
	Check(CountOf('F') == 2, "2 are stock films no level uses (" + std::to_string(CountOf('F')) + ")");
	Check(CountOf('S') == kMostMessageNumber - 118 - 45 - 2, "and every other number is used by a stock level (" + std::to_string(CountOf('S')) + ")");
	Check(MessageNumbers_Info(103).kind == MessageNumberKind::UsedByStock, "103 is stock Derelict's briefing film, so it is never offered: " + MessageNumbers_Info(103).words);

	// Every number's kind and words, from the table
	bool allMatch = true;
	int32_t firstUsed = 0;

	for (int32_t n = 1; n <= kMostMessageNumber; n++)
	{
		MessageNumberInfo const info = MessageNumbers_Info(n);
		char const k = kMessageNumberKinds[n - 1];
		MessageNumberKind const want = k == 'C' ? MessageNumberKind::Clean : k == 'O' ? MessageNumberKind::OldLine : k == 'F' ? MessageNumberKind::StockFilm : MessageNumberKind::UsedByStock;
		allMatch = allMatch && info.number == n && info.kind == want;

		if (want == MessageNumberKind::UsedByStock && firstUsed == 0)
		{
			firstUsed = n;
		}
	}

	Check(allMatch, "MessageNumbers_Info gives each number 1 to 450 the table's kind");
	Check(firstUsed > 0 && MessageNumbers_Info(firstUsed).words == std::to_string(firstUsed) + ": a stock level uses it, so its film plays there too", "a number a stock level uses is said so, in words (" + std::to_string(firstUsed) + ")");

	// Numbers the game doesn't play are refused, saying why
	for (int32_t const n : { 0, -1, kMostMessageNumber + 1, 1000, 1001, 2147483647, -2147483647 - 1 })
	{
		MessageNumberInfo const info = MessageNumbers_Info(n);
		Check(info.kind == MessageNumberKind::Refused && info.words == std::to_string(n) + ": not a number the game plays (1 to 450)", "the number " + std::to_string(n) + " is refused: " + info.words);
	}

	// The free list: the clean ones ascending, then the old-line ones ascending, then the stock films ascending, and nothing else
	std::vector<MessageNumberInfo> const free = MessageNumbers_FreeList();
	bool ordered = true;
	bool noneUsed = true;
	bool wordsRight = true;
	int stage = 0;
	int32_t last = 0;

	for (MessageNumberInfo const &info : free)
	{
		int const now = info.kind == MessageNumberKind::Clean ? 0 : info.kind == MessageNumberKind::OldLine ? 1 : info.kind == MessageNumberKind::StockFilm ? 2 : 3;
		noneUsed = noneUsed && now < 3;
		ordered = ordered && (now > stage || (now == stage && info.number > last));
		stage = now;
		last = info.number;
		std::string const n = std::to_string(info.number);
		std::string const want = now == 0 ? n + ": free" : now == 1 ? n + ": free, prints an old Rebellion line" : n + ": replaces a stock film no level uses";
		wordsRight = wordsRight && info.words == want;
	}

	Check(free.size() == 118 + 45 + 2, "the free list has the 165 numbers a mapper can take (" + std::to_string(free.size()) + ")");
	Check(noneUsed, "and no number a stock level uses");
	Check(ordered, "clean first, then old-line, then the stock films, each ascending");
	Check(wordsRight, "each with the Save dialog's words, letter for letter");
	Check(!free.empty() && free.front().words == std::to_string(free.front().number) + ": free" && free.front().kind == MessageNumberKind::Clean, "the first one offered is a clean number: " + (free.empty() ? std::string("(none)") : free.front().words));
	Check(MessageNumbers_Info(417).words == "417: free", "417, the README's example, is free: " + MessageNumbers_Info(417).words);

	// THE STOCK LEVELS THAT PLAY EACH S NUMBER (alpha.6, for Save's question): one entry per S number and no other, ascending, each naming a level file
	puts("The stock levels that play each S number:");
	bool onePerS = kMessageNumberLevelCount == static_cast<size_t>(CountOf('S'));
	bool ascending = true;
	bool onlyS = true;
	bool named = true;

	for (size_t i = 0; i < kMessageNumberLevelCount; i++)
	{
		MessageNumberLevels const &one = kMessageNumberLevels[i];

		ascending = ascending && (i == 0 || kMessageNumberLevels[i - 1].number < one.number);
		onlyS = onlyS && one.number >= 1 && one.number <= kMostMessageNumber && kMessageNumberKinds[one.number - 1] == 'S';
		// the files as Steam ships them, some ".rif" and some ".RIF" (odobenus.rif, Nost03_M.rif): looked for case-blind
		std::string lower = one.levels ? one.levels : "";

		for (char &c : lower)
		{
			c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
		}

		named = named && lower.size() > 4 && lower.find(".rif") != std::string::npos;
	}

	Check(onePerS, "as many entries as S numbers (" + std::to_string(kMessageNumberLevelCount) + " of " + std::to_string(CountOf('S')) + ")");
	Check(ascending && onlyS, "ascending, and each an S number");
	Check(named, "each names at least one level file");
	Check(MessageNumbers_StockLevels(103) == "Derelict.RIF", "103 is stock Derelict's briefing: " + MessageNumbers_StockLevels(103));
	Check(MessageNumbers_StockLevels(417).empty() && MessageNumbers_StockLevels(144).empty(), "a free number and a stock film no level plays name no level");

	// THE STOCK FILMS' BYTES (Vasquez 2026-11-27, Export map's film gathering, design-vasquez-export-films.md): each stock FMVs\message<N>.smk's size and
	// SHA-256, as the tool read them from a folder checked against Steam's depot manifests (53 films: 51 a stock level plays, and 144 and 237). Here: the
	// list's own consistency, and the rule built on it (a film is Rebellion's own only when its number, size and SHA-256 are all one row's)
	bool filmsAscending = kStockFilmCount > 0, filmsKinds = true, filmsHashes = true, everyF = true;

	for (size_t i = 0; i < kStockFilmCount; i++)
	{
		StockFilm const &film = kStockFilms[i];
		bool hex = film.sha256 && strlen(film.sha256) == 64;

		for (size_t c = 0; hex && c < 64; c++)
		{
			hex = (film.sha256[c] >= '0' && film.sha256[c] <= '9') || (film.sha256[c] >= 'a' && film.sha256[c] <= 'f');
		}

		filmsHashes = filmsHashes && hex && film.size > 0;
		filmsAscending = filmsAscending && film.number >= 1 && film.number <= kMostMessageNumber && (i == 0 || kStockFilms[i - 1].number < film.number);
		filmsKinds = filmsKinds && film.number >= 1 && film.number <= kMostMessageNumber &&
		             (kMessageNumberKinds[film.number - 1] == 'S' || kMessageNumberKinds[film.number - 1] == 'F');
	}

	for (int32_t n = 1; n <= kMostMessageNumber; n++)
	{
		everyF = everyF && (kMessageNumberKinds[n - 1] != 'F' || MessageNumbers_HasStockFilm(n));
	}

	Check(kStockFilmCount == 53, "the stock game ships 53 message films (the table has " + std::to_string(kStockFilmCount) + ")");
	Check(filmsAscending, "the films are listed by number, ascending, each 1 to 450, each once");
	Check(filmsKinds, "each film's number is an S or an F number (a stock film is never a free number)");
	Check(everyF, "every F number has its film in the list");
	Check(filmsHashes, "each has a size and a SHA-256 of 64 small hex letters");

	if (kStockFilmCount > 0)
	{
		StockFilm const &first = kStockFilms[0];
		std::string const hash = first.sha256;
		std::string other = hash;

		other[0] = other[0] == '0' ? '1' : '0';
		Check(MessageNumbers_HasStockFilm(first.number) && MessageNumbers_IsStockFilm(first.number, first.size, hash),
		      "a film with its number's size and SHA-256 is Rebellion's own (" + std::to_string(first.number) + ")");
		Check(!MessageNumbers_IsStockFilm(first.number, first.size, other) && !MessageNumbers_IsStockFilm(first.number, first.size + 1, hash),
		      "the same number with another SHA-256 or another size is not");
		Check(!MessageNumbers_IsStockFilm(417, first.size, hash) && !MessageNumbers_HasStockFilm(417) && !MessageNumbers_HasStockFilm(0) &&
		      !MessageNumbers_HasStockFilm(451), "nor is a number with no stock film (417, a free one; 0 and 451, which the game doesn't play)");
	}

	printf(checksFailed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return checksFailed ? 1 : 0;
}
