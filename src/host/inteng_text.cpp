// See inteng_text.h. Written 2026-11-16 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "inteng_text.h"

namespace host
{
	std::vector<IntengTextPiece> IntengText_Layout(std::string const &text, int32_t x, int32_t y, int32_t screenWidth, int32_t screenHeight, IntengLetterWidth widthOf, int32_t letterHeight,
		size_t mostPieces)
	{
		std::vector<IntengTextPiece> pieces;

		if (!widthOf || letterHeight < 1 || screenWidth < 1 || screenHeight < 1 || mostPieces == 0)
		{
			return pieces;
		}

		// in 64 bits, so no place overflows however long the text
		int64_t const firstX = x;
		int64_t const width = screenWidth;
		int64_t const height = screenHeight;
		int64_t const step = letterHeight;
		int64_t const tab = 4 * step;
		int64_t atX = firstX;
		int64_t atY = y;
		bool pieceOpen = false;

		auto const widthOfLetter = [&](unsigned char letter) -> int64_t
		{
			if (letter >= 128)
			{
				return 0;
			}

			int const w = widthOf((int)letter);
			return w < 0 ? 0 : w;
		};

		// a letter drawn at (atX, atY): onto the open piece, or a new one when the last jump moved the place
		auto const put = [&](unsigned char letter) -> bool
		{
			if (letter >= 128)
			{
				return true;
			}

			if (!pieceOpen)
			{
				if (pieces.size() >= mostPieces)
				{
					return false;
				}

				IntengTextPiece piece;

				piece.x = (int32_t)(atX > INT32_MAX ? INT32_MAX : (atX < INT32_MIN ? INT32_MIN : atX));
				piece.y = (int32_t)(atY > INT32_MAX ? INT32_MAX : (atY < INT32_MIN ? INT32_MIN : atY));
				pieces.push_back(piece);
				pieceOpen = true;
			}

			pieces.back().text += (char)letter;
			return true;
		};

		size_t at = 0;

		while (at < text.size())
		{
			// the word: its letters up to the next new line, tab, space or the end (0x57bebb-0x57bee8)
			size_t stop = at;
			int64_t wordWidth = 0;

			while (stop < text.size() && text[stop] != '\n' && text[stop] != '\t' && text[stop] != ' ')
			{
				wordWidth += widthOfLetter((unsigned char)text[stop]);
				stop++;
			}

			// a word that would reach the right edge, and is narrower than half the screen, starts a new line (0x57bef0-0x57bf21)
			if (width <= atX + wordWidth && wordWidth < width / 2)
			{
				atY += step;
				atX = firstX;
				pieceOpen = false;
			}

			// a line whose bottom would pass the screen's bottom: the text ends (0x57bf2d)
			if (height < step + atY)
			{
				return pieces;
			}

			// the word's letters, and the stop after it (drawn as a letter too: 0x57c169 breaks only after the stop)
			size_t const last = stop < text.size() ? stop : text.size() - 1;

			for (size_t i = at; i <= last; i++)
			{
				unsigned char const letter = (unsigned char)text[i];

				if (letter == '\n')
				{
					atY += step;
					atX = firstX;
					pieceOpen = false;
				}
				else if (letter == '\t')
				{
					atX = (atX / tab + 1) * tab;   // 0x57bf7d: from the screen's left (x / (4 h) + 1) x 4 h
					pieceOpen = false;
				}
				else
				{
					int64_t const letterWidth = widthOfLetter(letter);

					if (width <= atX + letterWidth)
					{
						// past the right edge: the next line (0x57bfc0); a letter that would not fit there either ends the text (0x57bfa0 -> 0x57c1e9)
						atY += step;
						atX = firstX;
						pieceOpen = false;

						if (width <= atX + letterWidth || height < step + atY)
						{
							return pieces;
						}
					}

					if (!put(letter))
					{
						return pieces;
					}

					atX += letterWidth;
				}
			}

			// a line below the bottom: the text ends (0x57c18a-0x57c1a8)
			if (height < step + atY)
			{
				return pieces;
			}

			at = last + 1;
		}

		return pieces;
	}
}
