// status_words.h: the status line's words, OWNED by the status line. Setting them copies the text, so the status line never points at a string that is changed, moved or freed
// later. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng, after twice the status line read freed memory: it held a pointer into another string's buffer
// (lightWords, given new words by RelightTheView). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
//   statusWords = text;        copies text (a null pointer: nothing is said); text may be the status line's own words
//   (char const *)statusWords  the words, or null; they stay as they are until the next setting
//   statusWords.From(p)        true when the words were last set from that very pointer: the address is kept only to be compared, never read through (the two places that ask
//                              which buffer the words came from: the bar's cut-off titles, a save's own words)

#pragma once

#include <string>

class StatusWords
{
public:
	StatusWords() = default;
	StatusWords(StatusWords const &) = delete;
	StatusWords &operator=(StatusWords const &) = delete;

	StatusWords &operator=(char const *text)
	{
		source = text;

		if (!text)
		{
			held = false;
			words.clear();
			return *this;
		}

		std::string copy(text);   // copied first: text may point into words itself

		words.swap(copy);
		held = true;
		return *this;
	}

	operator char const *() const
	{
		return held ? words.c_str() : nullptr;
	}

	bool From(char const *pointer) const
	{
		return held && pointer != nullptr && pointer == source;
	}

private:
	std::string words;
	char const *source = nullptr;
	bool held = false;
};
