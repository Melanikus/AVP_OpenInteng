// status_words_test.cpp: the status line's words are a copy the status line owns (src/host/status_words.h). Twice the status line read freed memory because it held a pointer
// into another string's buffer; these checks set it from strings that are then given new words, moved, cleared and freed, and read it back. Written 2026-11-18 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <string.h>

#include <memory>
#include <string>
#include <utility>

#include "status_words.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

static bool Says(StatusWords const &status, char const *words)
{
	char const *const now = status;

	return now != nullptr && strcmp(now, words) == 0;
}

int main()
{
	printf("THE STATUS LINE'S WORDS\n");

	StatusWords status;

	Check((char const *)status == nullptr, "at the start nothing is said (null)");

	// the case that bit (RelightTheView): the status set from a string, the string then given longer words (a new buffer, the old one freed)
	std::string lightWords = "Picked: light 123, its marker under the pointer";

	status = lightWords.c_str();
	lightWords = "view re-lit: 12 modules, and enough more words that the string needs a buffer of its own";
	Check(Says(status, "Picked: light 123, its marker under the pointer"), "set from a string that then gets new, longer words: the status keeps its own words");
	Check((char const *)status != lightWords.c_str(), "and they are not the string's buffer");

	// a string moved away, cleared, and one that is freed
	std::string moved = "Light 123 copied (V pastes it at the camera)";

	status = moved.c_str();

	std::string const taken = std::move(moved);

	moved.clear();
	Check(Says(status, "Light 123 copied (V pastes it at the camera)"), "set from a string that is then moved away and cleared: the status keeps its own words");

	{
		std::unique_ptr<std::string> gone(new std::string("Doors closed: the doors are drawn shut and Save Intensities lights them so"));

		status = gone->c_str();
	}

	Check(Says(status, "Doors closed: the doors are drawn shut and Save Intensities lights them so"), "set from a string that is then freed: the status keeps its own words");

	// set from its own words (light_screen.cpp's keep, "statusWords = statusWords")
	status = (char const *)status;
	Check(Says(status, "Doors closed: the doors are drawn shut and Save Intensities lights them so"), "set from its own words: the same words");

	// a buffer written in place after it was set: the status keeps the words it was given
	char buffer[64] = "Camera speed: 6";

	status = buffer;
	strcpy_s(buffer, sizeof(buffer), "Camera speed: 9");
	Check(Says(status, "Camera speed: 6"), "a buffer written in place later: the status keeps the words it was given until it is set again");

	// From: which pointer the words were last set from (the bar's cut-off titles, a save's own words), compared, never read through
	char cutOff[16] = "Mode, Options";

	status = cutOff;
	Check(status.From(cutOff), "From says the words were set from that buffer");
	Check(!status.From(buffer) && !status.From(nullptr), "and from no other, nor from null");
	cutOff[0] = '\0';
	Check(status.From(cutOff) && Says(status, "Mode, Options"), "the buffer emptied later: still From it, and the words as they were set");

	// nothing said again
	status = nullptr;
	Check((char const *)status == nullptr && !status.From(cutOff), "set to null: nothing is said, and From is false");
	status = "";
	Check(Says(status, ""), "set to empty words: an empty line, not null");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
