// message_numbers.cpp: see message_numbers.h. Written 2026-11-22 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
#include "message_numbers.h"

#include <string.h>

#include "smk_header.h"

namespace model
{
	namespace
	{
		// The table's letter for a number 1 to kMostMessageNumber. A table shorter than that, or a letter it doesn't know, makes the number
		// Refused, so a broken table can never offer a number as free (tests/model_message_numbers_test checks the table is whole).
		MessageNumberKind KindOf(int32_t number)
		{
			if (number < 1 || number > kMostMessageNumber || strlen(kMessageNumberKinds) != static_cast<size_t>(kMostMessageNumber))
			{
				return MessageNumberKind::Refused;
			}

			switch (kMessageNumberKinds[number - 1])
			{
				case 'C': return MessageNumberKind::Clean;
				case 'O': return MessageNumberKind::OldLine;
				case 'F': return MessageNumberKind::StockFilm;
				case 'S': return MessageNumberKind::UsedByStock;
				default: return MessageNumberKind::Refused;
			}
		}

		std::string WordsFor(int32_t number, MessageNumberKind kind)
		{
			std::string const n = std::to_string(number);

			switch (kind)
			{
				case MessageNumberKind::Clean: return n + ": free";
				case MessageNumberKind::OldLine: return n + ": free, prints an old Rebellion line";
				case MessageNumberKind::StockFilm: return n + ": replaces a stock film no level uses";
				case MessageNumberKind::UsedByStock: return n + ": a stock level uses it, so its film plays there too";
				default: return n + ": not a number the game plays (1 to " + std::to_string(kMostMessageNumber) + ")";
			}
		}
	}

	MessageNumberInfo MessageNumbers_Info(int32_t number)
	{
		MessageNumberKind const kind = KindOf(number);
		MessageNumberInfo info;

		info.number = number;
		info.kind = kind;
		info.words = WordsFor(number, kind);
		return info;
	}

	std::string MessageNumbers_StockLevels(int32_t number)
	{
		for (size_t i = 0; i < kMessageNumberLevelCount; i++)
		{
			if (kMessageNumberLevels[i].number == number)
			{
				std::string words = kMessageNumberLevels[i].levels;

				for (size_t at = words.find(';'); at != std::string::npos; at = words.find(';', at + 2))
				{
					words.replace(at, 1, ", ");
				}

				return words;
			}
		}

		return std::string();
	}

	std::vector<MessageNumberInfo> MessageNumbers_FreeList()
	{
		std::vector<MessageNumberInfo> list;

		for (MessageNumberKind const kind : { MessageNumberKind::Clean, MessageNumberKind::OldLine, MessageNumberKind::StockFilm })
		{
			for (int32_t n = 1; n <= kMostMessageNumber; n++)
			{
				if (KindOf(n) == kind)
				{
					list.push_back(MessageNumbers_Info(n));
				}
			}
		}

		return list;
	}

	bool MessageNumbers_HasStockFilm(int32_t number)
	{
		for (size_t i = 0; i < kStockFilmCount; i++)
		{
			if (kStockFilms[i].number == number)
			{
				return true;
			}
		}

		return false;
	}

	bool MessageNumbers_IsStockFilm(int32_t number, uint64_t size, std::string const &sha256)
	{
		for (size_t i = 0; i < kStockFilmCount; i++)
		{
			if (kStockFilms[i].number == number && kStockFilms[i].size == size && kStockFilms[i].sha256 && sha256 == kStockFilms[i].sha256)
			{
				return true;
			}
		}

		return false;
	}
}
